#include "e2e.h"
#include "securedata.h"
#include "keyprotector.h"
#include "x3dh.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QCryptographicHash>
#include <QMutexLocker>
#include <openssl/rand.h>

static constexpr int kOtpkPoolSize = 10;

E2EManager::E2EManager(QObject* parent) : QObject(parent) {}

E2EManager::~E2EManager() {
    // Удаляем мьютексы до обнуления ключей (ни один поток не должен их держать при дестрое)
    qDeleteAll(m_sessionMutexes);
    m_sessionMutexes.clear();

    // Гарантированно обнуляем весь приватный ключевой материал при уничтожении.
    // Предотвращает утечку ключей через освобождённую память (heap/swap).
    secureZero(m_ikPriv);
    secureZero(m_spkPriv);
    for (auto& kp : m_otpks)
        secureZero(kp.first);
    m_otpks.clear();
}

void E2EManager::init(const QUuid& ourUuid) {
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/keys";
    QDir().mkpath(dir);
    m_keysPath = dir + "/" + ourUuid.toString(QUuid::WithoutBraces) + ".json";
    loadOrGenerateKeys();
}

// ── Key persistence ───────────────────────────────────────────────────────

void E2EManager::loadOrGenerateKeys() {
    QFile f(m_keysPath);
    if (f.exists()) {
        if (!f.open(QIODevice::ReadOnly)) {
            qCritical("[E2E] Не удалось открыть %s", qPrintable(m_keysPath));
            return;
        }
        const QByteArray raw = f.readAll();
        f.close();

        // Определяем формат: зашифрованный блоб или legacy plaintext JSON.
        // Зашифрованный блоб начинается со случайного nonce — не с '{' (0x7B).
        QByteArray jsonBytes;
        const bool looksLikeJson = !raw.isEmpty() && raw[0] == '{';
        if (!looksLikeJson && KeyProtector::instance().isReady()) {
            // Новый формат: расшифровываем
            jsonBytes = KeyProtector::instance().decrypt(raw);
            if (jsonBytes.isEmpty()) {
                qCritical("[E2E] Не удалось расшифровать keys.json — ключ повреждён?");
                // НЕ генерируем новые ключи автоматически: это может означать атаку.
                return;
            }
            qDebug("[E2E] keys.json расшифрован успешно");
        } else {
            // Legacy plaintext или KeyProtector не готов
            jsonBytes = raw;
            if (!looksLikeJson)
                qWarning("[E2E] KeyProtector не готов — загружаем ключи без расшифровки");
        }

        const auto obj = QJsonDocument::fromJson(jsonBytes).object();
        m_ikPriv = QByteArray::fromHex(obj["ik_priv"].toString().toLatin1());
        m_ikPub  = QByteArray::fromHex(obj["ik_pub"].toString().toLatin1());
        m_spkPriv= QByteArray::fromHex(obj["spk_priv"].toString().toLatin1());
        m_spkPub = QByteArray::fromHex(obj["spk_pub"].toString().toLatin1());
        m_spkSig = QByteArray::fromHex(obj["spk_sig"].toString().toLatin1());

        m_otpks.clear();
        for (const auto& v : obj["otpks"].toArray()) {
            const auto o = v.toObject();
            m_otpks.append({
                QByteArray::fromHex(o["priv"].toString().toLatin1()),
                QByteArray::fromHex(o["pub"].toString().toLatin1())
            });
        }

        if (!m_ikPriv.isEmpty() && !m_spkPriv.isEmpty()) {
            // Вычисляем Ed25519 публичный ключ для верификации SPK подписей
            m_ikEdPub = X3DH::ikPrivToEdPub(m_ikPriv);
            qDebug("[E2E] Ключи загружены с диска (ikEdPub: %s)",
                   m_ikEdPub.isEmpty() ? "отсутствует" : "ОК");
            return;
        }
    }

    // Generate fresh bundle
    // FIX: раньше было *new QByteArray() — утечка памяти
    QByteArray dummyPriv, dummyPub;
    if (!X3DH::generateBundle(m_ikPriv, m_ikPub,
                               m_spkPriv, m_spkPub, m_spkSig,
                               dummyPriv, dummyPub))
    {
        qCritical("[E2E] Key generation failed!");
        return;
    }

    // Generate OTPKs
    m_otpks.clear();
    for (int i = 0; i < kOtpkPoolSize; ++i) {
        QByteArray priv, pub;
        QByteArray dummy1, dummy2, dummy3;
        // Результат намеренно игнорируется — при ошибке просто пустые ключи
        [[maybe_unused]] const bool ok =
            X3DH::generateBundle(dummy1, dummy2, dummy3, dummy3, dummy3, priv, pub);
        m_otpks.append({priv, pub});
    }

    // Вычисляем Ed25519 публичный ключ для верификации SPK подписей
    m_ikEdPub = X3DH::ikPrivToEdPub(m_ikPriv);

    saveKeys();
    qDebug("[E2E] Новые ключи сгенерированы (ikEdPub: %s)",
           m_ikEdPub.isEmpty() ? "отсутствует" : "ОК");
}

