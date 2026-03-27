#include "fileacceptdialog.h"
#include "../thememanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

// ── FileAcceptDialog ─────────────────────────────────────────────────────────

FileAcceptDialog::FileAcceptDialog(const QString& senderName,
                                   const QString& fileName,
                                   qint64 fileSize,
                                   const QString& offerId,
                                   QWidget* parent)
    : QDialog(parent)
    , m_offerId(offerId)
{
    setWindowTitle(tr("Incoming file"));
    setModal(true);
    setMinimumWidth(320);
    setMaximumWidth(450);

    setupUi(senderName, fileName, fileSize);
}

void FileAcceptDialog::setupUi(const QString& senderName,
                               const QString& fileName,
                               qint64 fileSize) {
    const auto& p = ThemeManager::instance().palette();

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);

    // Заголовок с иконкой
    m_titleLabel = new QLabel(tr("Incoming file transfer"));
    m_titleLabel->setObjectName("dlgTitle");
    m_titleLabel->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;")
        .arg(p.textPrimary));
    mainLayout->addWidget(m_titleLabel);

    // Разделитель
    auto* separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setObjectName("settingsSeparator");
    mainLayout->addWidget(separator);

    // Информация об отправителе
    auto* senderRow = new QHBoxLayout();
    auto* senderTitleLbl = new QLabel(tr("From:"));
    senderTitleLbl->setStyleSheet(QString("color: %1;").arg(p.textMuted));
    m_senderLabel = new QLabel(senderName);
    m_senderLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(p.accent));
    senderRow->addWidget(senderTitleLbl);
    senderRow->addWidget(m_senderLabel, 1);
    mainLayout->addLayout(senderRow);

    // Имя файла
    auto* fileRow = new QHBoxLayout();
    auto* fileTitleLbl = new QLabel(tr("File:"));
    fileTitleLbl->setStyleSheet(QString("color: %1;").arg(p.textMuted));
    m_fileNameLabel = new QLabel(fileName);
    m_fileNameLabel->setStyleSheet(QString("color: %1;").arg(p.textPrimary));
    m_fileNameLabel->setWordWrap(true);
    fileRow->addWidget(fileTitleLbl);
    fileRow->addWidget(m_fileNameLabel, 1);
    mainLayout->addLayout(fileRow);

    // Размер файла
    auto* sizeRow = new QHBoxLayout();
    auto* sizeTitleLbl = new QLabel(tr("Size:"));
    sizeTitleLbl->setStyleSheet(QString("color: %1;").arg(p.textMuted));
    m_fileSizeLabel = new QLabel(formatFileSize(fileSize));
    m_fileSizeLabel->setStyleSheet(QString("color: %1;").arg(p.textPrimary));
    sizeRow->addWidget(sizeTitleLbl);
    sizeRow->addWidget(m_fileSizeLabel, 1);
    mainLayout->addLayout(sizeRow);

    mainLayout->addSpacing(8);

    // Кнопки
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(12);

    m_rejectBtn = new QPushButton(tr("Reject"));
    m_rejectBtn->setObjectName("dlgCancelBtn");
    m_rejectBtn->setFixedHeight(36);
    connect(m_rejectBtn, &QPushButton::clicked, this, [this]() {
        emit rejected(m_offerId);
        reject();
    });

    m_acceptBtn = new QPushButton(tr("Accept"));
    m_acceptBtn->setObjectName("dlgOkBtn");
    m_acceptBtn->setFixedHeight(36);
    m_acceptBtn->setDefault(true);
    connect(m_acceptBtn, &QPushButton::clicked, this, [this]() {
        emit accepted(m_offerId);
        accept();
    });

    btnLayout->addWidget(m_rejectBtn, 1);
    btnLayout->addWidget(m_acceptBtn, 1);
    mainLayout->addLayout(btnLayout);

    // Стиль диалога
    setStyleSheet(QString(R"(
        QDialog {
            background: %1;
            border-radius: 12px;
        }
    )").arg(p.bgElevated));
}

QString FileAcceptDialog::formatFileSize(qint64 bytes) const {
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
