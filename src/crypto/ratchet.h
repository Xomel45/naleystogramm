#pragma once
#include <QByteArray>
#include <QMap>

// Double Ratchet Algorithm (per-message E2E encryption)
// After X3DH establishes a shared secret, Double Ratchet
// provides forward secrecy and break-in recovery.
//
// Each message uses a unique key derived from the ratchet chain.

struct RatchetState {
    // Root key (32 bytes)
    QByteArray rootKey;

    // Sending chain
    QByteArray sendChainKey;
    quint32    sendMsgNum{0};

    // Receiving chain
    QByteArray recvChainKey;
    quint32    recvMsgNum{0};

    // Our DH ratchet key pair
    QByteArray dhPriv;
    QByteArray dhPub;

    // Their current DH ratchet public key
    QByteArray peerDHPub;

    bool initialized{false};
};

struct RatchetMessage {
    QByteArray dhPub;        // our current DH ratchet public key
    quint32    msgNum{0};    // message counter
    QByteArray ciphertext;   // AES-256-GCM encrypted payload
    QByteArray nonce;        // GCM nonce (12 bytes)
    QByteArray tag;          // GCM auth tag (16 bytes)
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

private:
    [[nodiscard]] static QByteArray chainStep(QByteArray& chainKey);
    [[nodiscard]] static QByteArray dhRatchet(RatchetState& state,
                                               const QByteArray& peerDHPub);
    [[nodiscard]] static QByteArray hkdf2(const QByteArray& ikm,
                                           const QByteArray& info,
                                           int outLen = 64);
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
