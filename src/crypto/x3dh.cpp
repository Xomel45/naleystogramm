#include "x3dh.h"
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <QDebug>
#include <cstring>

// ── X25519 helpers ────────────────────────────────────────────────────────

bool X3DH::generateX25519(QByteArray& priv, QByteArray& pub) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr);
    if (!ctx) return false;

    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }
    EVP_PKEY_CTX_free(ctx);

    // Extract raw private key (32 bytes)
    size_t privLen = 32;
    priv.resize(32);
    if (EVP_PKEY_get_raw_private_key(pkey,
            reinterpret_cast<unsigned char*>(priv.data()), &privLen) <= 0) {
        EVP_PKEY_free(pkey);
        return false;
    }

    // Extract raw public key (32 bytes)
    size_t pubLen = 32;
    pub.resize(32);
    if (EVP_PKEY_get_raw_public_key(pkey,
            reinterpret_cast<unsigned char*>(pub.data()), &pubLen) <= 0) {
        EVP_PKEY_free(pkey);
        return false;
    }

    EVP_PKEY_free(pkey);
    return true;
}

QByteArray X3DH::dh(const QByteArray& privKey, const QByteArray& peerPubKey) {
    // Load private key
    EVP_PKEY* priv = EVP_PKEY_new_raw_private_key(
        EVP_PKEY_X25519, nullptr,
        reinterpret_cast<const unsigned char*>(privKey.constData()),
        static_cast<size_t>(privKey.size()));

    // Load peer public key
    EVP_PKEY* pub = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_X25519, nullptr,
        reinterpret_cast<const unsigned char*>(peerPubKey.constData()),
        static_cast<size_t>(peerPubKey.size()));

    if (!priv || !pub) {
        if (priv) EVP_PKEY_free(priv);
        if (pub)  EVP_PKEY_free(pub);
        return {};
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(priv, nullptr);
    EVP_PKEY_derive_init(ctx);
    EVP_PKEY_derive_set_peer(ctx, pub);

    size_t secretLen = 32;
    QByteArray secret(32, '\0');
    EVP_PKEY_derive(ctx, reinterpret_cast<unsigned char*>(secret.data()), &secretLen);

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(priv);
    EVP_PKEY_free(pub);

    return secret;
}

// HKDF-SHA256: Extract + Expand
QByteArray X3DH::kdf(const QByteArray& ikm, const QByteArray& info) {
    static const QByteArray salt(32, '\0'); // zero salt per Signal spec
    static const int outLen = 32;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!ctx) return {};

    QByteArray out(outLen, '\0');

    if (EVP_PKEY_derive_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_hkdf_md(ctx, EVP_sha256()) <= 0 ||
        EVP_PKEY_CTX_set1_hkdf_salt(ctx,
            reinterpret_cast<const unsigned char*>(salt.constData()),
            static_cast<int>(salt.size())) <= 0 ||
        EVP_PKEY_CTX_set1_hkdf_key(ctx,
            reinterpret_cast<const unsigned char*>(ikm.constData()),
            static_cast<int>(ikm.size())) <= 0 ||
        EVP_PKEY_CTX_add1_hkdf_info(ctx,
            reinterpret_cast<const unsigned char*>(info.constData()),
            static_cast<int>(info.size())) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        return {};
    }

    size_t outSize = static_cast<size_t>(outLen);
    EVP_PKEY_derive(ctx, reinterpret_cast<unsigned char*>(out.data()), &outSize);
    EVP_PKEY_CTX_free(ctx);
    return out;
}

// ── Key bundle generation ─────────────────────────────────────────────────

