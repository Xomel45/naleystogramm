#pragma once
#include <QObject>
#include <QHostAddress>
#include <QQueue>
#include <QUuid>

#ifdef HAVE_QT_MULTIMEDIA
class QAudioSource;
class QAudioSink;
class QIODevice;
#endif
class QUdpSocket;
class QTimer;

// ── MediaEngine ───────────────────────────────────────────────────────────────
// Реализует аудио-пайплайн для голосовых звонков:
//   Захват PCM → Opus encode → AES-256-GCM encrypt → UDP send
//   UDP recv  → AES-256-GCM decrypt → Opus decode → PCM воспроизведение
//
// Зависимости:
//   HAVE_QT_MULTIMEDIA — Qt6::Multimedia (QAudioSource/QAudioSink)
//   HAVE_OPUS          — libopus (кодек Opus)
//
// Без HAVE_OPUS все методы — no-op, кнопка звонка неактивна в UI.
class MediaEngine : public QObject {
    Q_OBJECT
public:
    explicit MediaEngine(QObject* parent = nullptr);
    ~MediaEngine() override;

    // Начать звонок: привязать UDP-сокет, запустить захват и воспроизведение.
    // peerIp/peerUdpPort — куда отправлять пакеты.
    // mediaKey — 32 байта AES-256-GCM ключа (из E2EManager::snapshotMediaKey).
    // Возвращает true при успехе.
    bool startCall(const QHostAddress& peerIp, quint16 peerUdpPort,
                   const QByteArray& mediaKey);
    void endCall();

    [[nodiscard]] bool    isInCall()      const { return m_inCall; }
    // Порт нашего QUdpSocket (после bind) — отправляется в CALL_INVITE/CALL_ACCEPT.
    [[nodiscard]] quint16 localUdpPort()  const;

    // Заглушить микрофон (отправляем тишину вместо реального звука).
    void setMuted(bool muted);
    [[nodiscard]] bool isMuted() const { return m_muted; }

    // Включить UDP-ретрансляцию: пакеты идут через relay-сервер с UUID-префиксом.
    // Вызывается до startCall() из CallManager в режиме ClientServer.
    void enableUdpRelay(const QString& relayIp, quint16 relayUdpPort,
                        const QUuid& myUuid, const QUuid& peerUuid);

signals:
    // Уровень захваченного звука (0.0–1.0), обновляется каждые 20 мс — для индикатора в UI.
    void audioLevelChanged(float level);
    // Ошибка медиадвижка (нет микрофона, ошибка кодека и т.п.)
    void mediaError(const QString& msg);

private slots:
    void onReadyRead();       // входящие UDP датаграммы
    void onCaptureTimer();    // захват кадра PCM каждые kFrameMs мс
    void onPlaybackTimer();   // вывод кадра из jitter-буфера (или PLC) каждые kFrameMs мс

private:
    // Упаковать Opus-кадр в UDP пакет: [seq|type|nonce|len|ciphertext|tag]
    QByteArray encryptPacket(const QByteArray& opusFrame);
    // Распаковать и расшифровать входящий UDP пакет → Opus-кадр.
    // Возвращает пустой массив при ошибке аутентификации.
    QByteArray decryptPacket(const QByteArray& raw);
    // Записать PCM-данные в аудиовыход.
    void       playPcm(const QByteArray& pcm);

    // Вспомогательные AES-256-GCM (независимы от Double Ratchet — для медиапакетов)
    static QByteArray aesGcmEncrypt(const QByteArray& key,
                                    const QByteArray& nonce,
                                    const QByteArray& plaintext,
                                    QByteArray& outTag);
    static QByteArray aesGcmDecrypt(const QByteArray& key,
                                    const QByteArray& nonce,
                                    const QByteArray& ciphertext,
                                    const QByteArray& tag);

    QUdpSocket*  m_udpSocket   {nullptr};
    QHostAddress m_peerIp;
    quint16      m_peerUdpPort {0};
    QByteArray   m_mediaKey;          // 32 байта

#ifdef HAVE_QT_MULTIMEDIA
    QAudioSource* m_capture        {nullptr};
    QAudioSink*   m_playback       {nullptr};
    QIODevice*    m_captureDevice  {nullptr};
    QIODevice*    m_playbackDevice {nullptr};
#endif

#ifdef HAVE_OPUS
    struct OpusState;   // pImpl — скрываем opus/opus.h из заголовка
    OpusState* m_opus {nullptr};
#endif

    QTimer*  m_captureTimer  {nullptr};
    QTimer*  m_playbackTimer {nullptr};  // отдельный таймер вывода (jitter-буфер)
    quint32  m_seqNum        {0};
    bool     m_inCall        {false};
    bool     m_muted         {false};

    // Jitter-буфер: декодированные PCM кадры ждут своей очереди
    QQueue<QByteArray> m_playbackQueue;

    // Relay UDP: пакеты проходят через ретранслятор с UUID-префиксом
    bool         m_udpRelayMode  {false};
    QHostAddress m_relayUdpAddr;
    quint16      m_relayUdpPort  {0};
    QByteArray   m_myUuidBytes;    // 16 байт (QUuid::toRfc4122)
    QByteArray   m_peerUuidBytes; // 16 байт (QUuid::toRfc4122)

    // Формат аудио: 16-bit PCM, 16000 Гц, моно (совместим с AudioRecorder)
    static constexpr int kSampleRate   = 16000;
    static constexpr int kChannels     = 1;
    static constexpr int kFrameMs      = 20;   // 20 мс — стандартный Opus фрейм
    static constexpr int kFrameSamples = kSampleRate * kFrameMs / 1000; // 320
    static constexpr int kMaxPacketSize = 4096; // защита от аномально больших датаграмм
    static constexpr int kNonceSize    = 12;   // GCM nonce
    static constexpr int kTagSize      = 16;   // GCM auth tag
    // Смещения полей UDP пакета
    static constexpr int kOffSeq       = 0;    // 4 байта
    static constexpr int kOffType      = 4;    // 1 байт
    static constexpr int kOffNonce     = 5;    // 12 байт
    static constexpr int kOffClen      = 17;   // 4 байта (длина ciphertext)
    static constexpr int kOffData      = 21;   // ciphertext, далее 16 байт тег
    static constexpr int kMinPktSize   = kOffData + kTagSize; // 37 байт
    static constexpr quint8 kPayloadOpus   = 0x01;
    // Relay UDP: UUID-префикс (16 байт target + 16 байт source)
    static constexpr int kRelayUuidPrefixSize = 32;
    // Jitter-буфер: максимум 20 кадров (~400 мс задержки в худшем случае)
    static constexpr int kJitterBufferMax = 20;
};
