#include "joingroupdialog.h"
#include "../thememanager.h"
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QRegularExpression>
#include <QUrl>

JoinGroupDialog::JoinGroupDialog(const QString& defaultUsername, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Вступить в группу или канал");
    setMinimumWidth(400);

    const ThemePalette& p = ThemeManager::instance().palette();

    auto* lay = new QVBoxLayout(this);
    lay->setSpacing(12);
    lay->setContentsMargins(20, 20, 20, 20);

    // Описание
    auto* desc = new QLabel(
        "Введите адрес сервера группы или канала.\n"
        "Пример: <b>http://mygroup.example:47822</b>");
    desc->setWordWrap(true);
    desc->setTextFormat(Qt::RichText);
    lay->addWidget(desc);

    // URL сервера
    auto* urlLabel = new QLabel("Адрес сервера:");
    lay->addWidget(urlLabel);
    m_urlEdit = new QLineEdit;
    m_urlEdit->setPlaceholderText("http://server:47822 или https://…");
    lay->addWidget(m_urlEdit);

    // Username
    auto* userLabel = new QLabel("Имя пользователя:");
    lay->addWidget(userLabel);
    m_usernameEdit = new QLineEdit(defaultUsername);
    m_usernameEdit->setPlaceholderText("vasya");
    lay->addWidget(m_usernameEdit);

    // Ошибка
    m_errorLabel = new QLabel;
    m_errorLabel->setStyleSheet(QString("color:%1;").arg(p.danger));
    m_errorLabel->setVisible(false);
    lay->addWidget(m_errorLabel);

    // Кнопки
    auto* btnBox = new QHBoxLayout;
    btnBox->addStretch();
    m_joinBtn = new QPushButton("Вступить");
    m_joinBtn->setDefault(true);
    m_joinBtn->setEnabled(false);
    m_joinBtn->setObjectName("primaryBtn");
    auto* cancelBtn = new QPushButton("Отмена");
    btnBox->addWidget(cancelBtn);
    btnBox->addWidget(m_joinBtn);
    lay->addLayout(btnBox);

    connect(m_urlEdit,      &QLineEdit::textChanged, this, &JoinGroupDialog::validate);
    connect(m_usernameEdit, &QLineEdit::textChanged, this, &JoinGroupDialog::validate);
    connect(m_joinBtn,  &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn,  &QPushButton::clicked, this, &QDialog::reject);
}

void JoinGroupDialog::validate() {
    const QString url  = m_urlEdit->text().trimmed();
    const QString user = m_usernameEdit->text().trimmed();

    m_errorLabel->setVisible(false);
    m_joinBtn->setEnabled(false);

    if (url.isEmpty() || user.isEmpty()) return;

    if (!url.startsWith("http://") && !url.startsWith("https://")) {
        m_errorLabel->setText("URL должен начинаться с http:// или https://");
        m_errorLabel->setVisible(true);
        return;
    }

    // Валидация имени пользователя: 3–32 символа, [a-zA-Z0-9_.-]
    static const QRegularExpression re("^[a-zA-Z0-9_.\\-]{3,32}$");
    if (!re.match(user).hasMatch()) {
        m_errorLabel->setText("Имя: 3–32 символа, только a-z A-Z 0-9 _ . -");
        m_errorLabel->setVisible(true);
        return;
    }

    m_joinBtn->setEnabled(true);
}

QString JoinGroupDialog::serverUrl() const {
    QString url = m_urlEdit->text().trimmed();
    // Убрать trailing slash
    while (url.endsWith('/')) url.chop(1);
    return url;
}

QString JoinGroupDialog::username() const {
    return m_usernameEdit->text().trimmed();
}
