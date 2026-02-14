#include "incomingdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

IncomingDialog::IncomingDialog(const QString& name, const QString& ip,
                                QWidget* parent) : QDialog(parent) {
    setWindowTitle("Входящий запрос");
    setFixedWidth(360);
    // Глобальный QSS уже применён через ThemeManager

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(12);

    auto* icon = new QLabel("◈");
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet("font-size: 38px;");

    auto* title = new QLabel("Входящий запрос");
    title->setObjectName("dlgTitle");
    title->setAlignment(Qt::AlignCenter);

    auto* info = new QLabel(
        QString("<b>%1</b><br><span style='font-size:12px;'>%2</span>")
        .arg(name.toHtmlEscaped(), ip.toHtmlEscaped()));
    info->setAlignment(Qt::AlignCenter);
    info->setTextFormat(Qt::RichText);

    auto* subtitle = new QLabel("хочет подключиться к тебе");
    subtitle->setObjectName("dlgSubtitle");
    subtitle->setAlignment(Qt::AlignCenter);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    auto* rejectBtn = new QPushButton("✕  Отклонить");
    rejectBtn->setObjectName("dlgRejectBtn");
    auto* acceptBtn = new QPushButton("✓  Принять");
    acceptBtn->setObjectName("dlgAcceptBtn");

    connect(acceptBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(rejectBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnRow->addWidget(rejectBtn, 1);
    btnRow->addWidget(acceptBtn, 1);

    layout->addWidget(icon);
    layout->addWidget(title);
    layout->addSpacing(4);
    layout->addWidget(info);
    layout->addWidget(subtitle);
    layout->addSpacing(10);
    layout->addLayout(btnRow);
}
