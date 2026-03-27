#pragma once
#include <QWidget>
#include "../core/filetransfer.h"

class QLabel;
class QProgressBar;
class QPushButton;

// ── FileTransferWidget ───────────────────────────────────────────────────────
// Виджет отображения прогресса передачи файла.
// Показывает имя файла, прогресс-бар, скорость, ETA и кнопку отмены.
// Предназначен для встраивания в чат или отдельное окно передач.
//
class FileTransferWidget : public QWidget {
    Q_OBJECT
public:
    explicit FileTransferWidget(const QString& transferId,
                                const QString& fileName,
                                qint64 totalSize,
                                bool outgoing,
                                QWidget* parent = nullptr);

    // Обновить прогресс передачи
    void updateProgress(const TransferProgress& progress);

    // Установить состояние завершения
    void setCompleted(bool success, const QString& message = QString());

    // Получить ID передачи
    [[nodiscard]] QString transferId() const { return m_transferId; }

signals:
    // Пользователь запросил отмену передачи
    void cancelRequested(QString transferId);

    // Пользователь запросил паузу
    void pauseRequested(QString transferId);

    // Пользователь запросил возобновление
    void resumeRequested(QString transferId);

private:
    void setupUi(const QString& fileName, qint64 totalSize, bool outgoing);

    // Форматирование размера файла
    QString formatFileSize(qint64 bytes) const;

    // Форматирование скорости
    QString formatSpeed(double bytesPerSec) const;

    // Форматирование времени
    QString formatEta(int seconds) const;

    QString m_transferId;
    bool    m_outgoing   {true};
    bool    m_paused     {false};
    bool    m_completed  {false};

    QLabel*       m_fileNameLabel  {nullptr};
    QLabel*       m_directionLabel {nullptr};  // ↑ или ↓
    QProgressBar* m_progressBar    {nullptr};
    QLabel*       m_progressLabel  {nullptr};  // "12.5 MB / 100 MB"
    QLabel*       m_speedLabel     {nullptr};  // "1.5 MB/s"
    QLabel*       m_etaLabel       {nullptr};  // "~2:30"
    QPushButton*  m_pauseBtn       {nullptr};
    QPushButton*  m_cancelBtn      {nullptr};
    QLabel*       m_statusLabel    {nullptr};
};
