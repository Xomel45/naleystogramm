#pragma once
#include <QObject>
#include <QUuid>
#include <QJsonObject>
#include <QFile>
#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QTimer>
#include <QFutureWatcher>
#include "network.h"

// Forward declaration чтобы не тянуть весь e2e.h
class E2EManager;

// ── Структура прогресса передачи ─────────────────────────────────────────────
// Используется для обновления UI в реальном времени
struct TransferProgress {
    QString  id;                   // ID передачи
    QString  fileName;             // Имя файла
    qint64   bytesTransferred;     // Переданные байты
    qint64   totalBytes;           // Общий размер файла
    double   speedBytesPerSec;     // Текущая скорость (байт/сек)
    int      etaSeconds;           // Примерное время до завершения
    int      percent;              // Процент выполнения (0-100)
    bool     outgoing;             // true = отправка, false = получение
};

// ── Состояние передачи ───────────────────────────────────────────────────────
enum class TransferState {
    Pending,       // Ожидание подтверждения
    Active,        // Идёт передача
    Paused,        // Приостановлено
    Completed,     // Завершено успешно
    Failed,        // Ошибка
    Cancelled      // Отменено пользователем
};

// ── Информация для возобновления передачи ────────────────────────────────────
// Сериализуется в JSON и сохраняется на диск при разрыве соединения
struct TransferResumeInfo {
    QString   id;                  // ID передачи
    QUuid     peerUuid;            // UUID пира
    QString   fileName;            // Имя файла
    QString   tempFilePath;        // Путь к временному файлу
    qint64    totalSize;           // Полный размер файла
    qint64    lastConfirmedChunk;  // Последний подтверждённый чанк
    QByteArray partialHash;        // Промежуточный хеш (сериализованный)
    QByteArray key;                // Ключ шифрования (32 байта для AES-256)
    QByteArray nonce;              // Базовый nonce (12 байт для GCM)
    bool       outgoing;           // true = отправка, false = получение
};

// ── Исходящая передача (стриминг) ────────────────────────────────────────────
struct OutgoingTransfer {
    QString      id;               // Уникальный ID передачи
    QUuid        peerUuid;         // UUID получателя
    QString      filePath;         // Путь к исходному файлу
    QString      fileName;         // Имя файла для получателя
    qint64       fileSize;         // Размер файла в байтах
    QByteArray   fileHash;         // SHA-256 хеш всего файла
    QByteArray   key;              // Ключ AES-256-GCM (32 байта)
    QByteArray   nonce;            // Базовый nonce (12 байт)
    qint64       bytesSent;        // Отправлено байт
    qint64       chunksSent;       // Отправлено чанков
    QFile*       file;             // Открытый файл для стриминга
    QElapsedTimer timer;           // Таймер для вычисления скорости
    TransferState state;           // Текущее состояние
    qint64       lastSpeedCalcBytes;   // Байты при последнем расчёте скорости
    qint64       lastSpeedCalcTime;    // Время последнего расчёта (мс)
    double       currentSpeed;         // Текущая скорость (байт/сек)
    int          durationMs{0};        // Длительность голосового (0 = обычный файл)
};

// ── Входящая передача (стриминг) ─────────────────────────────────────────────
struct IncomingTransfer {
    QString      id;               // Уникальный ID передачи
    QUuid        peerUuid;         // UUID отправителя
    QString      fileName;         // Имя файла
    QString      tempFilePath;     // Путь к временному файлу
    QString      finalFilePath;    // Путь к финальному файлу
    qint64       fileSize;         // Ожидаемый размер файла
    QByteArray   expectedHash;     // Ожидаемый SHA-256 хеш
    QByteArray   key;              // Ключ AES-256-GCM (32 байта)
    QByteArray   nonce;            // Базовый nonce (12 байт)
    qint64       bytesReceived;    // Получено байт
    qint64       chunksReceived;   // Получено чанков
    QFile*       file;             // Открытый временный файл
    QCryptographicHash* hasher;    // SHA-256 хешер для проверки целостности
    QElapsedTimer timer;           // Таймер для вычисления скорости
    TransferState state;           // Текущее состояние
    qint64       lastSpeedCalcBytes;   // Байты при последнем расчёте скорости
    qint64       lastSpeedCalcTime;    // Время последнего расчёта (мс)
    double       currentSpeed;         // Текущая скорость (байт/сек)
    int          durationMs{0};        // Длительность голосового (0 = обычный файл)
};

// ── FileTransfer ─────────────────────────────────────────────────────────────
// Потоковая передача файлов с AES-256-GCM шифрованием и проверкой хеша.
//
// Протокол:
//   FILE_OFFER    → получатель показывает диалог принятия
//   FILE_ACCEPT   → отправитель начинает стриминг FILE_CHUNK
//   FILE_REJECT   → отправитель удаляет передачу
//   FILE_CHUNK    → зашифрованный чанк с auth tag + nonce
//   FILE_COMPLETE → передача завершена, проверка хеша
//   FILE_CANCEL   → отмена передачи (в любую сторону)
//   FILE_PAUSE    → приостановка передачи
//   FILE_RESUME_REQUEST → запрос на возобновление (от получателя)
//   FILE_RESUME_ACK     → подтверждение возобновления (от отправителя)
//
class FileTransfer : public QObject {
    Q_OBJECT
public:
    explicit FileTransfer(NetworkManager* network, E2EManager* e2e,
                          QObject* parent = nullptr);
    ~FileTransfer();

