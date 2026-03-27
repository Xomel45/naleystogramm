#include "ratchet.h"
#include "openssl_raii.h"
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <QDebug>

// Логирует код ошибки OpenSSL и возвращает пустой результат / false.
// Используется для перехвата сбоев EVP_* функций.
static void logOpenSSLError(const char* where) {
    const unsigned long err = ERR_get_error();
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    qCritical("[Ratchet] OpenSSL error in %s: %s (code: %lu)", where, buf, err);
}

// ── AES-256-GCM ───────────────────────────────────────────────────────────

QByteArray DoubleRatchet::aesgcmEncrypt(const QByteArray& key,
                                         const QByteArray& nonce,
                                         const QByteArray& plaintext,
                                         QByteArray& outTag) {
    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        qCritical("[Ratchet] aesgcmEncrypt: EVP_CIPHER_CTX_new() вернул NULL");
        return {};
    }
    QByteArray ciphertext(plaintext.size(), '\0');
    outTag.resize(16);
    int len = 0;

    EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr,
        reinterpret_cast<const unsigned char*>(key.constData()),
        reinterpret_cast<const unsigned char*>(nonce.constData()));

    EVP_EncryptUpdate(ctx.get(), reinterpret_cast<unsigned char*>(ciphertext.data()),
        &len,
        reinterpret_cast<const unsigned char*>(plaintext.constData()),
        plaintext.size());

    EVP_EncryptFinal_ex(ctx.get(),
        reinterpret_cast<unsigned char*>(ciphertext.data()) + len, &len);

    EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16,
        reinterpret_cast<unsigned char*>(outTag.data()));

    // RAII освобождает ctx автоматически
    return ciphertext;
}

QByteArray DoubleRatchet::aesgcmDecrypt(const QByteArray& key,
                                         const QByteArray& nonce,
                                         const QByteArray& ciphertext,
                                         const QByteArray& tag) {
    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        qCritical("[Ratchet] aesgcmDecrypt: EVP_CIPHER_CTX_new() вернул NULL");
        return {};
    }
    QByteArray plaintext(ciphertext.size(), '\0');
    int len = 0;

    EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr,
        reinterpret_cast<const unsigned char*>(key.constData()),
        reinterpret_cast<const unsigned char*>(nonce.constData()));

    EVP_DecryptUpdate(ctx.get(), reinterpret_cast<unsigned char*>(plaintext.data()),
        &len,
        reinterpret_cast<const unsigned char*>(ciphertext.constData()),
        ciphertext.size());

    EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16,
        const_cast<unsigned char*>(
            reinterpret_cast<const unsigned char*>(tag.constData())));

    int ret = EVP_DecryptFinal_ex(ctx.get(),
        reinterpret_cast<unsigned char*>(plaintext.data()) + len, &len);

    // RAII освобождает ctx автоматически

    if (ret <= 0) {
        qWarning("[Ratchet] GCM auth tag mismatch — message tampered!");
        return {};
    }
    return plaintext;
}

// ── X25519 DH ─────────────────────────────────────────────────────────────

bool DoubleRatchet::generateX25519(QByteArray& priv, QByteArray& pub) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr);
    if (!ctx) { logOpenSSLError("generateX25519/ctx_new"); return false; }

    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        logOpenSSLError("generateX25519/keygen_init");
        EVP_PKEY_CTX_free(ctx); return false;
    }

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        logOpenSSLError("generateX25519/keygen");
        EVP_PKEY_CTX_free(ctx); return false;
    }
    EVP_PKEY_CTX_free(ctx);

    size_t len = 32;
    priv.resize(32); pub.resize(32);

    if (EVP_PKEY_get_raw_private_key(pkey,
            reinterpret_cast<unsigned char*>(priv.data()), &len) <= 0) {
        logOpenSSLError("generateX25519/get_priv");
        EVP_PKEY_free(pkey); return false;
    }
    if (EVP_PKEY_get_raw_public_key(pkey,
            reinterpret_cast<unsigned char*>(pub.data()), &len) <= 0) {
        logOpenSSLError("generateX25519/get_pub");
        EVP_PKEY_free(pkey); return false;
    }

    EVP_PKEY_free(pkey);
    return true;
}