void E2EManager::saveKeys() {
    QJsonObject obj;
    obj["ik_priv"]  = QString::fromLatin1(m_ikPriv.toHex());
    obj["ik_pub"]   = QString::fromLatin1(m_ikPub.toHex());
    obj["spk_priv"] = QString::fromLatin1(m_spkPriv.toHex());
    obj["spk_pub"]  = QString::fromLatin1(m_spkPub.toHex());
    obj["spk_sig"]  = QString::fromLatin1(m_spkSig.toHex());

    QJsonArray arr;
    for (const auto& kp : m_otpks) {
        QJsonObject o;
        o["priv"] = QString::fromLatin1(kp.first.toHex());
        o["pub"]  = QString::fromLatin1(kp.second.toHex());
        arr.append(o);
    }
    obj["otpks"] = arr;

    QFile f(m_keysPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("[E2E] Failed to save keys to %s", qPrintable(m_keysPath));
        return;
    }

    const QByteArray jsonBytes = QJsonDocument(obj).toJson();

    if (KeyProtector::instance().isReady()) {
        // Шифруем перед записью — приватные ключи хранятся только в зашифрованном виде
        const QByteArray encrypted = KeyProtector::instance().encrypt(jsonBytes);
        if (!encrypted.isEmpty()) {
            f.write(encrypted);
            qDebug("[E2E] keys.json сохранён в зашифрованном виде");
        } else {
            // Резервный вариант: пишем plaintext чтобы не потерять ключи
            qCritical("[E2E] Шифрование keys.json провалилось — сохраняем plaintext (небезопасно)");
            f.write(jsonBytes);
        }
    } else {
        // KeyProtector не инициализирован — пишем plaintext
        qWarning("[E2E] KeyProtector не готов — keys.json сохраняется без шифрования");
        f.write(jsonBytes);
    }
}

// ── Bundle serialization ──────────────────────────────────────────────────

QJsonObject E2EManager::ourBundleJson() const {
    QJsonObject bundle;
    bundle["ik"]      = QString::fromLatin1(m_ikPub.toHex());
    bundle["spk"]     = QString::fromLatin1(m_spkPub.toHex());
    bundle["spk_sig"] = QString::fromLatin1(m_spkSig.toHex());
    // Ed25519 публичный ключ для верификации подписи SPK на стороне пира
    if (!m_ikEdPub.isEmpty())
        bundle["ik_ed"] = QString::fromLatin1(m_ikEdPub.toHex());

    if (!m_otpks.isEmpty()) {
        bundle["otpk"] = QString::fromLatin1(m_otpks.first().second.toHex());
    }
    return bundle;
}

// ── Session establishment ─────────────────────────────────────────────────

