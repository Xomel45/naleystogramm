#include "ratchet.h"
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <QDebug>

// ── AES-256-GCM ───────────────────────────────────────────────────────────

QByteArray DoubleRatchet::aesgcmEncrypt(const QByteArray& key,
                                         const QByteArray& nonce,
                                         const QByteArray& plaintext,
                                         QByteArray& outTag) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    QByteArray ciphertext(plaintext.size(), '\0');
    outTag.resize(16);
    int len = 0;

    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_EncryptInit_ex(ctx, nullptr, nullptr,
        reinterpret_cast<const unsigned char*>(key.constData()),
        reinterpret_cast<const unsigned char*>(nonce.constData()));

    EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()),
        &len,
        reinterpret_cast<const unsigned char*>(plaintext.constData()),
        plaintext.size());

    EVP_EncryptFinal_ex(ctx,
        reinterpret_cast<unsigned char*>(ciphertext.data()) + len, &len);

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16,
        reinterpret_cast<unsigned char*>(outTag.data()));

    EVP_CIPHER_CTX_free(ctx);
    return ciphertext;
}

QByteArray DoubleRatchet::aesgcmDecrypt(const QByteArray& key,
                                         const QByteArray& nonce,
                                         const QByteArray& ciphertext,
                                         const QByteArray& tag) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    QByteArray plaintext(ciphertext.size(), '\0');
    int len = 0;

    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_DecryptInit_ex(ctx, nullptr, nullptr,
        reinterpret_cast<const unsigned char*>(key.constData()),
        reinterpret_cast<const unsigned char*>(nonce.constData()));

    EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plaintext.data()),
        &len,
        reinterpret_cast<const unsigned char*>(ciphertext.constData()),
        ciphertext.size());

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16,
        const_cast<unsigned char*>(
            reinterpret_cast<const unsigned char*>(tag.constData())));

    int ret = EVP_DecryptFinal_ex(ctx,
        reinterpret_cast<unsigned char*>(plaintext.data()) + len, &len);

    EVP_CIPHER_CTX_free(ctx);

    if (ret <= 0) {
        qWarning("[Ratchet] GCM auth tag mismatch — message tampered!");
        return {};
    }
    return plaintext;
}

// ── X25519 DH ─────────────────────────────────────────────────────────────

bool DoubleRatchet::generateX25519(QByteArray& priv, QByteArray& pub) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr);
    EVP_PKEY_keygen_init(ctx);
    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_keygen(ctx, &pkey);
    EVP_PKEY_CTX_free(ctx);

    size_t len = 32;
    priv.resize(32); pub.resize(32);
    EVP_PKEY_get_raw_private_key(pkey,
        reinterpret_cast<unsigned char*>(priv.data()), &len);
    EVP_PKEY_get_raw_public_key(pkey,
        reinterpret_cast<unsigned char*>(pub.data()), &len);
    EVP_PKEY_free(pkey);
    return true;
}

QByteArray DoubleRatchet::dh(const QByteArray& priv, const QByteArray& pub) {
    EVP_PKEY* pk = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr,
        reinterpret_cast<const unsigned char*>(priv.constData()), 32);
    EVP_PKEY* pp = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr,
        reinterpret_cast<const unsigned char*>(pub.constData()), 32);

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pk, nullptr);
    EVP_PKEY_derive_init(ctx);
    EVP_PKEY_derive_set_peer(ctx, pp);

    size_t secretLen = 32;
    QByteArray secret(32, '\0');
    EVP_PKEY_derive(ctx, reinterpret_cast<unsigned char*>(secret.data()), &secretLen);

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pk); EVP_PKEY_free(pp);
    return secret;
}

// ── HKDF ──────────────────────────────────────────────────────────────────

QByteArray DoubleRatchet::hkdf2(const QByteArray& ikm,
                                  const QByteArray& info, int outLen) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    QByteArray out(outLen, '\0');
    static const QByteArray salt(32, '\0');

    EVP_PKEY_derive_init(ctx);
    EVP_PKEY_CTX_set_hkdf_md(ctx, EVP_sha256());
    EVP_PKEY_CTX_set1_hkdf_salt(ctx,
        reinterpret_cast<const unsigned char*>(salt.constData()), salt.size());
    EVP_PKEY_CTX_set1_hkdf_key(ctx,
        reinterpret_cast<const unsigned char*>(ikm.constData()), ikm.size());
    EVP_PKEY_CTX_add1_hkdf_info(ctx,
        reinterpret_cast<const unsigned char*>(info.constData()), info.size());

    size_t s = static_cast<size_t>(outLen);
    EVP_PKEY_derive(ctx, reinterpret_cast<unsigned char*>(out.data()), &s);
    EVP_PKEY_CTX_free(ctx);
    return out;
}

