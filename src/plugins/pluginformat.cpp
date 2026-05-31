#include "pluginformat.h"
#include "../crypto/openssl_raii.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

// ── Последняя ошибка ──────────────────────────────────────────────────────

thread_local QString g_lastError;

void setError(const QString& msg) { g_lastError = msg; }

// ── Сериализация архива ───────────────────────────────────────────────────

QByteArray packArchive(const QList<PluginFile>& files) {
    QByteArray buf;
    QDataStream ds(&buf, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds << static_cast<quint32>(files.size());
    for (const auto& f : files) {
        const QByteArray name = f.name.toUtf8();
        ds << static_cast<quint16>(name.size());
        ds.writeRawData(name.constData(), name.size());
        ds << static_cast<quint32>(f.data.size());
        ds.writeRawData(f.data.constData(), f.data.size());
    }
    return buf;
}

QList<PluginFile> unpackArchive(const QByteArray& raw) {
    QList<PluginFile> files;
    QDataStream ds(raw);
    ds.setByteOrder(QDataStream::LittleEndian);

    quint32 count = 0;
    ds >> count;
    if (ds.status() != QDataStream::Ok) return {};

    for (quint32 i = 0; i < count; ++i) {
        quint16 nameLen = 0;
        ds >> nameLen;
        if (ds.status() != QDataStream::Ok) return {};

        QByteArray nameBuf(nameLen, '\0');
        if (ds.readRawData(nameBuf.data(), nameLen) != nameLen) return {};

        quint32 dataLen = 0;
        ds >> dataLen;
        if (ds.status() != QDataStream::Ok) return {};

        QByteArray dataBuf(static_cast<int>(dataLen), '\0');
        if (static_cast<quint32>(ds.readRawData(dataBuf.data(), static_cast<int>(dataLen)))
                != dataLen) return {};

        files.append({QString::fromUtf8(nameBuf), dataBuf});
    }
    return files;
}

// ── Криптография (AES-256-GCM + PBKDF2) ──────────────────────────────────

bool deriveKey(const QString& password, const QByteArray& salt, QByteArray& outKey) {
    outKey.resize(32);
    const QByteArray pwd = password.toUtf8();
    return PKCS5_PBKDF2_HMAC(
        pwd.constData(),        static_cast<int>(pwd.size()),
        reinterpret_cast<const unsigned char*>(salt.constData()), salt.size(),
        100000, EVP_sha256(),
        32,
        reinterpret_cast<unsigned char*>(outKey.data())
    ) == 1;
}

bool gcmEncrypt(const QByteArray& plain, const QByteArray& key, const QByteArray& nonce,
                QByteArray& cipher, QByteArray& tag) {
    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) return false;

    int len = 0, flen = 0;
    cipher.resize(plain.size());
    tag.resize(16);

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
        return false;
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, nonce.size(), nullptr) != 1)
        return false;
    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr,
            reinterpret_cast<const unsigned char*>(key.constData()),
            reinterpret_cast<const unsigned char*>(nonce.constData())) != 1)
        return false;
    if (EVP_EncryptUpdate(ctx.get(),
            reinterpret_cast<unsigned char*>(cipher.data()), &len,
            reinterpret_cast<const unsigned char*>(plain.constData()), plain.size()) != 1)
        return false;
    if (EVP_EncryptFinal_ex(ctx.get(),
            reinterpret_cast<unsigned char*>(cipher.data()) + len, &flen) != 1)
        return false;
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16,
            reinterpret_cast<unsigned char*>(tag.data())) != 1)
        return false;

    cipher.resize(len + flen);
    return true;
}

bool gcmDecrypt(const QByteArray& cipher, const QByteArray& key, const QByteArray& nonce,
                QByteArray tag, QByteArray& plain) {
    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) return false;

    int len = 0, flen = 0;
    plain.resize(cipher.size());

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
        return false;
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, nonce.size(), nullptr) != 1)
        return false;
    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr,
            reinterpret_cast<const unsigned char*>(key.constData()),
            reinterpret_cast<const unsigned char*>(nonce.constData())) != 1)
        return false;
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16,
            reinterpret_cast<unsigned char*>(tag.data())) != 1)
        return false;
    if (EVP_DecryptUpdate(ctx.get(),
            reinterpret_cast<unsigned char*>(plain.data()), &len,
            reinterpret_cast<const unsigned char*>(cipher.constData()), cipher.size()) != 1)
        return false;
    if (EVP_DecryptFinal_ex(ctx.get(),
            reinterpret_cast<unsigned char*>(plain.data()) + len, &flen) != 1)
        return false;

    plain.resize(len + flen);
    return true;
}

// ── Чтение заголовка (внутренняя) ─────────────────────────────────────────