QJsonObject E2EManager::initiateSession(const QUuid& peerUuid,
                                         const QJsonObject& theirBundle) {
    X3DHKeyBundle b;
    b.identityKey    = QByteArray::fromHex(theirBundle["ik"].toString().toLatin1());
    b.signedPreKey   = QByteArray::fromHex(theirBundle["spk"].toString().toLatin1());
    b.signedPreKeySig= QByteArray::fromHex(theirBundle["spk_sig"].toString().toLatin1());
    // Ed25519 ключ верификации SPK — присутствует у клиентов v0.2.2+
    if (theirBundle.contains("ik_ed"))
        b.ikEdPub = QByteArray::fromHex(theirBundle["ik_ed"].toString().toLatin1());
    if (theirBundle.contains("otpk"))
        b.oneTimePreKey = QByteArray::fromHex(theirBundle["otpk"].toString().toLatin1());

    QByteArray ephPub;
    auto secret = X3DH::initiatorAgreement(m_ikPriv, m_ikPub, b, ephPub);
    if (!secret) {
        qWarning("[E2E] initiatorAgreement failed");
        return {};
    }

    // Сохраняем публичный identity-ключ пира для Safety Numbers
    m_peerIdentityKeys[peerUuid] = b.identityKey;

    // Init ratchet as sender — use their SPK as initial ratchet pub
    m_sessions[peerUuid] = DoubleRatchet::initSender(*secret, b.signedPreKey);

    // ДИАГНОСТИКА: логируем начальное состояние ratchet на стороне инициатора.
    // CKs (цепочка отправки) и peerDH (SPK пира) у отправителя ДОЛЖНЫ совпадать
    // с CKr (ckr в dhRatchet), который вычислит получатель на первом сообщении.
#ifdef QT_DEBUG
    {
        const auto& s = m_sessions[peerUuid];
        qDebug("[E2E][initSender] uuid=%s  SK=%s  CKs=%s  peerDH(SPK)=%s",
               qPrintable(peerUuid.toString(QUuid::WithoutBraces).left(8)),
               secret->left(4).toHex().constData(),
               s.sendChainKey.left(4).toHex().constData(),
               s.peerDHPub.left(4).toHex().constData());
    }
#endif

    QJsonObject msg;
    msg["type"]   = "KEY_INIT";
    msg["ik"]     = QString::fromLatin1(m_ikPub.toHex());
    msg["ek"]     = QString::fromLatin1(ephPub.toHex());
    msg["otpk"]   = QString::fromLatin1(b.oneTimePreKey.toHex());
    msg["bundle"] = ourBundleJson(); // also send our bundle so they can reply

    emit sessionEstablished(peerUuid);
    return msg;
}

QJsonObject E2EManager::acceptSession(const QUuid& peerUuid,
                                       const QJsonObject& initMsg) {
    X3DHInitMessage alice;
    alice.identityKey  = QByteArray::fromHex(initMsg["ik"].toString().toLatin1());
    alice.ephemeralKey = QByteArray::fromHex(initMsg["ek"].toString().toLatin1());

    QByteArray otpkPriv;
    if (initMsg.contains("otpk") && !initMsg["otpk"].toString().isEmpty()) {
        const QByteArray otpkPub =
            QByteArray::fromHex(initMsg["otpk"].toString().toLatin1());
        otpkPriv = consumeOtpkPriv(otpkPub);
    }

    auto secret = X3DH::responderAgreement(m_ikPriv, m_spkPriv, otpkPriv, alice);
    if (!secret) {
        qWarning("[E2E] responderAgreement failed");
        return {};
    }

    // Сохраняем публичный identity-ключ пира для Safety Numbers
    m_peerIdentityKeys[peerUuid] = alice.identityKey;

    // Init ratchet as receiver — use our SPK as initial DH key pair
    m_sessions[peerUuid] = DoubleRatchet::initReceiver(
        *secret, m_spkPriv, m_spkPub);

    // ДИАГНОСТИКА: логируем начальное состояние ratchet на стороне получателя.
    // SK должен совпадать с SK у инициатора. CKs пока пуст — заполнится в dhRatchet
    // при первом входящем сообщении. dhPub = наш SPK (будет меняться после первого decrypt).
#ifdef QT_DEBUG
    {
        const auto& s = m_sessions[peerUuid];
        qDebug("[E2E][initReceiver] uuid=%s  SK=%s  dhPub(SPK)=%s  CKs=%s  CKr=%s",
               qPrintable(peerUuid.toString(QUuid::WithoutBraces).left(8)),
               secret->left(4).toHex().constData(),
               s.dhPub.left(4).toHex().constData(),
               s.sendChainKey.isEmpty() ? "пуст" : s.sendChainKey.left(4).toHex().constData(),
               s.recvChainKey.isEmpty() ? "пуст" : s.recvChainKey.left(4).toHex().constData());
    }
#endif

    emit sessionEstablished(peerUuid);

    // Return our bundle so initiator knows our ratchet key
    QJsonObject reply;
    reply["type"]   = "KEY_ACK";
    reply["bundle"] = ourBundleJson();
    return reply;
}

