#pragma once
#include <QObject>

#ifdef HAVE_QT_MULTIMEDIA

#include <QElapsedTimer>
#include <QAudioFormat>

class QAudioSource;
class QIODevice;
class QFile;
class QTimer;

// ── AudioRecorder ──────────────────────────────────────────────────────────
// Запись голосовых сообщений через микрофон в WAV-файл.
// Формат: PCM 16-bit, 16000 Гц, моно.
// Использование:
//   auto* rec = new AudioRecorder(this);
//   rec->startRecording();       // начать запись
//   rec->stopRecording();        // остановить → emit recorded(path, durationMs)
//
// Сигнал recorded(filePath, durationMs):
//   filePath    — временный WAV-файл (нужно скопировать/отправить, потом удалить)
//   durationMs  — длительность записи в миллисекундах

class AudioRecorder : public QObject {
    Q_OBJECT
public:
    explicit AudioRecorder(QObject* parent = nullptr);
    ~AudioRecorder() override;

    void startRecording();
    void stopRecording();
    [[nodiscard]] bool isRecording() const { return m_recording; }

signals:
    // Запись завершена: WAV-файл готов к отправке
    void recorded(const QString& filePath, int durationMs);
    // Уровень звука (0.0–1.0) — обновляется каждые 50 мс для визуализации
    void levelChanged(float level);

private:
    void writeWavHeader(QFile* f);
    void finalizeWavHeader(QFile* f);

    QAudioSource* m_source     {nullptr};
    QIODevice*    m_device     {nullptr};
    QFile*        m_tmpFile    {nullptr};
    QTimer*       m_levelTimer {nullptr};
    QAudioFormat  m_format;
    QElapsedTimer m_elapsed;
    bool          m_recording  {false};
};

#else

// Заглушка AudioRecorder: Qt6Multimedia недоступен (cross-compile без мультимедиа).
// Все методы — no-op; сигналы объявлены для совместимости кода-потребителя.
class AudioRecorder : public QObject {
    Q_OBJECT
public:
    explicit AudioRecorder(QObject* parent = nullptr) : QObject(parent) {}
    void startRecording() {}
    void stopRecording() {}
    [[nodiscard]] bool isRecording() const { return false; }
signals:
    void recorded(const QString& filePath, int durationMs);
    void levelChanged(float level);
};

#endif // HAVE_QT_MULTIMEDIA
