#include "filetransferwidget.h"
#include "thememanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

// ── FileTransferWidget ───────────────────────────────────────────────────────

FileTransferWidget::FileTransferWidget(const QString& transferId,
                                       const QString& fileName,
                                       qint64 totalSize,
                                       bool outgoing,
                                       QWidget* parent)
    : QWidget(parent)
    , m_transferId(transferId)
    , m_outgoing(outgoing)
{
    setupUi(fileName, totalSize, outgoing);
}

void FileTransferWidget::setupUi(const QString& fileName,
                                  qint64 totalSize,
                                  bool outgoing) {
    const auto& p = ThemeManager::instance().palette();

    setObjectName("fileTransferWidget");
    setMinimumWidth(250);
    setMaximumWidth(400);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(6);

    // Верхняя строка: направление + имя файла
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(6);

    // Иконка направления
    m_directionLabel = new QLabel(outgoing ? "↑" : "↓");
    m_directionLabel->setStyleSheet(QString("font-size: 16px; color: %1;")
        .arg(outgoing ? p.accent : "#4caf50"));
    m_directionLabel->setFixedWidth(20);

    // Имя файла
    m_fileNameLabel = new QLabel(fileName);
    m_fileNameLabel->setStyleSheet(QString("color: %1; font-weight: bold;")
        .arg(p.textPrimary));
    m_fileNameLabel->setWordWrap(true);

    topRow->addWidget(m_directionLabel);
    topRow->addWidget(m_fileNameLabel, 1);
    mainLayout->addLayout(topRow);

    // Прогресс-бар
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(8);
    m_progressBar->setStyleSheet(QString(R"(
        QProgressBar {
            background: %1;
            border-radius: 4px;
            border: none;
        }
        QProgressBar::chunk {
            background: %2;
            border-radius: 4px;
        }
    )").arg(p.bgInput, p.accent));
    mainLayout->addWidget(m_progressBar);

    // Строка прогресса: размер + скорость + ETA
    auto* progressRow = new QHBoxLayout();
    progressRow->setSpacing(8);

    // Размер: "12.5 MB / 100 MB"
    m_progressLabel = new QLabel(QString("0 B / %1").arg(formatFileSize(totalSize)));
    m_progressLabel->setStyleSheet(QString("color: %1; font-size: 11px;")
        .arg(p.textMuted));

    // Скорость: "1.5 MB/s"
    m_speedLabel = new QLabel(tr("Waiting..."));
    m_speedLabel->setStyleSheet(QString("color: %1; font-size: 11px;")
        .arg(p.textMuted));

    // ETA: "~2:30"
    m_etaLabel = new QLabel("");
    m_etaLabel->setStyleSheet(QString("color: %1; font-size: 11px;")
        .arg(p.textMuted));

    progressRow->addWidget(m_progressLabel);
    progressRow->addStretch();
    progressRow->addWidget(m_speedLabel);
    progressRow->addWidget(m_etaLabel);
    mainLayout->addLayout(progressRow);

    // Кнопки: пауза + отмена
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    m_pauseBtn = new QPushButton(tr("Pause"));
    m_pauseBtn->setObjectName("dlgCancelBtn");
    m_pauseBtn->setFixedHeight(26);
    m_pauseBtn->setFixedWidth(80);
    connect(m_pauseBtn, &QPushButton::clicked, this, [this]() {
        if (m_paused) {
            m_paused = false;
            m_pauseBtn->setText(tr("Pause"));
            emit resumeRequested(m_transferId);
        } else {
            m_paused = true;
            m_pauseBtn->setText(tr("Resume"));
            emit pauseRequested(m_transferId);
        }
    });

    m_cancelBtn = new QPushButton(tr("Cancel"));
    m_cancelBtn->setObjectName("dlgCancelBtn");
    m_cancelBtn->setFixedHeight(26);
    m_cancelBtn->setFixedWidth(80);
    m_cancelBtn->setStyleSheet(QString("background: %1;").arg(p.danger));
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        emit cancelRequested(m_transferId);
    });

    // Статус (для завершения/ошибки)
    m_statusLabel = new QLabel("");
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;")
        .arg(p.textMuted));
    m_statusLabel->hide();

    btnRow->addWidget(m_statusLabel);
    btnRow->addStretch();
    btnRow->addWidget(m_pauseBtn);
    btnRow->addWidget(m_cancelBtn);
    mainLayout->addLayout(btnRow);

    // Общий стиль виджета
    setStyleSheet(QString(R"(
        #fileTransferWidget {
            background: %1;
            border: 1px solid %2;
            border-radius: 8px;
        }
    )").arg(p.bgElevated, p.border));
}