struct FileHeader {
    quint16    flags    {0};
    PluginMeta meta;
    qint64     payloadOffset {0};  // позиция в файле после метаданных
};

bool readHeader(QDataStream& ds, FileHeader& hdr) {
    quint32 magic = 0;
    quint16 version = 0;
    ds >> magic >> version >> hdr.flags;
    if (ds.status() != QDataStream::Ok || magic != PluginFormat::kMagic)
        return false;
    if (version != PluginFormat::kVersion)
        return false;

    quint32 metaLen = 0;
    ds >> metaLen;
    if (ds.status() != QDataStream::Ok || metaLen == 0 || metaLen > 64 * 1024)
        return false;

    QByteArray metaJson(static_cast<int>(metaLen), '\0');
    if (ds.readRawData(metaJson.data(), static_cast<int>(metaLen))
            != static_cast<int>(metaLen))
        return false;

    hdr.meta = PluginMeta::fromJson(
        QJsonDocument::fromJson(metaJson).object());
    hdr.meta.encrypted = (hdr.flags & PluginFormat::kFlagEncrypted) != 0;
    return hdr.meta.isValid();
}

// ── Получить payload (plaintext) ──────────────────────────────────────────

QByteArray readPayload(QDataStream& ds, const FileHeader& hdr, const QString& key) {
    if (hdr.flags & PluginFormat::kFlagEncrypted) {
        // Читаем соль, нонс, тег, зашифрованный payload
        QByteArray salt(32, '\0'), nonce(12, '\0'), tag(16, '\0');
        if (ds.readRawData(salt.data(), 32) != 32)  return {};
        if (ds.readRawData(nonce.data(), 12) != 12) return {};
        if (ds.readRawData(tag.data(), 16) != 16)   return {};

        quint32 cipherLen = 0;
        ds >> cipherLen;
        if (ds.status() != QDataStream::Ok || cipherLen == 0) return {};

        QByteArray cipher(static_cast<int>(cipherLen), '\0');
        if (static_cast<quint32>(ds.readRawData(cipher.data(), static_cast<int>(cipherLen)))
                != cipherLen) return {};

        QByteArray derivedKey;
        if (!deriveKey(key, salt, derivedKey)) {
            setError(QStringLiteral("Key derivation failed"));
            return {};
        }

        QByteArray plain;
        if (!gcmDecrypt(cipher, derivedKey, nonce, tag, plain)) {
            setError(QStringLiteral("Decryption failed — wrong key or corrupted file"));
            return {};
        }
        return plain;

    } else {
        quint32 payloadLen = 0;
        ds >> payloadLen;
        if (ds.status() != QDataStream::Ok || payloadLen == 0) return {};

        QByteArray payload(static_cast<int>(payloadLen), '\0');
        if (static_cast<quint32>(ds.readRawData(payload.data(), static_cast<int>(payloadLen)))
                != payloadLen) return {};
        return payload;
    }
}

} // anonymous namespace

// ═════════════════════════════════════════════════════════════════════════════

// ── PluginMeta ────────────────────────────────────────────────────────────

QJsonObject PluginMeta::toJson() const {
    return {
        {QStringLiteral("id"),             id},
        {QStringLiteral("name"),           name},
        {QStringLiteral("description"),    description},
        {QStringLiteral("version"),        version},
        {QStringLiteral("author"),         author},
        {QStringLiteral("minAppVersion"),  minAppVersion},
        {QStringLiteral("encrypted"),      encrypted},
    };
}

PluginMeta PluginMeta::fromJson(const QJsonObject& obj) {
    PluginMeta m;
    m.id             = obj[QStringLiteral("id")].toString();
    m.name           = obj[QStringLiteral("name")].toString();
    m.description    = obj[QStringLiteral("description")].toString();
    m.version        = obj[QStringLiteral("version")].toString();
    m.author         = obj[QStringLiteral("author")].toString();
    m.minAppVersion  = obj[QStringLiteral("minAppVersion")].toString();
    m.encrypted      = obj[QStringLiteral("encrypted")].toBool(false);
    return m;
}

// ── PluginFormat ──────────────────────────────────────────────────────────

QString PluginFormat::lastError() { return g_lastError; }

PluginMeta PluginFormat::readMeta(const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Cannot open: ") + filePath);
        return {};
    }
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);

    FileHeader hdr;
    if (!readHeader(ds, hdr)) {
        setError(QStringLiteral("Invalid .plugin header: ") + filePath);
        return {};
    }
    return hdr.meta;
}

