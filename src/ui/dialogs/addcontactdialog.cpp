#include "addcontactdialog.h"
#include "../thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>

// ── AddContactDialog ──────────────────────────────────────────────────────

AddContactDialog::AddContactDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Добавить контакт");
    setFixedWidth(420);
    // Стиль берём из глобального QSS (ThemeManager уже применил его)

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    auto* title = new QLabel("Добавить контакт");
    title->setObjectName("dlgTitle");

    auto* subtitle = new QLabel("Вставь строку подключения от другого пользователя");
    subtitle->setObjectName("dlgSubtitle");
    subtitle->setWordWrap(true);

    m_input = new QTextEdit();
    m_input->setObjectName("dlgInput");
    m_input->setPlaceholderText("Имя@UUID@IP:Порт");
    m_input->setFixedHeight(72);

    m_error = new QLabel();
    m_error->setObjectName("dlgError");
    m_error->hide();

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    auto* cancelBtn = new QPushButton("Отмена");
    cancelBtn->setObjectName("dlgCancelBtn");
    auto* okBtn = new QPushButton("Подключиться");
    okBtn->setObjectName("dlgOkBtn");

    connect(okBtn, &QPushButton::clicked, this, [this]() {
        if (m_input->toPlainText().trimmed().isEmpty()) {
            m_error->setText("Введи строку подключения");
            m_error->show();
            return;
        }
        accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnRow->addWidget(cancelBtn);
    btnRow->addStretch();
    btnRow->addWidget(okBtn);

    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(m_input);
    layout->addWidget(m_error);
    layout->addLayout(btnRow);
}

QString AddContactDialog::connectionString() const {
    return m_input->toPlainText().trimmed();
}

// FIX: IncomingDialog реализован в incomingdialog.cpp
