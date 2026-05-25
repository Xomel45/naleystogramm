#pragma once
#include <QWidget>
#include <QVector>
#include <QColor>

// ── VoiceWaveform ──────────────────────────────────────────────────────────
// Виджет для отображения волновой формы голосового сообщения в TG-стиле.
// Столбики симметрично вверх/вниз от центра — визуальный эффект каналов.
// Высота столбика определяется амплитудой PCM в соответствующем отрезке.
// Прогресс воспроизведения делит столбики на «сыгранные» (акцент) и «нет» (серые).

class VoiceWaveform : public QWidget {
    Q_OBJECT
public:
    explicit VoiceWaveform(QWidget* parent = nullptr);

    // Загружает PCM из WAV-файла и строит массив амплитуд (асинхронно — в вызывающем потоке).
    // При пустом пути — показывает заглушку из одинаковых столбиков.
    void loadFile(const QString& wavPath);

    // Прогресс воспроизведения 0.0 (начало) – 1.0 (конец).
    void setProgress(float progress);
    float progress() const { return m_progress; }

    // Цвета столбиков.
    void setColors(const QColor& played, const QColor& unplayed);

    QSize sizeHint()        const override { return {200, 38}; }
    QSize minimumSizeHint() const override { return {80,  38}; }

signals:
    // Пользователь кликнул по волне — запрос перемотки в позицию [0,1].
    void seekRequested(float position);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* ev) override;

private:
    static QVector<float> extractAmplitudes(const QString& wavPath, int numBars);

    QVector<float> m_bars;
    float          m_progress    {0.0f};
    QColor         m_colorPlayed   {Qt::white};
    QColor         m_colorUnplayed {0x60, 0x60, 0x60};

    static constexpr int kNumBars = 60;
    static constexpr int kMinBarH = 2;   // минимальная высота столбика (px от центра)
};
