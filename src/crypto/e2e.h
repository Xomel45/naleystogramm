#pragma once
#include "x3dh.h"
#include "ratchet.h"
#include <QObject>
#include <QMap>
#include <QHash>
#include <QMutex>
#include <QUuid>
#include <QJsonObject>

// One E2E session per peer.
// Handles key generation, bundle exchange, and message encryption.
class E2EManager : public QObject {
    Q_OBJECT
public:
    explicit E2EManager(QObject* parent = nullptr);
    ~E2EManager();

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

    // Получить 32-байтный ключ шифрования медиа для звонка.
    // Детерминирован: оба пира получат одинаковый ключ при одинаковых callId+salt.
    // salt — 8 случайных байт из CALL_INVITE (передаются открыто, но rootKey секретен).
    [[nodiscard]] QByteArray  snapshotMediaKey(const QUuid& peerUuid,
                                               const QString& callId,
                                               const QByteArray& salt);

    // Возвращает «номер безопасности» — отпечаток пары identity-ключей (защита от MITM).
    // Формат: SHA-256(ourIK || peerIK) в виде 5 групп по 8 hex-символов.
    // Пользователи должны сверить значение голосом/видео.
    [[nodiscard]] QString     getSafetyNumber(const QUuid& peerUuid) const;

signals:
    void sessionEstablished(QUuid peerUuid);

private:
    // Our permanent identity key pair
    QByteArray m_ikPriv;
    QByteArray m_ikPub;
    // Ed25519 публичный ключ, выведенный из m_ikPriv — используется для верификации SPK
    QByteArray m_ikEdPub;

    // Our signed pre-key
    QByteArray m_spkPriv;
    QByteArray m_spkPub;
    QByteArray m_spkSig;

    // Our one-time pre-keys (pool)
    QList<QPair<QByteArray,QByteArray>> m_otpks; // (priv, pub)

    QMap<QUuid, RatchetState> m_sessions;

    // Identity-публичные ключи пиров — для вычисления Safety Numbers
    QMap<QUuid, QByteArray>   m_peerIdentityKeys;

    // Мьютексы для потокобезопасного доступа к RatchetState (по одному на пир).
    // m_mapMutex защищает саму HashMap при создании новых мьютексов (L-1).
    QHash<QUuid, QMutex*>     m_sessionMutexes;
    QMutex                    m_mapMutex;
    QMutex* mutexFor(const QUuid& uuid);

    QString     m_keysPath;

    void        loadOrGenerateKeys();
    void        saveKeys();
    QByteArray  consumeOtpkPriv(const QByteArray& pub);
};
