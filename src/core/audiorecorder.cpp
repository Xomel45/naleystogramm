#include "audiorecorder.h"

// Весь файл компилируется только при наличии Qt6Multimedia
#ifdef HAVE_QT_MULTIMEDIA

#include <QAudioSource>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QFile>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>
#include <QDebug>
#include <cstring>

// WAV-заголовок — 44 байта стандартного PCM RIFF
// Записывается дважды: сначала с нулевым размером,
// потом перезаписывается с реальным после остановки записи.

static constexpr int kWavHeaderSize = 44;

// Записывает заголовок WAV в файл.
// dataSize — размер блока данных PCM в байтах (0 при первом вызове).
static void writeWavHeaderRaw(QFile* f, const QAudioFormat& fmt, quint32 dataSize) {
    const quint16 numChannels = static_cast<quint16>(fmt.channelCount());
    const quint32 sampleRate  = static_cast<quint32>(fmt.sampleRate());
    // Для PCM 16-bit: blockAlign = channels * 2, byteRate = sampleRate * blockAlign
    const quint16 bitsPerSample = 16;
    const quint16 blockAlign    = numChannels * (bitsPerSample / 8);
    const quint32 byteRate      = sampleRate * blockAlign;
    const quint32 riffSize      = 36 + dataSize;  // 4 + (8 + 16) + (8 + dataSize) - 8

    quint8 hdr[kWavHeaderSize];
    std::memset(hdr, 0, sizeof(hdr));

    // RIFF chunk
    hdr[0]='R'; hdr[1]='I'; hdr[2]='F'; hdr[3]='F';
    hdr[4] = static_cast<quint8>(riffSize);
    hdr[5] = static_cast<quint8>(riffSize >> 8);
    hdr[6] = static_cast<quint8>(riffSize >> 16);
    hdr[7] = static_cast<quint8>(riffSize >> 24);
    hdr[8]='W'; hdr[9]='A'; hdr[10]='V'; hdr[11]='E';

    // fmt sub-chunk
    hdr[12]='f'; hdr[13]='m'; hdr[14]='t'; hdr[15]=' ';
    // chunk size = 16 (PCM)
    hdr[16]=16; hdr[17]=0; hdr[18]=0; hdr[19]=0;
    // AudioFormat = 1 (PCM)
    hdr[20]=1;  hdr[21]=0;
    // NumChannels
    hdr[22]=static_cast<quint8>(numChannels);
    hdr[23]=static_cast<quint8>(numChannels>>8);
    // SampleRate
    hdr[24]=static_cast<quint8>(sampleRate);
    hdr[25]=static_cast<quint8>(sampleRate>>8);
    hdr[26]=static_cast<quint8>(sampleRate>>16);
    hdr[27]=static_cast<quint8>(sampleRate>>24);
    // ByteRate
    hdr[28]=static_cast<quint8>(byteRate);
    hdr[29]=static_cast<quint8>(byteRate>>8);
    hdr[30]=static_cast<quint8>(byteRate>>16);
    hdr[31]=static_cast<quint8>(byteRate>>24);
    // BlockAlign
    hdr[32]=static_cast<quint8>(blockAlign);
    hdr[33]=static_cast<quint8>(blockAlign>>8);
    // BitsPerSample
    hdr[34]=static_cast<quint8>(bitsPerSample);
    hdr[35]=static_cast<quint8>(bitsPerSample>>8);

    // data sub-chunk
    hdr[36]='d'; hdr[37]='a'; hdr[38]='t'; hdr[39]='a';
    hdr[40]=static_cast<quint8>(dataSize);
    hdr[41]=static_cast<quint8>(dataSize>>8);
    hdr[42]=static_cast<quint8>(dataSize>>16);
    hdr[43]=static_cast<quint8>(dataSize>>24);

    f->write(reinterpret_cast<const char*>(hdr), kWavHeaderSize);
}

// ── AudioRecorder ──────────────────────────────────────────────────────────

AudioRecorder::AudioRecorder(QObject* parent) : QObject(parent) {
    // Настройка формата: PCM 16-bit, 16000 Гц, моно
    m_format.setSampleRate(16000);
    m_format.setChannelCount(1);
    m_format.setSampleFormat(QAudioFormat::Int16);
}