void FileTransferWidget::updateProgress(const TransferProgress& progress) {
    if (m_completed) return;

    // Обновляем прогресс-бар
    m_progressBar->setValue(progress.percent);

    // Обновляем размер
    m_progressLabel->setText(QString("%1 / %2")
        .arg(formatFileSize(progress.bytesTransferred))
        .arg(formatFileSize(progress.totalBytes)));

    // Обновляем скорость
    if (progress.speedBytesPerSec > 0) {
        m_speedLabel->setText(formatSpeed(progress.speedBytesPerSec));
    }

    // Обновляем ETA
    if (progress.etaSeconds > 0) {
        m_etaLabel->setText(QString("~%1").arg(formatEta(progress.etaSeconds)));
    } else {
        m_etaLabel->setText("");
    }
}

void FileTransferWidget::setCompleted(bool success, const QString& message) {
    m_completed = true;
    const auto& p = ThemeManager::instance().palette();

    // Скрываем кнопки управления
    m_pauseBtn->hide();
    m_cancelBtn->hide();

    // Показываем статус
    m_statusLabel->show();

    if (success) {
        m_progressBar->setValue(100);
        m_statusLabel->setText(tr("Completed"));
        m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;")
            .arg("#4caf50"));  // Зелёный
        m_speedLabel->setText("");
        m_etaLabel->setText("");
    } else {
        m_statusLabel->setText(message.isEmpty() ? tr("Failed") : message);
        m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;")
            .arg(p.danger));
        m_progressBar->setStyleSheet(QString(R"(
            QProgressBar {
                background: %1;
                border-radius: 4px;
                border: none;
            }
            QProgressBar::chunk {
                background: %2;
                border-radius: 4px;
            }
        )").arg(p.bgInput, p.danger));
    }
}

QString FileTransferWidget::formatFileSize(qint64 bytes) const {
    const double kb = bytes / 1024.0;
    const double mb = kb / 1024.0;
    const double gb = mb / 1024.0;

    if (gb >= 1.0) {
        return QString("%1 GB").arg(gb, 0, 'f', 2);
    } else if (mb >= 1.0) {
        return QString("%1 MB").arg(mb, 0, 'f', 2);
    } else if (kb >= 1.0) {
        return QString("%1 KB").arg(kb, 0, 'f', 1);
    } else {
        return QString("%1 B").arg(bytes);
    }
}

QString FileTransferWidget::formatSpeed(double bytesPerSec) const {
    const double kb = bytesPerSec / 1024.0;
    const double mb = kb / 1024.0;

    if (mb >= 1.0) {
        return QString("%1 MB/s").arg(mb, 0, 'f', 1);
    } else if (kb >= 1.0) {
        return QString("%1 KB/s").arg(kb, 0, 'f', 0);
    } else {
        return QString("%1 B/s").arg(static_cast<int>(bytesPerSec));
    }
}

QString FileTransferWidget::formatEta(int seconds) const {
    if (seconds < 60) {
        return QString("%1s").arg(seconds);
    } else if (seconds < 3600) {
        const int min = seconds / 60;
        const int sec = seconds % 60;
        return QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0'));
    } else {
        const int hr = seconds / 3600;
        const int min = (seconds % 3600) / 60;
        return QString("%1h %2m").arg(hr).arg(min);
    }
}