QByteArray DoubleRatchet::dh(const QByteArray& priv, const QByteArray& pub) {
    EVP_PKEY* pk = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr,
        reinterpret_cast<const unsigned char*>(priv.constData()), 32);
    EVP_PKEY* pp = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr,
        reinterpret_cast<const unsigned char*>(pub.constData()), 32);

    if (!pk || !pp) {
        logOpenSSLError("dh/new_key");
        if (pk) EVP_PKEY_free(pk);
        if (pp) EVP_PKEY_free(pp);
        return {};
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pk, nullptr);
    if (!ctx) { logOpenSSLError("dh/ctx_new"); EVP_PKEY_free(pk); EVP_PKEY_free(pp); return {}; }

    if (EVP_PKEY_derive_init(ctx) <= 0) {
        logOpenSSLError("dh/derive_init");
        EVP_PKEY_CTX_free(ctx); EVP_PKEY_free(pk); EVP_PKEY_free(pp); return {};
    }
    if (EVP_PKEY_derive_set_peer(ctx, pp) <= 0) {
        logOpenSSLError("dh/derive_set_peer");
        EVP_PKEY_CTX_free(ctx); EVP_PKEY_free(pk); EVP_PKEY_free(pp); return {};
    }

    size_t secretLen = 32;
    QByteArray secret(32, '\0');
    if (EVP_PKEY_derive(ctx,
            reinterpret_cast<unsigned char*>(secret.data()), &secretLen) <= 0) {
        logOpenSSLError("dh/derive");
        EVP_PKEY_CTX_free(ctx); EVP_PKEY_free(pk); EVP_PKEY_free(pp); return {};
    }

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pk); EVP_PKEY_free(pp);
    return secret;
}

// ── HKDF ──────────────────────────────────────────────────────────────────

QByteArray DoubleRatchet::hkdf2(const QByteArray& ikm,
                                  const QByteArray& info, int outLen) {
    // Нулевой соль соответствует спецификации Signal Protocol:
    // доменное разделение обеспечивается параметром info, а не солью.
    static const QByteArray salt(32, '\0');

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!ctx) { logOpenSSLError("hkdf2/ctx_new"); return {}; }

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
        logOpenSSLError("hkdf2/setup");
        EVP_PKEY_CTX_free(ctx);
        return {};
    }

    size_t s = static_cast<size_t>(outLen);
    if (EVP_PKEY_derive(ctx,
            reinterpret_cast<unsigned char*>(out.data()), &s) <= 0) {
        logOpenSSLError("hkdf2/derive");
        EVP_PKEY_CTX_free(ctx);
        return {};
    }

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

// ── Сохранение пропущенных ключей ────────────────────────────────────────
//
// Продвигает chainKey и msgNum от текущей позиции до until,
// сохраняя ключ каждого пропущенного сообщения в state.skippedKeys.
// Используется при получении сообщения с msgNum > ожидаемого, чтобы
// расшифровать задержанные сообщения, которые придут позже.

void DoubleRatchet::skipChainKeys(RatchetState& state,
                                   QByteArray& chainKey,
                                   const QByteArray& dhPub,
                                   quint32& msgNum,
                                   quint32 until) {
    if (until <= msgNum) return;

    // Защита от исчерпания памяти при очень большом пропуске
    if (until - msgNum > kMaxSkippedKeys) {
        qWarning("[Ratchet] skipChainKeys: пропуск %u→%u превышает лимит, обрезаем до %u",
                 msgNum, until, msgNum + kMaxSkippedKeys);
        until = msgNum + kMaxSkippedKeys;
    }

    while (msgNum < until) {
        if (state.skippedKeys.size() >= static_cast<int>(kMaxSkippedKeys)) {
            qWarning("[Ratchet] skipChainKeys: буфер пропущенных ключей переполнен (%lld)",
                     static_cast<long long>(state.skippedKeys.size()));
            break;
        }
        state.skippedKeys[{dhPub, msgNum}] = chainStep(chainKey);
        ++msgNum;
    }
}

// ── DH ratchet step ───────────────────────────────────────────────────────

