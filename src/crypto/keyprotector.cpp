#include "keyprotector.h"
#include "openssl_raii.h"
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <sys/stat.h>   // chmod

// ── Singleton ─────────────────────────────────────────────────────────────────

KeyProtector& KeyProtector::instance() {
    static KeyProtector self;
    return self;
}

// ── Инициализация: загрузка или генерация мастер-ключа ───────────────────────

bool KeyProtector::init() {
    const QString dir  = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString path = dir + "/master.key";

    QFile f(path);

    if (f.exists()) {
        // Загружаем существующий мастер-ключ
        if (!f.open(QIODevice::ReadOnly)) {
            qCritical("[KeyProtector] Не удалось открыть master.key: %s",
                      qPrintable(f.errorString()));
            return false;
        }
        const QByteArray data = f.readAll();
        f.close();

        if (data.size() != 32) {
            qCritical("[KeyProtector] master.key повреждён (размер %d ≠ 32)", data.size());
            return false;
        }
        m_masterKey = data;
        qDebug("[KeyProtector] Мастер-ключ загружен");
        return true;
    }

    // Первый запуск — генерируем 32 случайных байта
    m_masterKey.resize(32);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(m_masterKey.data()), 32) != 1) {
        qCritical("[KeyProtector] RAND_bytes провалился при генерации мастер-ключа");
        m_masterKey.clear();
        return false;
    }

    // Сохраняем с правами 0600 (только владелец может читать/писать)
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCritical("[KeyProtector] Не удалось создать master.key: %s",
                  qPrintable(f.errorString()));
        m_masterKey.clear();
        return false;
    }
    f.write(m_masterKey);
    f.close();

    // Устанавливаем права 0600 — никто кроме владельца не может читать
    ::chmod(qPrintable(path), S_IRUSR | S_IWUSR);

    qDebug("[KeyProtector] Новый мастер-ключ сгенерирован и сохранён");
    return true;
}

// ── HKDF-деривация дочернего ключа ────────────────────────────────────────────

QByteArray KeyProtector::deriveKey(const QByteArray& label, int bytes) const {
    if (m_masterKey.isEmpty()) {
        qCritical("[KeyProtector] deriveKey вызван до init()");
        return {};
    }

    // Нулевой соль — доменное разделение обеспечивается параметром label
    static const QByteArray kSalt(32, '\0');

    EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr));
    if (!ctx) { qCritical("[KeyProtector] HKDF ctx_new провалился"); return {}; }

    QByteArray out(bytes, '\0');
    size_t outLen = static_cast<size_t>(bytes);

    if (EVP_PKEY_derive_init(ctx.get()) <= 0 ||
        EVP_PKEY_CTX_set_hkdf_md(ctx.get(), EVP_sha256()) <= 0 ||
        EVP_PKEY_CTX_set1_hkdf_salt(ctx.get(),
            reinterpret_cast<const unsigned char*>(kSalt.constData()),
            kSalt.size()) <= 0 ||
        EVP_PKEY_CTX_set1_hkdf_key(ctx.get(),
            reinterpret_cast<const unsigned char*>(m_masterKey.constData()),
            m_masterKey.size()) <= 0 ||
        EVP_PKEY_CTX_add1_hkdf_info(ctx.get(),
            reinterpret_cast<const unsigned char*>(label.constData()),
            label.size()) <= 0 ||
        EVP_PKEY_derive(ctx.get(),
            reinterpret_cast<unsigned char*>(out.data()), &outLen) <= 0)
    {
        qCritical("[KeyProtector] HKDF деривация провалилась");
        return {};
    }
    return out;
}

// ── AES-256-GCM шифрование ────────────────────────────────────────────────────
//
// Формат выходного блоба: [12 байт nonce][16 байт tag][ciphertext]
// Ключ шифрования выводится через deriveKey("encrypt").

