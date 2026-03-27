#include "addcontactdialog.h"
#include "../thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>

// ── AddContactDialog ──────────────────────────────────────────────────────

AddContactDialog::AddContactDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Add Contact"));
    setFixedWidth(420);
    // Стиль берём из глобального QSS (ThemeManager уже применил его)

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    auto* title = new QLabel(tr("Add Contact"));
    title->setObjectName("dlgTitle");

    auto* subtitle = new QLabel(tr("Paste connection string from another user"));
    subtitle->setObjectName("dlgSubtitle");
    subtitle->setWordWrap(true);

    m_input = new QTextEdit();
    m_input->setObjectName("dlgInput");
    m_input->setPlaceholderText(tr("Name@UUID@IP:Port"));
    m_input->setFixedHeight(72);

    m_error = new QLabel();
    m_error->setObjectName("dlgError");
    m_error->hide();

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    auto* cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setObjectName("dlgCancelBtn");
    auto* okBtn = new QPushButton(tr("Connect"));
    okBtn->setObjectName("dlgOkBtn");

    connect(okBtn, &QPushButton::clicked, this, [this]() {
        if (m_input->toPlainText().trimmed().isEmpty()) {
            m_error->setText(tr("Enter connection string"));
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