QByteArray DoubleRatchet::dhRatchet(RatchetState& state,
                                     const QByteArray& peerDHPub) {
    // Шаг 1: CKr = KDF_RK(RK, DH(наш_текущий_ключ, новый_ключ_пира))
    const QByteArray dhOut1   = dh(state.dhPriv, peerDHPub);
    const QByteArray derived1 = hkdf2(state.rootKey + dhOut1, "RatchetStep", 64);
    const QByteArray ckr      = derived1.right(32);
    state.rootKey   = derived1.left(32);
    state.peerDHPub = peerDHPub;

    // Шаг 2: генерируем новую DH-пару для цепочки ответных сообщений
    if (!generateX25519(state.dhPriv, state.dhPub))
        qCritical("[Ratchet] dhRatchet: не удалось сгенерировать DH-пару");

    // Шаг 3: CKs = KDF_RK(RK', DH(новый_наш_ключ, новый_ключ_пира))
    // Инициализирует цепочку отправки — необходима для ответных сообщений
    const QByteArray dhOut2   = dh(state.dhPriv, peerDHPub);
    const QByteArray derived2 = hkdf2(state.rootKey + dhOut2, "RatchetStep", 64);
    state.rootKey      = derived2.left(32);
    state.sendChainKey = derived2.right(32);

    // ДИАГНОСТИКА: CKr (цепочка приёма) у получателя ДОЛЖНА совпадать
    // с CKs (цепочкой отправки) отправителя, вычисленной на том же DH-выходе.
    // Если они расходятся, в логах будут разные значения CKr здесь и CKs в initSender/encrypt.
#ifdef QT_DEBUG
    qDebug("[Ratchet] dhRatchet: наш_DH=%s → peerDH=%s  CKr=%s  CKs=%s  RK=%s",
           state.dhPub.left(4).toHex().constData(),  // наш НОВЫЙ DH (после генерации B1)
           peerDHPub.left(4).toHex().constData(),
           ckr.left(4).toHex().constData(),
           state.sendChainKey.left(4).toHex().constData(),
           state.rootKey.left(4).toHex().constData());
#endif

    return ckr; // цепочка получения
}

// ── Init ──────────────────────────────────────────────────────────────────

RatchetState DoubleRatchet::initSender(const QByteArray& sharedSecret,
                                        const QByteArray& peerDHPub) {
    RatchetState s;
    s.rootKey   = sharedSecret;
    s.peerDHPub = peerDHPub;

    // Генерируем начальную DH-пару (A1) — она уйдёт в заголовок первых сообщений.
    // ВАЖНО: dhRatchet() НЕ вызываем — он бы заменил A1 на A2,
    // после чего получатель использовал бы DH(spkPriv, A2pub), а не DH(spkPriv, A1pub),
    // что сразу разрывает синхронизацию ключей.
    if (!generateX25519(s.dhPriv, s.dhPub))
        qCritical("[Ratchet] initSender: не удалось сгенерировать DH-пару");

    // RK', CKs = KDF_RK(SK, DH(A1_priv, SPK_pub)) — dhPriv/dhPub остаются A1
    const QByteArray dhOut   = dh(s.dhPriv, peerDHPub);
    const QByteArray derived = hkdf2(s.rootKey + dhOut, "RatchetStep", 64);
    s.rootKey      = derived.left(32);
    s.sendChainKey = derived.right(32);
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
    // Фиксируем цепочку ДО шага — получатель должен увидеть то же CK_до в своём Decrypt
#ifdef QT_DEBUG
    const QByteArray ckBefore = state.sendChainKey.left(4).toHex();
#endif
    const QByteArray msgKey   = chainStep(state.sendChainKey);

    const QByteArray aesKey = msgKey.left(32);
    QByteArray nonce(12, '\0');
    RAND_bytes(reinterpret_cast<unsigned char*>(nonce.data()), 12);

    RatchetMessage msg;
    msg.dhPub        = state.dhPub;
    msg.msgNum       = state.sendMsgNum++;
    // Сообщаем получателю, сколько сообщений было отправлено
    // с предыдущим DH-ключом — необходимо для корректного пропуска ключей.
    msg.prevChainLen = state.prevSendMsgNum;
    msg.nonce        = nonce;
    msg.ciphertext   = aesgcmEncrypt(aesKey, nonce, plaintext, msg.tag);

    // КЛЮЧЕВОЙ ЛОГ: CK_до на отправителе ОБЯЗАН совпасть с CK_до в Decrypt на получателе.
    // DH_Step для Encrypt всегда NO — отправитель меняет DH-ключ только когда ПОЛУЧАЕТ ответ.
#ifdef QT_DEBUG
    qDebug("[Ratchet] Encrypt: MsgNum=%u  DH_Step=NO  CK_до=%s  CK_после=%s  MK=%s  DH=%s  pN=%u",
           msg.msgNum,
           ckBefore.constData(),
           state.sendChainKey.left(4).toHex().constData(),
           msgKey.left(4).toHex().constData(),
           state.dhPub.left(4).toHex().constData(),
           msg.prevChainLen);
#endif

    return msg;
}

// ── Decrypt ───────────────────────────────────────────────────────────────