// ── Chain step: KDF_CK(CK) → (CK', MK) ──────────────────────────────────

QByteArray DoubleRatchet::chainStep(QByteArray& chainKey) {
    // CK' = HMAC-SHA256(CK, 0x02)
    // MK  = HMAC-SHA256(CK, 0x01)
    const QByteArray derived = hkdf2(chainKey, "MsgKey", 64);
    const QByteArray newCK   = derived.left(32);
    const QByteArray msgKey  = derived.right(32);
    chainKey = newCK;
    return msgKey;
}

// ── DH ratchet step ───────────────────────────────────────────────────────

QByteArray DoubleRatchet::dhRatchet(RatchetState& state,
                                     const QByteArray& peerDHPub) {
    // Compute DH output with peer's new key
    QByteArray dhOut = dh(state.dhPriv, peerDHPub);

    // Derive new root key and chain key: RK, CK = KDF_RK(RK, DH)
    const QByteArray combined = state.rootKey + dhOut;
    const QByteArray derived  = hkdf2(combined, "RatchetStep", 64);

    state.rootKey     = derived.left(32);
    state.peerDHPub   = peerDHPub;

    // Generate new DH key pair for next ratchet step
    if (!generateX25519(state.dhPriv, state.dhPub))
        qCritical("[Ratchet] Failed to generate DH key pair!");

    return derived.right(32); // new chain key
}

// ── Init ──────────────────────────────────────────────────────────────────

RatchetState DoubleRatchet::initSender(const QByteArray& sharedSecret,
                                        const QByteArray& peerDHPub) {
    RatchetState s;
    s.rootKey    = sharedSecret;
    s.peerDHPub  = peerDHPub;
    if (!generateX25519(s.dhPriv, s.dhPub))
        qCritical("[Ratchet] Failed to generate initial DH key pair!");

    // Perform initial DH ratchet step to derive send chain
    s.sendChainKey = dhRatchet(s, peerDHPub);
    s.initialized  = true;
    return s;
}

RatchetState DoubleRatchet::initReceiver(const QByteArray& sharedSecret,
                                          const QByteArray& ourDHPriv,
                                          const QByteArray& ourDHPub) {
    RatchetState s;
    s.rootKey    = sharedSecret;
    s.dhPriv     = ourDHPriv;
    s.dhPub      = ourDHPub;
    s.initialized = true;
    return s;
}

// ── Encrypt ───────────────────────────────────────────────────────────────

RatchetMessage DoubleRatchet::encrypt(RatchetState& state,
                                       const QByteArray& plaintext) {
    const QByteArray msgKey = chainStep(state.sendChainKey);

    // Split msgKey: first 32 bytes = AES key, next 12 = nonce seed
    const QByteArray aesKey = msgKey.left(32);
    QByteArray nonce(12, '\0');
    RAND_bytes(reinterpret_cast<unsigned char*>(nonce.data()), 12);

    RatchetMessage msg;
    msg.dhPub  = state.dhPub;
    msg.msgNum = state.sendMsgNum++;
    msg.nonce  = nonce;
    msg.ciphertext = aesgcmEncrypt(aesKey, nonce, plaintext, msg.tag);

    return msg;
}

// ── Decrypt ───────────────────────────────────────────────────────────────

QByteArray DoubleRatchet::decrypt(RatchetState& state,
                                   const RatchetMessage& msg) {
    // If peer's DH pub changed → perform DH ratchet step first
    if (msg.dhPub != state.peerDHPub) {
        state.recvChainKey = dhRatchet(state, msg.dhPub);
        state.recvMsgNum   = 0;
    }

    const QByteArray msgKey = chainStep(state.recvChainKey);
    state.recvMsgNum++;

    const QByteArray aesKey = msgKey.left(32);
    return aesgcmDecrypt(aesKey, msg.nonce, msg.ciphertext, msg.tag);
}
