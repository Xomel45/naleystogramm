#include "mediaengine.h"
#include <QUdpSocket>
#include <QTimer>
#include <QDebug>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <cstring>

#ifdef HAVE_QT_MULTIMEDIA
#include <QAudioSource>
#include <QAudioSink>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QMediaDevices>
#endif

#ifdef HAVE_OPUS
#include <opus/opus.h>
#endif

// ── Opus pImpl ────────────────────────────────────────────────────────────────
#ifdef HAVE_OPUS
struct MediaEngine::OpusState {
    OpusEncoder* encoder {nullptr};
    OpusDecoder* decoder {nullptr};

    bool init(int sampleRate, int channels) {
        int err = 0;
        encoder = opus_encoder_create(sampleRate, channels, OPUS_APPLICATION_VOIP, &err);
        if (err != OPUS_OK || !encoder) return false;
        // 32 кбит/с — лучшее качество речи при умеренном трафике
        opus_encoder_ctl(encoder, OPUS_SET_BITRATE(32000));
        opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(5));
        opus_encoder_ctl(encoder, OPUS_SET_INBAND_FEC(1));   // Forward Error Correction
        opus_encoder_ctl(encoder, OPUS_SET_DTX(1));          // Discontinuous Transmission (тишина не кодируется)

        decoder = opus_decoder_create(sampleRate, channels, &err);
        if (err != OPUS_OK || !decoder) {
            opus_encoder_destroy(encoder); encoder = nullptr;
            return false;
        }
        return true;
    }

    ~OpusState() {
        if (encoder) { opus_encoder_destroy(encoder); encoder = nullptr; }
        if (decoder) { opus_decoder_destroy(decoder); decoder = nullptr; }
    }
};
#endif

// ── Конструктор / деструктор ─────────────────────────────────────────────────

MediaEngine::MediaEngine(QObject* parent) : QObject(parent) {}

MediaEngine::~MediaEngine() {
    endCall();
}

// ── startCall ────────────────────────────────────────────────────────────────

bool MediaEngine::startCall(const QHostAddress& peerIp, quint16 peerUdpPort,
                             const QByteArray& mediaKey)
{
    if (m_inCall) return false;
    if (mediaKey.size() != 32) {
        emit mediaError("Неверный размер медиа-ключа");
        return false;
    }

#ifndef HAVE_OPUS
    emit mediaError("libopus не найден — голосовые звонки недоступны");
    return false;
#endif

#ifndef HAVE_QT_MULTIMEDIA
    emit mediaError("Qt6Multimedia не найден — голосовые звонки недоступны");
    return false;
#endif

    m_peerIp       = peerIp;
    m_peerUdpPort  = peerUdpPort;
    m_mediaKey     = mediaKey;
    m_seqNum       = 0;

    // ── UDP сокет ─────────────────────────────────────────────────────────────
    m_udpSocket = new QUdpSocket(this);
    if (!m_udpSocket->bind(QHostAddress::Any, 0)) {
        emit mediaError(QString("Не удалось открыть UDP сокет: %1")
                        .arg(m_udpSocket->errorString()));
        delete m_udpSocket; m_udpSocket = nullptr;
        return false;
    }
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &MediaEngine::onReadyRead);

#ifdef HAVE_OPUS
    // ── Opus кодек ────────────────────────────────────────────────────────────
    m_opus = new OpusState;
    if (!m_opus->init(kSampleRate, kChannels)) {
        emit mediaError("Не удалось инициализировать Opus кодек");
        delete m_opus; m_opus = nullptr;
        delete m_udpSocket; m_udpSocket = nullptr;
        return false;
    }
#endif

