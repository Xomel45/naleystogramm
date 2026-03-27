#pragma once
#include <QByteArray>
#include <QMap>
#include <QPair>

// Double Ratchet Algorithm (per-message E2E encryption)
// After X3DH establishes a shared secret, Double Ratchet
// provides forward secrecy and break-in recovery.
//
// Each message uses a unique key derived from the ratchet chain.

// Максимальное количество хранимых ключей пропущенных сообщений.
// Защищает от исчерпания памяти при получении сообщений сильно не по порядку.
static constexpr quint32 kMaxSkippedKeys = 100;

struct RatchetState {
    // Корневой ключ (32 байта)
    QByteArray rootKey;

    // Цепочка отправки
    QByteArray sendChainKey;
    quint32    sendMsgNum{0};
    quint32    prevSendMsgNum{0}; // кол-во сообщений, отправленных с предыдущим DH-ключом

    // Цепочка приёма
    QByteArray recvChainKey;
    quint32    recvMsgNum{0};

    // Наша пара DH-ключей
    QByteArray dhPriv;
    QByteArray dhPub;

    // Текущий DH-публичный ключ собеседника
    QByteArray peerDHPub;

    // Кеш ключей пропущенных сообщений: {dhPub, msgNum} → msgKey.
    // Позволяет расшифровать сообщения, доставленные не по порядку.
    QMap<QPair<QByteArray, quint32>, QByteArray> skippedKeys;

    bool initialized{false};
};

struct RatchetMessage {
    QByteArray dhPub;           // текущий DH-публичный ключ отправителя
    quint32    msgNum{0};       // счётчик сообщений в текущей эпохе
    quint32    prevChainLen{0}; // кол-во сообщений, отправленных с предыдущим DH-ключом
    QByteArray ciphertext;      // зашифрованные данные (AES-256-GCM)
    QByteArray nonce;           // GCM nonce (12 байт)
    QByteArray tag;             // GCM тег аутентификации (16 байт)
};

class DoubleRatchet {
public:
    [[nodiscard]] static RatchetState initSender(
        const QByteArray& sharedSecret,
        const QByteArray& peerDHPub);

    [[nodiscard]] static RatchetState initReceiver(
        const QByteArray& sharedSecret,
        const QByteArray& ourDHPriv,
        const QByteArray& ourDHPub);

    [[nodiscard]] static RatchetMessage encrypt(RatchetState& state,
                                                const QByteArray& plaintext);

    [[nodiscard]] static QByteArray decrypt(RatchetState& state,
                                            const RatchetMessage& msg);

    // HKDF-SHA256: публично доступен для деривации ключей вне Double Ratchet
    // (например, для медиа-ключей голосовых звонков в E2EManager).
    [[nodiscard]] static QByteArray hkdf2(const QByteArray& ikm,
                                           const QByteArray& info,
                                           int outLen = 64);

private:
    [[nodiscard]] static QByteArray chainStep(QByteArray& chainKey);
    [[nodiscard]] static QByteArray dhRatchet(RatchetState& state,
                                               const QByteArray& peerDHPub);

    // Сохраняет ключи сообщений с номерами [msgNum, until) в skippedKeys.
    // Продвигает chainKey и msgNum до until (или до достижения лимита).
    static void skipChainKeys(RatchetState& state,
                               QByteArray& chainKey,
                               const QByteArray& dhPub,
                               quint32& msgNum,
                               quint32 until);
    [[nodiscard]] static QByteArray aesgcmEncrypt(const QByteArray& key,
                                                   const QByteArray& nonce,
                                                   const QByteArray& plaintext,
                                                   QByteArray& outTag);
    [[nodiscard]] static QByteArray aesgcmDecrypt(const QByteArray& key,
                                                   const QByteArray& nonce,
                                                   const QByteArray& ciphertext,
                                                   const QByteArray& tag);
    [[nodiscard]] static bool       generateX25519(QByteArray& priv, QByteArray& pub);
    [[nodiscard]] static QByteArray dh(const QByteArray& priv, const QByteArray& pub);
};
