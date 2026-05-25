#include "voicewaveform.h"
#include <QPainter>
#include <QMouseEvent>
#include <QFile>
#include <QtEndian>
#include <cmath>

VoiceWaveform::VoiceWaveform(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(38);
}

// ── Загрузка WAV ─────────────────────────────────────────────────────────

QVector<float> VoiceWaveform::extractAmplitudes(const QString& wavPath, int numBars) {
    QFile f(wavPath);
    if (wavPath.isEmpty() || !f.open(QIODevice::ReadOnly))
        return QVector<float>(numBars, 0.15f);  // заглушка

    const QByteArray raw = f.readAll();
    f.close();

    // Ищем chunk "data" в RIFF-контейнере
    int dataOffset = -1;
    int dataBytes  = 0;
    for (int i = 0; i + 8 <= raw.size(); ++i) {
        if (raw[i]=='d' && raw[i+1]=='a' && raw[i+2]=='t' && raw[i+3]=='a') {
            dataBytes  = qFromLittleEndian<qint32>(
                reinterpret_cast<const uchar*>(raw.constData() + i + 4));
            dataOffset = i + 8;
            break;
        }
    }
    if (dataOffset < 0 || dataBytes <= 0)
        return QVector<float>(numBars, 0.15f);

    // Ограничиваем реальным размером буфера
    dataBytes = qMin(dataBytes, raw.size() - dataOffset);
    const int numSamples = dataBytes / 2;  // 16-bit mono PCM
    if (numSamples == 0)
        return QVector<float>(numBars, 0.15f);

    const auto* samples = reinterpret_cast<const qint16*>(raw.constData() + dataOffset);

    QVector<float> bars(numBars, 0.0f);
    const float step = static_cast<float>(numSamples) / numBars;
    for (int b = 0; b < numBars; ++b) {
        const int s0 = static_cast<int>(b * step);
        const int s1 = qMin(static_cast<int>((b + 1) * step), numSamples);
        float rms = 0.0f;
        for (int i = s0; i < s1; ++i) {
            const float v = qFromLittleEndian<qint16>(
                reinterpret_cast<const uchar*>(samples + i)) / 32768.0f;
            rms += v * v;
        }
        bars[b] = (s1 > s0) ? std::sqrt(rms / (s1 - s0)) : 0.0f;
    }

    // Нормализуем — тихая запись тоже выглядит хорошо
    float maxVal = 0.0f;
    for (float v : bars) maxVal = qMax(maxVal, v);
    if (maxVal > 0.001f)
        for (float& v : bars) v = qMax(0.04f, v / maxVal);
    else
        for (float& v : bars) v = 0.15f;

    return bars;
}

void VoiceWaveform::loadFile(const QString& wavPath) {
    m_bars = extractAmplitudes(wavPath, kNumBars);
    m_progress = 0.0f;
    update();
}

// ── Прогресс ─────────────────────────────────────────────────────────────

void VoiceWaveform::setProgress(float p) {
    m_progress = qBound(0.0f, p, 1.0f);
    update();
}

void VoiceWaveform::setColors(const QColor& played, const QColor& unplayed) {
    m_colorPlayed   = played;
    m_colorUnplayed = unplayed;
    update();
}

// ── Отрисовка ─────────────────────────────────────────────────────────────

void VoiceWaveform::paintEvent(QPaintEvent*) {
    if (m_bars.isEmpty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);  // чёткие пиксели

    const int n  = m_bars.size();
    const int w  = width();
    const int h  = height();
    const int cy = h / 2;
    const int maxH = cy - 2;  // максимальная высота от центра

    // Ширина и шаг столбика: вписываемся в ширину виджета
    const float pitch = static_cast<float>(w) / n;
    const int   barW  = qMax(1, static_cast<int>(pitch) - 1);

    for (int i = 0; i < n; ++i) {
        const float amp = m_bars[i];
        // Верхняя и нижняя полосы разной высоты для эффекта стерео-каналов:
        // верхняя = оригинальная амплитуда, нижняя = чуть меньше (×0.65)
        const int topH = qMax(kMinBarH, static_cast<int>(amp * maxH));
        const int botH = qMax(kMinBarH, static_cast<int>(amp * maxH * 0.65f));

        const int x = static_cast<int>(i * pitch);
        const bool played = (static_cast<float>(i) / n) < m_progress;

        // Цвет с лёгкой прозрачностью для нижней (имитация второго канала)
        const QColor colPlayed    = m_colorPlayed;
        const QColor colUnplayed  = m_colorUnplayed;
        const QColor colPlayedBot = m_colorPlayed.darker(130);
        const QColor colUnplayedBot = QColor(m_colorUnplayed.red(),
                                             m_colorUnplayed.green(),
                                             m_colorUnplayed.blue(), 160);

        p.fillRect(x, cy - topH, barW, topH,
                   played ? colPlayed    : colUnplayed);
        p.fillRect(x, cy + 1,    barW, botH,
                   played ? colPlayedBot : colUnplayedBot);
    }
}

// ── Клик = перемотка ──────────────────────────────────────────────────────

void VoiceWaveform::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton) {
        const float pos = static_cast<float>(ev->position().x()) / width();
        emit seekRequested(qBound(0.0f, pos, 1.0f));
    }
    QWidget::mousePressEvent(ev);
}