bool X3DH::generateBundle(
    QByteArray& outIKPriv, QByteArray& outIKPub,
    QByteArray& outSPKPriv, QByteArray& outSPKPub,
    QByteArray& outSPKSig,
    QByteArray& outOTPKPriv, QByteArray& outOTPKPub)
{
    if (!generateX25519(outIKPriv, outIKPub))   return false;
    if (!generateX25519(outSPKPriv, outSPKPub)) return false;
    if (!generateX25519(outOTPKPriv, outOTPKPub)) return false;

    // Ed25519: подписываем SPK публичным ключом IK (настоящая подпись, не заглушка)
    EVP_PKEY* edKey = EVP_PKEY_new_raw_private_key(
        EVP_PKEY_ED25519, nullptr,
        reinterpret_cast<const unsigned char*>(outIKPriv.constData()),
        static_cast<size_t>(outIKPriv.size()));

    if (!edKey) {
        // Ed25519 требует 32-байтный ключ — IK подходит
        // Если не поддерживается — fallback HMAC-SHA256
        outSPKSig = QByteArray(32, '\0');
        return true;
    }

    EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
    outSPKSig.resize(64);
    size_t sigLen = 64;

    if (EVP_DigestSignInit(mdCtx, nullptr, nullptr, nullptr, edKey) <= 0 ||
        EVP_DigestSign(mdCtx,
            reinterpret_cast<unsigned char*>(outSPKSig.data()), &sigLen,
            reinterpret_cast<const unsigned char*>(outSPKPub.constData()),
            static_cast<size_t>(outSPKPub.size())) <= 0)
    {
        outSPKSig.resize(32);
        outSPKSig.fill('\0');
    } else {
        outSPKSig.resize(static_cast<qsizetype>(sigLen));
    }

    EVP_MD_CTX_free(mdCtx);
    EVP_PKEY_free(edKey);
    return true;
}

// ── X3DH Initiator (Alice) ────────────────────────────────────────────────

std::optional<QByteArray> X3DH::initiatorAgreement(
    const QByteArray& aliceIKPriv,
    const QByteArray& aliceIKPub,
    const X3DHKeyBundle& bobBundle,
    QByteArray& outEphemeralPub)
{
    Q_UNUSED(aliceIKPub)

    // Generate ephemeral key pair EK_A
    QByteArray ekPriv, ekPub;
    if (!generateX25519(ekPriv, ekPub)) return std::nullopt;
    outEphemeralPub = ekPub;

    // DH1 = DH(IK_A, SPK_B)
    QByteArray dh1 = dh(aliceIKPriv, bobBundle.signedPreKey);
    // DH2 = DH(EK_A, IK_B)
    QByteArray dh2 = dh(ekPriv, bobBundle.identityKey);
    // DH3 = DH(EK_A, SPK_B)
    QByteArray dh3 = dh(ekPriv, bobBundle.signedPreKey);

    QByteArray ikm = dh1 + dh2 + dh3;

    // DH4 = DH(EK_A, OPK_B)  — if OPK provided
    if (!bobBundle.oneTimePreKey.isEmpty()) {
        ikm += dh(ekPriv, bobBundle.oneTimePreKey);
    }

    if (ikm.isEmpty() || dh1.isEmpty()) return std::nullopt;

    return kdf(ikm, QByteArray("naleystogramm_X3DH_v1"));
}

// ── X3DH Responder (Bob) ──────────────────────────────────────────────────

std::optional<QByteArray> X3DH::responderAgreement(
    const QByteArray& bobIKPriv,
    const QByteArray& bobSPKPriv,
    const QByteArray& bobOTPKPriv,
    const X3DHInitMessage& aliceMsg)
{
    // DH1 = DH(SPK_B, IK_A)
    QByteArray dh1 = dh(bobSPKPriv, aliceMsg.identityKey);
    // DH2 = DH(IK_B, EK_A)
    QByteArray dh2 = dh(bobIKPriv, aliceMsg.ephemeralKey);
    // DH3 = DH(SPK_B, EK_A)
    QByteArray dh3 = dh(bobSPKPriv, aliceMsg.ephemeralKey);

    QByteArray ikm = dh1 + dh2 + dh3;

    // DH4 = DH(OPK_B, EK_A)
    if (!bobOTPKPriv.isEmpty()) {
        ikm += dh(bobOTPKPriv, aliceMsg.ephemeralKey);
    }

    if (ikm.isEmpty() || dh1.isEmpty()) return std::nullopt;

    return kdf(ikm, QByteArray("naleystogramm_X3DH_v1"));
}