QByteArray DoubleRatchet::decrypt(RatchetState& state,
                                   const RatchetMessage& msg) {
    // 1. Проверяем кеш пропущенных ключей — для сообщений, доставленных не по порядку
    const QPair<QByteArray, quint32> skippedKey{msg.dhPub, msg.msgNum};
    const auto it = state.skippedKeys.find(skippedKey);
    if (it != state.skippedKeys.end()) {
        // Расшифровываем из кеша пропущенных ключей (сообщение доставлено не по порядку)
        const QByteArray msgKey = it.value();
        state.skippedKeys.erase(it);
        const QByteArray aesKey = msgKey.left(32);
        const QByteArray result = aesgcmDecrypt(aesKey, msg.nonce, msg.ciphertext, msg.tag);
#ifdef QT_DEBUG
        qDebug("[Ratchet] Decrypt[пропущен]: MsgNum=%u  MK=%s  DH=%s  %s",
               msg.msgNum,
               msgKey.left(4).toHex().constData(),
               msg.dhPub.left(4).toHex().constData(),
               result.isEmpty() ? "ОШИБКА" : "OK");
#endif
        return result;
    }

    // 2. Новый DH-ключ собеседника → шаг DH-храповика
    const bool didDHRatchet = (msg.dhPub != state.peerDHPub);
    if (didDHRatchet) {
        // Флаг захвачен ДО dhRatchet, потому что dhRatchet обновляет state.peerDHPub.
        // Используем в финальном логе ниже для поля DH_Step.
#ifdef QT_DEBUG
        qDebug("[Ratchet][decrypt] DH-шаг: oldPeer=%s → newPeer=%s  N=%u  pN=%u",
               state.peerDHPub.left(4).toHex().constData(),
               msg.dhPub.left(4).toHex().constData(),
               msg.msgNum, msg.prevChainLen);
#endif

        // Сохраняем ключи оставшихся сообщений старой цепочки.
        // msg.prevChainLen сообщает, сколько всего сообщений было отправлено
        // с предыдущим DH-ключом — они могут прийти позже, вне порядка.
        const QByteArray oldDHPub = state.peerDHPub;
        skipChainKeys(state, state.recvChainKey, oldDHPub,
                      state.recvMsgNum, msg.prevChainLen);

        // Запоминаем длину текущей цепочки отправки и сбрасываем счётчик
        state.prevSendMsgNum = state.sendMsgNum;
        state.sendMsgNum     = 0;  // сброс per Signal Protocol spec (Ns = 0 после DH-шага)

        // Выполняем DH-шаг: обновляет peerDHPub, генерирует новую DH-пару,
        // вычисляет CKr (возврат) и CKs (state.sendChainKey) за два KDF_RK прохода
        state.recvChainKey = dhRatchet(state, msg.dhPub);
        state.recvMsgNum   = 0;

        // Сохраняем пропущенные ключи в новой цепочке до текущего сообщения
        skipChainKeys(state, state.recvChainKey, msg.dhPub,
                      state.recvMsgNum, msg.msgNum);
    } else {
        // Тот же DH-ключ — пропускаем до текущего номера сообщения
        skipChainKeys(state, state.recvChainKey, msg.dhPub,
                      state.recvMsgNum, msg.msgNum);
    }

    // 3. Получаем ключ текущего сообщения
    // КЛЮЧЕВОЙ ЛОГ: CK_до ДОЛЖЕН совпадать с CK_до из Encrypt на отправителе.
    // Если CK_до расходится — ищите, когда цепочки разошлись в dhRatchet/chainStep.
    // didDHRatchet захвачен выше, ДО вызова dhRatchet (который изменяет state.peerDHPub).
#ifdef QT_DEBUG
    const QByteArray ckBefore = state.recvChainKey.left(4).toHex();
#endif
    const QByteArray msgKey       = chainStep(state.recvChainKey);
    state.recvMsgNum++;

    const QByteArray aesKey = msgKey.left(32);
    const QByteArray result = aesgcmDecrypt(aesKey, msg.nonce, msg.ciphertext, msg.tag);

    // DH_Step=YES означает, что dhRatchet был вызван ДО этой точки в текущем вызове decrypt()
#ifdef QT_DEBUG
    qDebug("[Ratchet] Decrypt: MsgNum=%u  DH_Step=%s  CK_до=%s  CK_после=%s  MK=%s  DH=%s  Result=%s",
           msg.msgNum,
           didDHRatchet ? "YES" : "NO",
           ckBefore.constData(),
           state.recvChainKey.left(4).toHex().constData(),
           msgKey.left(4).toHex().constData(),
           msg.dhPub.left(4).toHex().constData(),
           result.isEmpty() ? "FAIL" : "OK");
#endif

    return result;
}
