#include "devicelinkdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QIntValidator>

// ── DeviceLinkDialog ──────────────────────────────────────────────────────────

DeviceLinkDialog::DeviceLinkDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Привязать к устройству"));
    setFixedWidth(380);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(12);

    auto* title = new QLabel(tr("Привязка к главному устройству"));
    title->setObjectName("dlgTitle");
    layout->addWidget(title);

    auto* subtitle = new QLabel(
        tr("Откройте «Привязать устройство» на главном устройстве, "
           "затем введите его адрес и показанный код."));
    subtitle->setObjectName("dlgSubtitle");
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    layout->addSpacing(8);

    // ── IP:порт ───────────────────────────────────────────────────────────────
    auto* hostLabel = new QLabel(tr("Адрес главного устройства (IP:порт)"));
    hostLabel->setObjectName("settingsFieldLabel");
    layout->addWidget(hostLabel);

    m_hostEdit = new QLineEdit();
    m_hostEdit->setObjectName("dlgInput");
    m_hostEdit->setPlaceholderText(QStringLiteral("192.168.1.100:47821"));
    layout->addWidget(m_hostEdit);

    // ── Код ───────────────────────────────────────────────────────────────────
    auto* codeLabel = new QLabel(tr("Код с главного устройства (6 цифр)"));
    codeLabel->setObjectName("settingsFieldLabel");
    layout->addWidget(codeLabel);

    m_codeEdit = new QLineEdit();
    m_codeEdit->setObjectName("dlgInput");
    m_codeEdit->setPlaceholderText(QStringLiteral("000000"));
    m_codeEdit->setMaxLength(6);
    m_codeEdit->setValidator(new QIntValidator(0, 999999, this));
    layout->addWidget(m_codeEdit);

    // ── Ошибка ────────────────────────────────────────────────────────────────
    m_error = new QLabel();
    m_error->setObjectName("dlgError");
    m_error->hide();
    layout->addWidget(m_error);

    // ── Кнопки ───────────────────────────────────────────────────────────────
    auto* cancelBtn = new QPushButton(tr("Отмена"));
    cancelBtn->setObjectName("dlgCancelBtn");
    auto* linkBtn = new QPushButton(tr("Привязать"));
    linkBtn->setObjectName("dlgOkBtn");

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(linkBtn, &QPushButton::clicked, this, [this]() {
        const QString h = m_hostEdit->text().trimmed();
        const QString c = m_codeEdit->text().trimmed();

        if (h.isEmpty()) {
            m_error->setText(tr("Введите адрес главного устройства"));
            m_error->show();
            return;
        }
        if (!h.contains(':')) {
            m_error->setText(tr("Формат: IP:порт (например 192.168.1.100:47821)"));
            m_error->show();
            return;
        }
        if (c.length() != 6) {
            m_error->setText(tr("Код должен содержать ровно 6 цифр"));
            m_error->show();
            return;
        }
        accept();
    });

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    btnRow->addWidget(cancelBtn);
    btnRow->addStretch();
    btnRow->addWidget(linkBtn);
    layout->addLayout(btnRow);
}

QString DeviceLinkDialog::host() const {
    const QString s = m_hostEdit->text().trimmed();
    const int colon = s.lastIndexOf(':');
    return colon > 0 ? s.left(colon) : s;
}

int DeviceLinkDialog::port() const {
    const QString s = m_hostEdit->text().trimmed();
    const int colon = s.lastIndexOf(':');
    if (colon < 0) return 47821;
    bool ok = false;
    const int p = s.mid(colon + 1).toInt(&ok);
    return (ok && p > 0 && p <= 65535) ? p : 47821;
}

QString DeviceLinkDialog::code() const {
    return m_codeEdit->text().trimmed().rightJustified(6, '0');
}