#ifdef HAVE_QT_MULTIMEDIA
    // ── Аудио формат: PCM 16-bit, 16000 Гц, моно ─────────────────────────────
    QAudioFormat fmt;
    fmt.setSampleRate(kSampleRate);
    fmt.setChannelCount(kChannels);
    fmt.setSampleFormat(QAudioFormat::Int16);

    // ── Захват (микрофон) ─────────────────────────────────────────────────────
    const QAudioDevice inputDev = QMediaDevices::defaultAudioInput();
    if (inputDev.isNull()) {
        emit mediaError("Микрофон не найден");
        endCall();
        return false;
    }
    m_capture = new QAudioSource(inputDev, fmt, this);
    m_captureDevice = m_capture->start();
    if (m_capture->error() != QAudio::NoError) {
        emit mediaError(QString("Ошибка запуска микрофона: %1")
                        .arg(static_cast<int>(m_capture->error())));
        endCall();
        return false;
    }

    // ── Воспроизведение (динамики) ────────────────────────────────────────────
    const QAudioDevice outputDev = QMediaDevices::defaultAudioOutput();
    if (!outputDev.isNull()) {
        m_playback = new QAudioSink(outputDev, fmt, this);
        m_playbackDevice = m_playback->start();
    }
#endif

    // ── Таймер захвата: каждые kFrameMs мс читаем PCM-кадр ───────────────────
    m_captureTimer = new QTimer(this);
    m_captureTimer->setInterval(kFrameMs);
    connect(m_captureTimer, &QTimer::timeout, this, &MediaEngine::onCaptureTimer);
    m_captureTimer->start();

    // ── Таймер воспроизведения (jitter-буфер): отдельный от захвата ──────────
    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setInterval(kFrameMs);
    connect(m_playbackTimer, &QTimer::timeout, this, &MediaEngine::onPlaybackTimer);
    m_playbackTimer->start();

    m_inCall = true;
    qDebug("[MediaEngine] Звонок начат → UDP %s:%d",
           qPrintable(peerIp.toString()), peerUdpPort);
    return true;
}

// ── endCall ──────────────────────────────────────────────────────────────────

void MediaEngine::endCall() {
    if (!m_inCall && !m_captureTimer && !m_udpSocket) return;
    m_inCall = false;

    if (m_captureTimer) {
        m_captureTimer->stop();
        m_captureTimer->deleteLater();
        m_captureTimer = nullptr;
    }
    if (m_playbackTimer) {
        m_playbackTimer->stop();
        m_playbackTimer->deleteLater();
        m_playbackTimer = nullptr;
    }
    m_playbackQueue.clear();

#ifdef HAVE_QT_MULTIMEDIA
    if (m_capture) {
        m_capture->stop();
        m_capture->deleteLater();
        m_capture = nullptr;
        m_captureDevice = nullptr;
    }
    if (m_playback) {
        m_playback->stop();
        m_playback->deleteLater();
        m_playback = nullptr;
        m_playbackDevice = nullptr;
    }
#endif

#ifdef HAVE_OPUS
    delete m_opus;
    m_opus = nullptr;
#endif

    if (m_udpSocket) {
        m_udpSocket->close();
        m_udpSocket->deleteLater();
        m_udpSocket = nullptr;
    }

    m_mediaKey.fill(0);
    m_mediaKey.clear();
    emit audioLevelChanged(0.0f);
    qDebug("[MediaEngine] Звонок завершён");
}

// ── localUdpPort ─────────────────────────────────────────────────────────────

quint16 MediaEngine::localUdpPort() const {
    return m_udpSocket ? m_udpSocket->localPort() : 0;
}

// ── setMuted ─────────────────────────────────────────────────────────────────

void MediaEngine::setMuted(bool muted) {
    m_muted = muted;
}

// ── onCaptureTimer ────────────────────────────────────────────────────────────
// Захватываем ровно один кадр kFrameSamples (320 сэмплов * 2 байта = 640 байт),
// кодируем Opus, шифруем, отправляем UDP.

