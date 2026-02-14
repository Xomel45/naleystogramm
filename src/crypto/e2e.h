#pragma once
#include "x3dh.h"
#include "ratchet.h"
#include <QObject>
#include <QMap>
#include <QUuid>
#include <QJsonObject>

// One E2E session per peer.
// Handles key generation, bundle exchange, and message encryption.
class E2EManager : public QObject {
    Q_OBJECT
public:
    explicit E2EManager(QObject* parent = nullptr);

    // Load or generate our key bundle from disk
    void        init(const QUuid& ourUuid);

    [[nodiscard]] QJsonObject ourBundleJson() const;
    [[nodiscard]] QJsonObject acceptSession(const QUuid& peerUuid,
                               const QJsonObject& theirBundle);
    [[nodiscard]] QJsonObject initiateSession(const QUuid& peerUuid,
                                 const QJsonObject& theirBundle);

    void        processInitMessage(const QUuid& peerUuid,
                                    const QJsonObject& initMsg);

    [[nodiscard]] QJsonObject encrypt(const QUuid& peerUuid, const QByteArray& plaintext);
    [[nodiscard]] QByteArray  decrypt(const QUuid& peerUuid, const QJsonObject& envelope);
    [[nodiscard]] bool        hasSession(const QUuid& peerUuid) const;

signals:
    void sessionEstablished(QUuid peerUuid);

private:
    // Our permanent identity key pair
    QByteArray m_ikPriv;
    QByteArray m_ikPub;

    // Our signed pre-key
    QByteArray m_spkPriv;
    QByteArray m_spkPub;
    QByteArray m_spkSig;

    // Our one-time pre-keys (pool)
    QList<QPair<QByteArray,QByteArray>> m_otpks; // (priv, pub)

    QMap<QUuid, RatchetState> m_sessions;

    QString     m_keysPath;

    void        loadOrGenerateKeys();
    void        saveKeys();
    QByteArray  consumeOtpkPriv(const QByteArray& pub);
};