QByteArray KeyProtector::encrypt(const QByteArray& plaintext) const {
    if (!isReady()) return {};

    const QByteArray encKey = deriveKey("naleystogramm-file-enc-v1");
    if (encKey.isEmpty()) return {};

    // Генерируем случайный nonce (12 байт для GCM)
    QByteArray nonce(12, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(nonce.data()), 12) != 1) {
        qCritical("[KeyProtector] RAND_bytes для nonce провалился");
        return {};
    }

    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) return {};

    QByteArray ciphertext(plaintext.size(), '\0');
    QByteArray tag(16, '\0');
    int len = 0;

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
        EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr,
            reinterpret_cast<const unsigned char*>(encKey.constData()),
            reinterpret_cast<const unsigned char*>(nonce.constData())) != 1)
    {
        qCritical("[KeyProtector] Инициализация шифрования провалилась");
        return {};
    }

    if (EVP_EncryptUpdate(ctx.get(),
            reinterpret_cast<unsigned char*>(ciphertext.data()), &len,
            reinterpret_cast<const unsigned char*>(plaintext.constData()),
            plaintext.size()) != 1)
    {
        qCritical("[KeyProtector] EVP_EncryptUpdate провалился");
        return {};
    }

    int finalLen = 0;
    if (EVP_EncryptFinal_ex(ctx.get(),
            reinterpret_cast<unsigned char*>(ciphertext.data()) + len, &finalLen) != 1)
    {
        qCritical("[KeyProtector] EVP_EncryptFinal_ex провалился");
        return {};
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16,
            reinterpret_cast<unsigned char*>(tag.data())) != 1)
    {
        qCritical("[KeyProtector] Получение GCM-тега провалилось");
        return {};
    }

    // Формат: nonce(12) + tag(16) + ciphertext
    return nonce + tag + ciphertext;
}

// ── AES-256-GCM расшифровка ───────────────────────────────────────────────────

QByteArray KeyProtector::decrypt(const QByteArray& blob) const {
    if (!isReady()) return {};
    // Минимальный размер: 12 (nonce) + 16 (tag) + 0 (пустое сообщение) = 28
    if (blob.size() < 28) {
        qWarning("[KeyProtector] decrypt: блоб слишком мал (%d байт)", blob.size());
        return {};
    }

    const QByteArray encKey = deriveKey("naleystogramm-file-enc-v1");
    if (encKey.isEmpty()) return {};

    const QByteArray nonce      = blob.left(12);
    const QByteArray tag        = blob.mid(12, 16);
    const QByteArray ciphertext = blob.mid(28);

    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) return {};

    QByteArray plaintext(ciphertext.size(), '\0');
    int len = 0;

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
        EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr,
            reinterpret_cast<const unsigned char*>(encKey.constData()),
            reinterpret_cast<const unsigned char*>(nonce.constData())) != 1)
    {
        qCritical("[KeyProtector] Инициализация расшифровки провалилась");
        return {};
    }

    if (EVP_DecryptUpdate(ctx.get(),
            reinterpret_cast<unsigned char*>(plaintext.data()), &len,
            reinterpret_cast<const unsigned char*>(ciphertext.constData()),
            ciphertext.size()) != 1)
    {
        qCritical("[KeyProtector] EVP_DecryptUpdate провалился");
        return {};
    }

    // Устанавливаем ожидаемый GCM-тег перед финализацией
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16,
            const_cast<char*>(tag.constData())) != 1)
    {
        qCritical("[KeyProtector] Установка GCM-тега провалилась");
        return {};
    }

    int finalLen = 0;
    if (EVP_DecryptFinal_ex(ctx.get(),
            reinterpret_cast<unsigned char*>(plaintext.data()) + len, &finalLen) <= 0)
    {
        // Тег аутентификации не совпал — данные повреждены или ключ неверен
        qCritical("[KeyProtector] GCM-тег не совпал — данные повреждены или неверный ключ");
        return {};
    }

    return plaintext;
}