void MediaEngine::onCaptureTimer() {
#ifndef HAVE_OPUS
    return;
#else
    if (!m_captureDevice || !m_udpSocket) return;

    const int wantBytes = kFrameSamples * 2;  // int16 → 2 байта/сэмпл
    QByteArray pcm = m_captureDevice->read(wantBytes);

    // Дополняем тишиной если данных меньше ожидаемого (начало потока)
    if (pcm.size() < wantBytes)
        pcm.append(wantBytes - pcm.size(), '\0');

    // Заглушка: заменяем звук на тишину
    if (m_muted)
        pcm.fill('\0');

    // Уровень звука для UI (среднеквадратичное по абсолютным значениям)
    const auto* samples = reinterpret_cast<const qint16*>(pcm.constData());
    qint64 sum = 0;
    for (int i = 0; i < kFrameSamples; ++i)
        sum += qAbs(static_cast<qint32>(samples[i]));
    emit audioLevelChanged(qMin(static_cast<float>(sum) / kFrameSamples / 32768.0f * 3.0f, 1.0f));

#ifdef HAVE_OPUS
    if (!m_opus || !m_opus->encoder) return;
    // Кодируем PCM → Opus (буфер 4096 байт — более чем достаточно для кадра 20 мс)
    QByteArray opusFrame(4096, '\0');
    const int encodedBytes = opus_encode(
        m_opus->encoder,
        reinterpret_cast<const opus_int16*>(pcm.constData()),
        kFrameSamples,
        reinterpret_cast<unsigned char*>(opusFrame.data()),
        opusFrame.size()
    );
    if (encodedBytes <= 0) {
        qWarning("[MediaEngine] Ошибка Opus encode: %d", encodedBytes);
        return;
    }
    opusFrame.resize(encodedBytes);
#else
    // Запасной вариант: отправляем сырой PCM (только без libopus, не должно случаться)
    const QByteArray& opusFrame = pcm;
#endif

    const QByteArray packet = encryptPacket(opusFrame);
    if (packet.isEmpty()) return;

    if (m_udpRelayMode) {
        // Relay: добавляем UUID-префикс [16B target UUID][16B source UUID][пакет]
        QByteArray relayPacket;
        relayPacket.reserve(kRelayUuidPrefixSize + packet.size());
        relayPacket.append(m_peerUuidBytes); // куда
        relayPacket.append(m_myUuidBytes);   // от кого
        relayPacket.append(packet);
        m_udpSocket->writeDatagram(relayPacket, m_relayUdpAddr, m_relayUdpPort);
    } else {
        m_udpSocket->writeDatagram(packet, m_peerIp, m_peerUdpPort);
    }
#endif // HAVE_OPUS
}

// ── onReadyRead ────────────────────────────────────────────────────────────────
// Читаем входящие UDP датаграммы, расшифровываем, декодируем Opus, воспроизводим.

void MediaEngine::onReadyRead() {
    while (m_udpSocket && m_udpSocket->hasPendingDatagrams()) {
        QHostAddress sender;
        quint16 senderPort = 0;
        QByteArray raw(m_udpSocket->pendingDatagramSize(), '\0');
        m_udpSocket->readDatagram(raw.data(), raw.size(), &sender, &senderPort);

        // Relay режим: проверяем и снимаем UUID-префикс
        if (m_udpRelayMode) {
            if (raw.size() < kRelayUuidPrefixSize + kMinPktSize) continue;
            // Первые 16 байт — UUID получателя (нас), следующие 16 — UUID отправителя (пира)
            const QByteArray srcUuid = raw.mid(16, 16);
            if (srcUuid != m_peerUuidBytes) continue; // пакет не от нашего пира
            raw = raw.mid(kRelayUuidPrefixSize);
        }

        if (raw.size() > kMaxPacketSize || raw.size() < kMinPktSize) continue;

        const QByteArray opusFrame = decryptPacket(raw);
        if (opusFrame.isEmpty()) continue;   // ошибка аутентификации

#ifdef HAVE_OPUS
        if (!m_opus || !m_opus->decoder) continue;
        // Декодируем Opus → PCM и кладём в jitter-буфер
        QByteArray pcmOut(kFrameSamples * 2, '\0');
        const int decoded = opus_decode(
            m_opus->decoder,
            reinterpret_cast<const unsigned char*>(opusFrame.constData()),
            opusFrame.size(),
            reinterpret_cast<opus_int16*>(pcmOut.data()),
            kFrameSamples,
            0   // fec=0
        );
        if (decoded <= 0) {
            qWarning("[MediaEngine] Ошибка Opus decode: %d", decoded);
            continue;
        }
        pcmOut.resize(decoded * 2);
        if (m_playbackQueue.size() < kJitterBufferMax)
            m_playbackQueue.enqueue(pcmOut);
#endif
    }
}