bool PluginFormat::write(const QString& filePath, const PluginMeta& meta,
                         const QList<PluginFile>& files, const QString& key) {
    g_lastError.clear();

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(QStringLiteral("Cannot open for writing: ") + filePath);
        return false;
    }

    const QByteArray metaJson = QJsonDocument(meta.toJson()).toJson(QJsonDocument::Compact);
    const QByteArray archive  = packArchive(files);
    const quint16    flags    = key.isEmpty() ? 0 : kFlagEncrypted;

    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);

    // Заголовок
    ds << kMagic << kVersion << flags;
    ds << static_cast<quint32>(metaJson.size());
    ds.writeRawData(metaJson.constData(), metaJson.size());

    if (flags & kFlagEncrypted) {
        QByteArray salt(32, '\0'), nonce(12, '\0');
        if (RAND_bytes(reinterpret_cast<unsigned char*>(salt.data()), 32) != 1 ||
            RAND_bytes(reinterpret_cast<unsigned char*>(nonce.data()), 12) != 1) {
            setError(QStringLiteral("RAND_bytes failed"));
            return false;
        }

        QByteArray derivedKey;
        if (!deriveKey(key, salt, derivedKey)) {
            setError(QStringLiteral("Key derivation failed"));
            return false;
        }

        QByteArray cipher, tag;
        if (!gcmEncrypt(archive, derivedKey, nonce, cipher, tag)) {
            setError(QStringLiteral("Encryption failed"));
            return false;
        }

        ds.writeRawData(salt.constData(), 32);
        ds.writeRawData(nonce.constData(), 12);
        ds.writeRawData(tag.constData(), 16);
        ds << static_cast<quint32>(cipher.size());
        ds.writeRawData(cipher.constData(), cipher.size());
    } else {
        ds << static_cast<quint32>(archive.size());
        ds.writeRawData(archive.constData(), archive.size());
    }

    return ds.status() == QDataStream::Ok;
}

bool PluginFormat::extract(const QString& filePath, const QString& destDir, const QString& key) {
    g_lastError.clear();

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Cannot open: ") + filePath);
        return false;
    }

    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);

    FileHeader hdr;
    if (!readHeader(ds, hdr)) {
        setError(QStringLiteral("Invalid header"));
        return false;
    }

    if ((hdr.flags & kFlagEncrypted) && key.isEmpty()) {
        setError(QStringLiteral("Plugin is encrypted, key required"));
        return false;
    }

    const QByteArray payload = readPayload(ds, hdr, key);
    if (payload.isEmpty()) {
        if (g_lastError.isEmpty())
            setError(QStringLiteral("Empty payload"));
        return false;
    }

    const QList<PluginFile> extracted = unpackArchive(payload);
    if (extracted.isEmpty()) {
        setError(QStringLiteral("Archive is empty or corrupt"));
        return false;
    }

    QDir dir(destDir);
    if (!dir.mkpath(QStringLiteral("."))) {
        setError(QStringLiteral("Cannot create directory: ") + destDir);
        return false;
    }

    const QString base = QDir(destDir).absolutePath() + QLatin1Char('/');

    for (const auto& pf : extracted) {
        // ── Защита от path traversal ──────────────────────────────────────
        // Отклоняем: абсолютные пути, сегменты .., обратные слэши (Windows)
        if (pf.name.isEmpty() ||
            QFileInfo(pf.name).isAbsolute() ||
            pf.name.contains(QStringLiteral("..")) ||
            pf.name.contains(QLatin1Char('\\'))) {
            setError(QStringLiteral("Unsafe entry name: ") + pf.name);
            return false;
        }
        // Канонический путь должен оставаться внутри base
        const QString candidate = QDir::cleanPath(base + pf.name);
        if (!candidate.startsWith(base)) {
            setError(QStringLiteral("Entry escapes destDir: ") + pf.name);
            return false;
        }

        const QString outPath = candidate;
        QFileInfo fi(outPath);
        dir.mkpath(fi.dir().absolutePath());

        QFile out(outPath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            setError(QStringLiteral("Cannot write: ") + outPath);
            return false;
        }
        out.write(pf.data);
        out.close();

#if defined(Q_OS_UNIX)
        // Устанавливаем +x для .so файлов
        if (pf.name.endsWith(QStringLiteral(".so")) ||
            pf.name.endsWith(QStringLiteral(".dll"))) {
            out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                               QFileDevice::ExeOwner  | QFileDevice::ReadGroup  |
                               QFileDevice::ReadOther);
        }
#endif
    }
    return true;
}

bool PluginFormat::verifyKey(const QString& filePath, const QString& key) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);

    FileHeader hdr;
    if (!readHeader(ds, hdr)) return false;

    // Незашифрованный — всегда OK
    if (!(hdr.flags & kFlagEncrypted)) return true;

    // Пытаемся расшифровать — gcmDecrypt провалится при неверном теге
    const QByteArray payload = readPayload(ds, hdr, key);
    return !payload.isEmpty();
}
