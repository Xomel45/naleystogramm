#include "e2e.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <openssl/rand.h>

static constexpr int kOtpkPoolSize = 10;

E2EManager::E2EManager(QObject* parent) : QObject(parent) {}

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
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        const auto obj = QJsonDocument::fromJson(f.readAll()).object();
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
            qDebug("[E2E] Keys loaded from disk");
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

    saveKeys();
    qDebug("[E2E] New keys generated");
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
    f.write(QJsonDocument(obj).toJson());
}

// ── Bundle serialization ──────────────────────────────────────────────────

QJsonObject E2EManager::ourBundleJson() const {
    QJsonObject bundle;
    bundle["ik"]      = QString::fromLatin1(m_ikPub.toHex());
    bundle["spk"]     = QString::fromLatin1(m_spkPub.toHex());
    bundle["spk_sig"] = QString::fromLatin1(m_spkSig.toHex());

    if (!m_otpks.isEmpty()) {
        bundle["otpk"] = QString::fromLatin1(m_otpks.first().second.toHex());
    }
    return bundle;
}

// ── Session establishment ─────────────────────────────────────────────────

QJsonObject E2EManager::initiateSession(const QUuid& peerUuid,
                                         const QJsonObject& theirBundle) {
    X3DHKeyBundle b;
    b.identityKey   = QByteArray::fromHex(theirBundle["ik"].toString().toLatin1());
    b.signedPreKey  = QByteArray::fromHex(theirBundle["spk"].toString().toLatin1());
    b.signedPreKeySig=QByteArray::fromHex(theirBundle["spk_sig"].toString().toLatin1());
    if (theirBundle.contains("otpk"))
        b.oneTimePreKey = QByteArray::fromHex(theirBundle["otpk"].toString().toLatin1());

    QByteArray ephPub;
    auto secret = X3DH::initiatorAgreement(m_ikPriv, m_ikPub, b, ephPub);
    if (!secret) {
        qWarning("[E2E] initiatorAgreement failed");
        return {};
    }

    // Init ratchet as sender — use their SPK as initial ratchet pub
    m_sessions[peerUuid] = DoubleRatchet::initSender(*secret, b.signedPreKey);

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

    // Init ratchet as receiver — use our SPK as initial DH key pair
    m_sessions[peerUuid] = DoubleRatchet::initReceiver(
        *secret, m_spkPriv, m_spkPub);

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

QJsonObject E2EManager::encrypt(const QUuid& peerUuid,
                                  const QByteArray& plaintext) {
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
    env["ct"]         = QString::fromLatin1(rm.ciphertext.toHex());
    env["nonce"]      = QString::fromLatin1(rm.nonce.toHex());
    env["tag"]        = QString::fromLatin1(rm.tag.toHex());
    return env;
}

QByteArray E2EManager::decrypt(const QUuid& peerUuid,
                                 const QJsonObject& envelope) {
    if (!m_sessions.contains(peerUuid)) {
        qWarning("[E2E] No session for %s", qPrintable(peerUuid.toString()));
        return {};
    }

    RatchetMessage rm;
    rm.dhPub      = QByteArray::fromHex(envelope["dh"].toString().toLatin1());
    rm.msgNum     = static_cast<quint32>(envelope["n"].toInt());
    rm.ciphertext = QByteArray::fromHex(envelope["ct"].toString().toLatin1());
    rm.nonce      = QByteArray::fromHex(envelope["nonce"].toString().toLatin1());
    rm.tag        = QByteArray::fromHex(envelope["tag"].toString().toLatin1());

    auto& state = m_sessions[peerUuid];
    return DoubleRatchet::decrypt(state, rm);
}

bool E2EManager::hasSession(const QUuid& peerUuid) const {
    return m_sessions.contains(peerUuid);
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