// ── onPlaybackTimer ───────────────────────────────────────────────────────────
// Каждые kFrameMs мс берём кадр из jitter-буфера или генерируем PLC (если пусто).

void MediaEngine::onPlaybackTimer() {
#ifdef HAVE_OPUS
    if (!m_opus || !m_opus->decoder) return;

    if (!m_playbackQueue.isEmpty()) {
        // Обычный путь: воспроизводим кадр из буфера
        playPcm(m_playbackQueue.dequeue());
    } else {
        // Jitter-буфер пуст — используем Opus PLC (Packet Loss Concealment)
        QByteArray pcmPlc(kFrameSamples * 2, '\0');
        const int decoded = opus_decode(
            m_opus->decoder,
            nullptr, 0,  // nullptr = PLC
            reinterpret_cast<opus_int16*>(pcmPlc.data()),
            kFrameSamples,
            0
        );
        if (decoded > 0) {
            pcmPlc.resize(decoded * 2);
            playPcm(pcmPlc);
        }
    }
#endif
}

// ── enableUdpRelay ────────────────────────────────────────────────────────────

void MediaEngine::enableUdpRelay(const QString& relayIp, quint16 relayUdpPort,
                                  const QUuid& myUuid, const QUuid& peerUuid)
{
    m_udpRelayMode  = true;
    m_relayUdpAddr  = QHostAddress(relayIp);
    m_relayUdpPort  = relayUdpPort;
    m_myUuidBytes   = myUuid.toRfc4122();   // 16 байт
    m_peerUuidBytes = peerUuid.toRfc4122(); // 16 байт
    qDebug("[MediaEngine] Relay UDP включён → %s:%d", qPrintable(relayIp), relayUdpPort);
}

// ── playPcm ───────────────────────────────────────────────────────────────────

void MediaEngine::playPcm(const QByteArray& pcm) {
#ifdef HAVE_QT_MULTIMEDIA
    if (m_playbackDevice)
        m_playbackDevice->write(pcm);
#else
    Q_UNUSED(pcm)
#endif
}

// ── encryptPacket ─────────────────────────────────────────────────────────────
// Формат пакета:
//   [0..3]  seq_num      (uint32 big-endian)
//   [4]     payload_type (0x01 = Opus)
//   [5..16] nonce        (12 байт случайных)
//   [17..20] ciphertext_len (uint32 big-endian)
//   [21..21+N-1] ciphertext
//   [21+N..21+N+15] GCM tag

QByteArray MediaEngine::encryptPacket(const QByteArray& opusFrame) {
    // Генерируем случайный nonce
    QByteArray nonce(kNonceSize, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(nonce.data()), kNonceSize) != 1)
        return {};

    QByteArray tag;
    const QByteArray ciphertext = aesGcmEncrypt(m_mediaKey, nonce, opusFrame, tag);
    if (ciphertext.isEmpty()) return {};

    QByteArray pkt;
    pkt.reserve(kOffData + ciphertext.size() + kTagSize);

    // seq_num (big-endian uint32)
    const quint32 seq = ++m_seqNum;
    pkt.append(static_cast<char>((seq >> 24) & 0xFF));
    pkt.append(static_cast<char>((seq >> 16) & 0xFF));
    pkt.append(static_cast<char>((seq >>  8) & 0xFF));
    pkt.append(static_cast<char>( seq        & 0xFF));
    // payload_type
    pkt.append(static_cast<char>(kPayloadOpus));
    // nonce
    pkt.append(nonce);
    // ciphertext_len (big-endian uint32)
    const quint32 clen = static_cast<quint32>(ciphertext.size());
    pkt.append(static_cast<char>((clen >> 24) & 0xFF));
    pkt.append(static_cast<char>((clen >> 16) & 0xFF));
    pkt.append(static_cast<char>((clen >>  8) & 0xFF));
    pkt.append(static_cast<char>( clen        & 0xFF));
    // ciphertext + tag
    pkt.append(ciphertext);
    pkt.append(tag);
    return pkt;
}

