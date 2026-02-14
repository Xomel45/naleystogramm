#include "updatebanner.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>

UpdateBanner::UpdateBanner(QWidget* parent) : QWidget(parent) {
    setObjectName("updateBanner");
    setFixedHeight(36);
    QWidget::hide();

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 8, 0);
    layout->setSpacing(8);

    m_label = new QLabel();
    m_label->setObjectName("updateBannerText");

    m_openBtn = new QPushButton("Обновить");
    m_openBtn->setObjectName("updateBannerBtn");
    m_openBtn->setFixedHeight(24);
    connect(m_openBtn, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl(m_url));
    });

    m_closeBtn = new QPushButton("✕");
    m_closeBtn->setObjectName("iconBtn");
    m_closeBtn->setFixedSize(22, 22);
    connect(m_closeBtn, &QPushButton::clicked, this, &UpdateBanner::hide);

    layout->addWidget(m_label, 1);
    layout->addWidget(m_openBtn);
    layout->addWidget(m_closeBtn);
}

void UpdateBanner::showUpdate(const UpdateInfo& info) {
    m_url = info.url;
    m_label->setText(QString("🆕 Версия %1").arg(info.version));
    QWidget::show();
}

void UpdateBanner::hide() {
    QWidget::hide();
}
