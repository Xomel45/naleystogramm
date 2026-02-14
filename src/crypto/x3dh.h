#pragma once
#include <QByteArray>
#include <optional>

// X3DH (Extended Triple Diffie-Hellman) key agreement
// Used to establish a shared secret between two parties.
//
// Alice (initiator) uses Bob's pre-key bundle to compute:
//   masterSecret = KDF( DH(IK_A, SPK_B) || DH(EK_A, IK_B) ||
//                       DH(EK_A, SPK_B) || DH(EK_A, OPK_B) )
//
// Bob (responder) mirrors the computation with his private keys.
//
// All keys are Curve25519 (X25519) via OpenSSL EVP_PKEY.

struct X3DHKeyBundle {
    QByteArray identityKey;       // IK pub (32 bytes, raw X25519)
    QByteArray signedPreKey;      // SPK pub
    QByteArray signedPreKeySig;   // Ed25519 signature of SPK
    QByteArray oneTimePreKey;     // OPK pub (may be empty)
};

struct X3DHInitMessage {
    QByteArray identityKey;       // IK_A pub
    QByteArray ephemeralKey;      // EK_A pub
    QByteArray usedOtpkId;        // which OPK was consumed (hex id)
    QByteArray initialCiphertext; // first encrypted message payload
};

class X3DH {
public:
    [[nodiscard]] static bool generateBundle(
        QByteArray& outIdentityPriv,
        QByteArray& outIdentityPub,
        QByteArray& outSignedPrePriv,
        QByteArray& outSignedPrePub,
        QByteArray& outSignedPreSig,
        QByteArray& outOtpkPriv,
        QByteArray& outOtpkPub);

    [[nodiscard]] static std::optional<QByteArray> initiatorAgreement(
        const QByteArray& aliceIKPriv,
        const QByteArray& aliceIKPub,
        const X3DHKeyBundle& bobBundle,
        QByteArray& outEphemeralPub);

    [[nodiscard]] static std::optional<QByteArray> responderAgreement(
        const QByteArray& bobIKPriv,
        const QByteArray& bobSPKPriv,
        const QByteArray& bobOTPKPriv,
        const X3DHInitMessage& aliceMsg);

private:
    [[nodiscard]] static QByteArray dh(const QByteArray& privKey, const QByteArray& peerPubKey);
    [[nodiscard]] static QByteArray kdf(const QByteArray& ikm, const QByteArray& info);
    [[nodiscard]] static bool generateX25519(QByteArray& priv, QByteArray& pub);
};