    // Отправка файла пиру (стриминг).
    // durationMs > 0 = голосовое сообщение: поле duration_ms добавляется в FILE_OFFER,
    // получатель принимает автоматически без диалога.
    void sendFile(const QUuid& peerUuid, const QString& filePath, int durationMs = 0);

    // Обработка входящих сообщений протокола
    void handleMessage(const QUuid& from, const QJsonObject& msg);

    // Получить текущий прогресс передачи
    [[nodiscard]] TransferProgress getProgress(const QString& transferId) const;

    // Проверить наличие незавершённых передач для пира
    [[nodiscard]] bool hasPendingTransfers(const QUuid& peerUuid) const;

signals:
    // Входящее предложение файла.
    // durationMs > 0 = голосовое сообщение → принимать автоматически без диалога.
    void fileOffer(QUuid from, QString name, qint64 size, QString offerId, int durationMs);

    // Передача началась
    void transferStarted(TransferProgress progress);

    // Обновление прогресса передачи
    void transferProgress(TransferProgress progress);

    // Передача завершена успешно
    void transferCompleted(QString id, QString filePath, bool outgoing);

    // Передача завершена с ошибкой
    void transferFailed(QString id, QString error);

    // Передача отменена
    void transferCancelled(QString id);

    // Файл получен (для совместимости со старым кодом)
    void fileReceived(QUuid from, QString path, QString name);

public slots:
    // Принять предложение файла
    void acceptOffer(const QUuid& from, const QString& offerId);

    // Отклонить предложение файла
    void rejectOffer(const QUuid& from, const QString& offerId);

    // Отменить активную передачу
    void cancelTransfer(const QString& transferId);

    // Приостановить передачу
    void pauseTransfer(const QString& transferId);

    // Возобновить передачу
    void resumeTransfer(const QString& transferId);

private:
    // ── Методы стриминга отправки ────────────────────────────────────────────

    // Вычислить SHA-256 хеш файла (статический — запускается в пуле потоков)
    static QByteArray computeFileHashStatic(const QString& filePath);

    // Отправить FILE_OFFER после вычисления хеша (вызывается из watcher::finished)
    void sendFileOffer(const QString& offerId);

    // Начать стриминг (вызывается после FILE_ACCEPT)
    void startStreaming(const QString& offerId);

    // Отправить следующий чанк (асинхронно через QTimer)
    void sendNextChunk(const QString& offerId);

    // Зашифровать чанк с AES-256-GCM
    QByteArray encryptChunk(const QByteArray& plaintext,
                            const QByteArray& key,
                            const QByteArray& nonce,
                            qint64 chunkSeq,
                            QByteArray& authTagOut);

    // ── Методы стриминга получения ───────────────────────────────────────────

    // Обработать входящий чанк
    void handleFileChunk(const QUuid& from, const QJsonObject& msg);

    // Расшифровать чанк с AES-256-GCM
    QByteArray decryptChunk(const QByteArray& ciphertext,
                            const QByteArray& authTag,
                            const QByteArray& key,
                            const QByteArray& nonce,
                            qint64 chunkSeq);

    // Завершить приём файла (проверка хеша, переименование)
    void finishReceiving(const QString& offerId);

    // ── Методы паузы/возобновления ───────────────────────────────────────────

    // Сохранить состояние передачи на диск
    void saveTransferState(const TransferResumeInfo& info);

    // Загрузить состояние передачи с диска
    bool loadTransferState(const QString& transferId, TransferResumeInfo& info);

    // Удалить сохранённое состояние
    void removeTransferState(const QString& transferId);

    // Обработать запрос на возобновление
    void handleResumeRequest(const QUuid& from, const QJsonObject& msg);

    // ── Утилиты ──────────────────────────────────────────────────────────────

    // Санитизация имени файла (защита от path traversal)
    QString sanitizeFileName(const QString& name);

    // Безопасный путь для сохранения
    QString safeDownloadPath(const QString& fileName);

    // Путь к временному файлу
    QString tempFilePath(const QString& transferId);

    // Путь к директории состояния передач
    QString transferStateDir();

    // Обновить и отправить прогресс
    void emitProgress(OutgoingTransfer& t);
    void emitProgress(IncomingTransfer& t);

    // Вычислить текущую скорость передачи
    double calculateSpeed(qint64 currentBytes, qint64& lastBytes,
                          qint64& lastTimeMs, QElapsedTimer& timer);

    // ── Данные ───────────────────────────────────────────────────────────────

    NetworkManager* m_net{nullptr};
    E2EManager*     m_e2e{nullptr};

    QMap<QString, OutgoingTransfer> m_outgoing;  // Исходящие передачи
    QMap<QString, IncomingTransfer> m_incoming;  // Входящие передачи

    // Константы
    static constexpr int kChunkSize          = 65536;      // 64 KB чанк
    static constexpr int kGcmTagSize         = 16;         // 16 байт для GCM auth tag
    static constexpr int kGcmNonceSize       = 12;         // 12 байт для GCM nonce
    static constexpr int kAesKeySize         = 32;         // 32 байта для AES-256
    static constexpr int kSpeedCalcInterval  = 500;        // Интервал расчёта скорости (мс)
};