AudioRecorder::~AudioRecorder() {
    if (m_recording) stopRecording();
}

void AudioRecorder::startRecording() {
    if (m_recording) return;

    // Проверяем доступность микрофона
    const QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    if (inputDevice.isNull()) {
        qWarning("[AudioRecorder] Микрофон не найден!");
        return;
    }

    // Создаём временный WAV-файл в директории tmp
    const QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tmpDir);
    const QString tmpPath = tmpDir + "/naleys_voice_"
        + QUuid::createUuid().toString(QUuid::Id128).left(8) + ".wav";

    m_tmpFile = new QFile(tmpPath, this);
    if (!m_tmpFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("[AudioRecorder] Temp File Creation Failed");
        delete m_tmpFile;
        m_tmpFile = nullptr;
        return;
    }

    // Записываем placeholder WAV-заголовок (dataSize=0, будет исправлен при остановке)
    writeWavHeaderRaw(m_tmpFile, m_format, 0);

    // Запускаем захват звука
    m_source = new QAudioSource(inputDevice, m_format, this);
    m_device = m_source->start();  // возвращает QIODevice для чтения PCM-данных

    if (m_source->error() != QAudio::NoError) {
        qWarning("[AudioRecorder] Audio Capture Start Failed");
        m_tmpFile->remove();
        delete m_tmpFile;  m_tmpFile = nullptr;
        delete m_source;   m_source = nullptr;
        m_device = nullptr;
        return;
    }

    // Читаем PCM из QAudioSource и пишем в файл каждые 50 мс
    m_levelTimer = new QTimer(this);
    m_levelTimer->setInterval(50);
    connect(m_levelTimer, &QTimer::timeout, this, [this]() {
        if (!m_device || !m_tmpFile) return;

        // Читаем доступные данные и пишем в файл
        const QByteArray data = m_device->readAll();
        if (!data.isEmpty()) {
            m_tmpFile->write(data);

            // Вычисляем пиковый уровень (avg RMS по 16-bit сэмплам)
            const auto* samples = reinterpret_cast<const qint16*>(data.constData());
            const int count = data.size() / 2;
            if (count > 0) {
                qint64 sum = 0;
                for (int i = 0; i < count; ++i)
                    sum += qAbs(static_cast<qint32>(samples[i]));
                const float level = static_cast<float>(sum) / count / 32768.0f;
                emit levelChanged(qMin(level * 3.0f, 1.0f));  // небольшое усиление
            }
        }
    });
    m_levelTimer->start();

    m_elapsed.start();
    m_recording = true;
    qDebug("[AudioRecorder] Recording Started");
}

void AudioRecorder::stopRecording() {
    if (!m_recording) return;
    m_recording = false;

    const int durationMs = static_cast<int>(m_elapsed.elapsed());

    // Останавливаем таймер чтения
    if (m_levelTimer) {
        m_levelTimer->stop();
        // Читаем оставшиеся данные
        if (m_device && m_tmpFile) {
            const QByteArray tail = m_device->readAll();
            if (!tail.isEmpty()) m_tmpFile->write(tail);
        }
        m_levelTimer->deleteLater();
        m_levelTimer = nullptr;
    }

    // Останавливаем захват
    if (m_source) {
        m_source->stop();
        m_source->deleteLater();
        m_source = nullptr;
        m_device = nullptr;
    }

    // Перезаписываем WAV-заголовок с реальным размером данных
    if (m_tmpFile) {
        const qint64 totalSize  = m_tmpFile->size();
        const quint32 dataSize  = static_cast<quint32>(
            qMax(qint64(0), totalSize - kWavHeaderSize));

        m_tmpFile->seek(0);
        writeWavHeaderRaw(m_tmpFile, m_format, dataSize);
        m_tmpFile->flush();

        const QString filePath = m_tmpFile->fileName();
        m_tmpFile->close();
        m_tmpFile->deleteLater();
        m_tmpFile = nullptr;

        qDebug("[AudioRecorder] Recording Completed");

        emit levelChanged(0.0f);
        emit recorded(filePath, durationMs);
    }
}

#endif // HAVE_QT_MULTIMEDIA