void E2EManager::processInitMessage(const QUuid& peerUuid,
                                     const QJsonObject& msg) {
    if (msg["type"] == "KEY_INIT") {
        const auto reply = acceptSession(peerUuid, msg);
        // Caller is responsible for sending reply back to peer
        Q_UNUSED(reply)
    }
}

// ── Encrypt / Decrypt ─────────────────────────────────────────────────────

// Возвращает (создаёт при необходимости) мьютекс для конкретного пира.
// L-1: доступ к m_sessionMutexes защищён m_mapMutex, чтобы одновременные вызовы
// из разных потоков не гонялись на вставке в HashMap.
QMutex* E2EManager::mutexFor(const QUuid& uuid) {
    QMutexLocker<QMutex> mapLock(&m_mapMutex);
    if (!m_sessionMutexes.contains(uuid))
        m_sessionMutexes[uuid] = new QMutex();
    return m_sessionMutexes[uuid];
}

QJsonObject E2EManager::encrypt(const QUuid& peerUuid,
                                  const QByteArray& plaintext) {
    QMutexLocker<QMutex> lock(mutexFor(peerUuid));

    if (!m_sessions.contains(peerUuid)) {
        qWarning("[E2E] No session for %s", qPrintable(peerUuid.toString()));
        return {};
    }

    auto& state = m_sessions[peerUuid];
    const RatchetMessage rm = DoubleRatchet::encrypt(state, plaintext);

    QJsonObject env;
    env["type"]       = "CHAT";
    env["dh"]         = QString::fromLatin1(rm.dhPub.toHex());
    env["n"]          = static_cast<int>(rm.msgNum);
    env["pn"]         = static_cast<int>(rm.prevChainLen); // длина предыдущей цепочки (для пропущенных ключей)
    env["ct"]         = QString::fromLatin1(rm.ciphertext.toHex());
    env["nonce"]      = QString::fromLatin1(rm.nonce.toHex());
    env["tag"]        = QString::fromLatin1(rm.tag.toHex());

    // Лог заголовка — для сверки с [E2E][←...] на стороне получателя
#ifdef QT_DEBUG
    qDebug("[E2E][→%s] отправка CHAT: dh=%s  n=%d  pn=%d",
           qPrintable(peerUuid.toString(QUuid::WithoutBraces).left(8)),
           rm.dhPub.left(4).toHex().constData(),
           static_cast<int>(rm.msgNum),
           static_cast<int>(rm.prevChainLen));
#endif

    return env;
}

