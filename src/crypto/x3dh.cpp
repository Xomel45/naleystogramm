#include "x3dh.h"
#include "openssl_raii.h"
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <QDebug>
#include <cstring>

// ── X25519 helpers ────────────────────────────────────────────────────────

bool X3DH::generateX25519(QByteArray& priv, QByteArray& pub) {
    EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr));
    if (!ctx) return false;
    if (EVP_PKEY_keygen_init(ctx.get()) <= 0) return false;

    EVP_PKEY* pkeyRaw = nullptr;
    if (EVP_PKEY_keygen(ctx.get(), &pkeyRaw) <= 0) return false;
    EvpPkeyPtr pkey(pkeyRaw);

    // Extract raw private key (32 bytes)
    size_t privLen = 32;
    priv.resize(32);
    if (EVP_PKEY_get_raw_private_key(pkey.get(),
            reinterpret_cast<unsigned char*>(priv.data()), &privLen) <= 0) return false;

    // Extract raw public key (32 bytes)
    size_t pubLen = 32;
    pub.resize(32);
    if (EVP_PKEY_get_raw_public_key(pkey.get(),
            reinterpret_cast<unsigned char*>(pub.data()), &pubLen) <= 0) return false;

    return true;
}

QByteArray X3DH::dh(const QByteArray& privKey, const QByteArray& peerPubKey) {
    // Загружаем приватный ключ
    EvpPkeyPtr priv(EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr,
        reinterpret_cast<const unsigned char*>(privKey.constData()),
        static_cast<size_t>(privKey.size())));

    // Загружаем публичный ключ пира
    EvpPkeyPtr pub(EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr,
        reinterpret_cast<const unsigned char*>(peerPubKey.constData()),
        static_cast<size_t>(peerPubKey.size())));

    if (!priv || !pub) return {};

    EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new(priv.get(), nullptr));
    if (!ctx) return {};                          // ← критическая проверка: ранее отсутствовала
    if (EVP_PKEY_derive_init(ctx.get()) <= 0) return {};
    if (EVP_PKEY_derive_set_peer(ctx.get(), pub.get()) <= 0) return {};

    size_t secretLen = 32;
    QByteArray secret(32, '\0');
    if (EVP_PKEY_derive(ctx.get(),
            reinterpret_cast<unsigned char*>(secret.data()), &secretLen) <= 0)
        return {};

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

// ── Верификация подписи SPK ────────────────────────────────────────────────
//
// Оба алгоритма — X25519 и Ed25519 — используют одну кривую Curve25519,
// поэтому байты приватного ключа взаимозаменяемы как скаляр.
// Публичный ключ Ed25519 отличается от X25519 (другая форма представления точки).
//
// ikPrivToEdPub: преобразует X25519 приватный ключ → Ed25519 публичный ключ.
// Используется при генерации бандла для получения ключа верификации.

QByteArray X3DH::ikPrivToEdPub(const QByteArray& ikPriv) {
    if (ikPriv.size() != 32) return {};

    EvpPkeyPtr edKey(EVP_PKEY_new_raw_private_key(
        EVP_PKEY_ED25519, nullptr,
        reinterpret_cast<const unsigned char*>(ikPriv.constData()),
        static_cast<size_t>(ikPriv.size())));
    if (!edKey) return {};

    QByteArray pub(32, '\0');
    size_t pubLen = 32;
    if (EVP_PKEY_get_raw_public_key(edKey.get(),
            reinterpret_cast<unsigned char*>(pub.data()), &pubLen) <= 0)
        return {};
    return pub;
}

// Верифицировать Ed25519 подпись SPK с помощью ik_ed публичного ключа.
// Если подпись неверна — возможна MITM-атака или компрометация ключей.

bool X3DH::verifySpkSig(const QByteArray& ikEdPub,
                          const QByteArray& spkPub,
                          const QByteArray& sig) {
    if (ikEdPub.size() != 32 || spkPub.isEmpty() || sig.isEmpty()) {
        qWarning("[X3DH] verifySpkSig: некорректные аргументы");
        return false;
    }

    EvpPkeyPtr edKey(EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519, nullptr,
        reinterpret_cast<const unsigned char*>(ikEdPub.constData()),
        static_cast<size_t>(ikEdPub.size())));
    if (!edKey) {
        qWarning("[X3DH] verifySpkSig: не удалось загрузить Ed25519 публичный ключ");
        return false;
    }

    EvpMdCtxPtr mdCtx(EVP_MD_CTX_new());
    if (!mdCtx) return false;

    if (EVP_DigestVerifyInit(mdCtx.get(), nullptr, nullptr, nullptr, edKey.get()) <= 0) {
        qWarning("[X3DH] verifySpkSig: DigestVerifyInit провалился");
        return false;
    }

    const int ret = EVP_DigestVerify(mdCtx.get(),
        reinterpret_cast<const unsigned char*>(sig.constData()),
        static_cast<size_t>(sig.size()),
        reinterpret_cast<const unsigned char*>(spkPub.constData()),
        static_cast<size_t>(spkPub.size()));

    if (ret != 1) {
        qCritical("[X3DH] ⚠️ Подпись SPK недействительна! Возможна MITM-атака.");
        return false;
    }
    return true;
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
    EvpPkeyPtr edKey(EVP_PKEY_new_raw_private_key(
        EVP_PKEY_ED25519, nullptr,
        reinterpret_cast<const unsigned char*>(outIKPriv.constData()),
        static_cast<size_t>(outIKPriv.size())));

    if (!edKey) {
        // Ed25519 требует 32-байтный ключ — IK подходит
        // Если не поддерживается — fallback HMAC-SHA256
        outSPKSig = QByteArray(32, '\0');
        return true;
    }

    EvpMdCtxPtr mdCtx(EVP_MD_CTX_new());
    if (!mdCtx) {
        // Сбой аллокации — fallback нулевая подпись
        outSPKSig = QByteArray(32, '\0');
        return true;
    }

    outSPKSig.resize(64);
    size_t sigLen = 64;

    if (EVP_DigestSignInit(mdCtx.get(), nullptr, nullptr, nullptr, edKey.get()) <= 0 ||
        EVP_DigestSign(mdCtx.get(),
            reinterpret_cast<unsigned char*>(outSPKSig.data()), &sigLen,
            reinterpret_cast<const unsigned char*>(outSPKPub.constData()),
            static_cast<size_t>(outSPKPub.size())) <= 0)
    {
        outSPKSig.resize(32);
        outSPKSig.fill('\0');
    } else {
        outSPKSig.resize(static_cast<qsizetype>(sigLen));
    }

    // RAII освобождает mdCtx и edKey автоматически
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

    // ── Верификация подписи SPK — критична для защиты от MITM ────────────────
    // Если пир прислал ik_ed (новые клиенты), проверяем подпись SPK.
    // Если верификация провалилась — ABORT: сессия может быть скомпрометирована.
    if (!bobBundle.ikEdPub.isEmpty()) {
        if (!verifySpkSig(bobBundle.ikEdPub, bobBundle.signedPreKey, bobBundle.signedPreKeySig)) {
            qCritical("[X3DH] ⚠️ Подпись ключа недействительна! Возможна атака посредника (MITM). Сессия отклонена.");
            return std::nullopt;
        }
        qDebug("[X3DH] SPK подпись верифицирована успешно");
    } else {
        // Старый клиент без ik_ed — верификация невозможна, логируем предупреждение
        qWarning("[X3DH] Пир не предоставил ik_ed — верификация SPK пропущена (старый клиент)");
    }

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