// ── decryptPacket ─────────────────────────────────────────────────────────────

QByteArray MediaEngine::decryptPacket(const QByteArray& raw) {
    if (raw.size() < kMinPktSize) return {};

    // Читаем ciphertext_len
    const quint32 clen =
        (static_cast<quint32>(static_cast<quint8>(raw[kOffClen    ])) << 24) |
        (static_cast<quint32>(static_cast<quint8>(raw[kOffClen + 1])) << 16) |
        (static_cast<quint32>(static_cast<quint8>(raw[kOffClen + 2])) <<  8) |
         static_cast<quint32>(static_cast<quint8>(raw[kOffClen + 3]));

    // Проверяем, что пакет не обрезан
    const int expectedSize = kOffData + static_cast<int>(clen) + kTagSize;
    if (raw.size() < expectedSize) return {};

    const QByteArray nonce      = raw.mid(kOffNonce, kNonceSize);
    const QByteArray ciphertext = raw.mid(kOffData, static_cast<int>(clen));
    const QByteArray tag        = raw.mid(kOffData + static_cast<int>(clen), kTagSize);

    return aesGcmDecrypt(m_mediaKey, nonce, ciphertext, tag);
}

// ── AES-256-GCM (аналог ratchet.cpp, без Double Ratchet зависимостей) ────────

QByteArray MediaEngine::aesGcmEncrypt(const QByteArray& key,
                                       const QByteArray& nonce,
                                       const QByteArray& plaintext,
                                       QByteArray& outTag)
{
    outTag.clear();
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray ciphertext(plaintext.size(), '\0');
    QByteArray tag(kTagSize, '\0');
    int outLen = 0, finalLen = 0;
    bool ok = false;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceSize, nullptr) == 1 &&
        EVP_EncryptInit_ex(ctx, nullptr, nullptr,
            reinterpret_cast<const unsigned char*>(key.constData()),
            reinterpret_cast<const unsigned char*>(nonce.constData())) == 1 &&
        EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()),
            &outLen,
            reinterpret_cast<const unsigned char*>(plaintext.constData()),
            plaintext.size()) == 1 &&
        EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()) + outLen,
            &finalLen) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagSize,
            reinterpret_cast<unsigned char*>(tag.data())) == 1)
    {
        ciphertext.resize(outLen + finalLen);
        outTag = tag;
        ok = true;
    }

    EVP_CIPHER_CTX_free(ctx);
    return ok ? ciphertext : QByteArray{};
}

QByteArray MediaEngine::aesGcmDecrypt(const QByteArray& key,
                                       const QByteArray& nonce,
                                       const QByteArray& ciphertext,
                                       const QByteArray& tag)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray plaintext(ciphertext.size(), '\0');
    int outLen = 0, finalLen = 0;
    bool ok = false;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceSize, nullptr) == 1 &&
        EVP_DecryptInit_ex(ctx, nullptr, nullptr,
            reinterpret_cast<const unsigned char*>(key.constData()),
            reinterpret_cast<const unsigned char*>(nonce.constData())) == 1 &&
        EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plaintext.data()),
            &outLen,
            reinterpret_cast<const unsigned char*>(ciphertext.constData()),
            ciphertext.size()) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagSize,
            const_cast<unsigned char*>(
                reinterpret_cast<const unsigned char*>(tag.constData()))) == 1 &&
        EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plaintext.data()) + outLen,
            &finalLen) == 1)
    {
        plaintext.resize(outLen + finalLen);
        ok = true;
    }

    EVP_CIPHER_CTX_free(ctx);
    return ok ? plaintext : QByteArray{};
}