QByteArray E2EManager::decrypt(const QUuid& peerUuid,
                                 const QJsonObject& envelope) {
    QMutexLocker<QMutex> lock(mutexFor(peerUuid));

    if (!m_sessions.contains(peerUuid)) {
        qWarning("[E2E] No session for %s", qPrintable(peerUuid.toString()));
        return {};
    }

    RatchetMessage rm;
    rm.dhPub        = QByteArray::fromHex(envelope["dh"].toString().toLatin1());
    rm.msgNum       = static_cast<quint32>(envelope["n"].toInt());
    rm.prevChainLen = static_cast<quint32>(envelope["pn"].toInt(0)); // 0 для обратной совместимости
    rm.ciphertext   = QByteArray::fromHex(envelope["ct"].toString().toLatin1());
    rm.nonce        = QByteArray::fromHex(envelope["nonce"].toString().toLatin1());
    rm.tag          = QByteArray::fromHex(envelope["tag"].toString().toLatin1());

    // Лог входящего заголовка — dh/n/pn ДОЛЖНЫ совпадать с [E2E][→...] на отправителе
#ifdef QT_DEBUG
    qDebug("[E2E][←%s] получен CHAT:   dh=%s  n=%d  pn=%d",
           qPrintable(peerUuid.toString(QUuid::WithoutBraces).left(8)),
           rm.dhPub.left(4).toHex().constData(),
           static_cast<int>(rm.msgNum),
           static_cast<int>(rm.prevChainLen));
#endif

    auto& state = m_sessions[peerUuid];
    const QByteArray result = DoubleRatchet::decrypt(state, rm);
    if (result.isEmpty())
        qWarning("[E2E] ❌ расшифровка провалилась (dh=%s  n=%d) — ключи не совпадают!",
                 rm.dhPub.left(4).toHex().constData(), static_cast<int>(rm.msgNum));
    return result;
}

bool E2EManager::hasSession(const QUuid& peerUuid) const {
    return m_sessions.contains(peerUuid);
}

// ── Media Key (для голосовых звонков) ─────────────────────────────────────
//
// Оба пира получат одинаковый 32-байтный ключ при одинаковых callId+salt,
// т.к. rootKey идентичен на обоих концах (после X3DH/DR инициализации).
// salt отправляется открыто в CALL_INVITE, но rootKey не раскрывается.

QByteArray E2EManager::snapshotMediaKey(const QUuid& peerUuid,
                                          const QString& callId,
                                          const QByteArray& salt)
{
    QMutexLocker lock(mutexFor(peerUuid));
    const auto it = m_sessions.constFind(peerUuid);
    if (it == m_sessions.constEnd() || !it->initialized) return {};

    // info = "naleystogramm-media-v1:" + callId + ":" + salt (hex)
    const QByteArray info = QByteArrayLiteral("naleystogramm-media-v1:")
                          + callId.toUtf8()
                          + ":"
                          + salt.toHex();
    return DoubleRatchet::hkdf2(it->rootKey, info, 32);
}

// ── Safety Numbers ────────────────────────────────────────────────────────
//
// «Номер безопасности» = SHA-256(нашIKpub || ихIKpub).
// Позволяет пользователям верифицировать сессию голосом/видео
// и убедиться, что нет атаки «человек посередине» (MITM).

QString E2EManager::getSafetyNumber(const QUuid& peerUuid) const {
    if (m_ikPub.isEmpty() || !m_peerIdentityKeys.contains(peerUuid))
        return {};

    // Конкатенируем наш и их identity-ключ, хэшируем SHA-256
    const QByteArray combined = m_ikPub + m_peerIdentityKeys[peerUuid];
    const QByteArray hash = QCryptographicHash::hash(combined,
                                                      QCryptographicHash::Sha256);

    // Форматируем как 5 групп по 8 hex-символов (160 бит = 20 байт)
    const QString hex = QString::fromLatin1(hash.left(20).toHex());
    QString result;
    for (int i = 0; i < 5; ++i) {
        if (i > 0) result += ' ';
        result += hex.mid(i * 8, 8).toUpper();
    }
    return result;
}

QByteArray E2EManager::consumeOtpkPriv(const QByteArray& pub) {
    for (int i = 0; i < m_otpks.size(); ++i) {
        if (m_otpks[i].second == pub) {
            const QByteArray priv = m_otpks[i].first;
            m_otpks.removeAt(i);
            saveKeys(); // consumed — update disk
            return priv;
        }
    }
    return {};
}
