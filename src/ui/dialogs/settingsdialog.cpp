#include "settingsdialog.h"
#include "../thememanager.h"
#include "../../core/identity.h"
#include "../../core/network.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QSettings>
#include <QNetworkProxy>
#include <QGroupBox>
#include <QFormLayout>
#include <QCheckBox>

// ── SettingsDialog ─────────────────────────────────────────────────────────

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Настройки");
    setMinimumSize(560, 400);
    setModal(true);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Левый навигационный sidebar ───────────────────────────────────────
    m_nav = new QListWidget();
    m_nav->setObjectName("settingsNav");
    m_nav->setFixedWidth(150);
    m_nav->setFrameShape(QFrame::NoFrame);

    const struct { QString icon; QString label; } sections[] = {
        { "👤", "Профиль"   },
        { "🌐", "Сеть"      },
        { "🎨", "Интерфейс" },
    };
    for (const auto& s : sections) {
        auto* item = new QListWidgetItem(
            QString("  %1  %2").arg(s.icon, s.label));
        item->setSizeHint({150, 44});
        m_nav->addItem(item);
    }
    m_nav->setCurrentRow(0);

    // Разделитель
    auto* divider = new QFrame();
    divider->setFrameShape(QFrame::VLine);
    divider->setObjectName("settingsDivider");

    // ── Правая область — страницы ─────────────────────────────────────────
    m_stack = new QStackedWidget();
    m_stack->setObjectName("settingsStack");

    auto* profilePage   = new QWidget(); buildProfilePage(profilePage);
    auto* networkPage   = new QWidget(); buildNetworkPage(networkPage);
    auto* interfacePage = new QWidget(); buildInterfacePage(interfacePage);

    m_stack->addWidget(profilePage);
    m_stack->addWidget(networkPage);
    m_stack->addWidget(interfacePage);

    connect(m_nav, &QListWidget::currentRowChanged,
            m_stack, &QStackedWidget::setCurrentIndex);

    // ── Кнопки внизу ─────────────────────────────────────────────────────
    auto* rightCol = new QWidget();
    auto* rightLayout = new QVBoxLayout(rightCol);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    rightLayout->addWidget(m_stack, 1);

    auto* btnBar = new QWidget();
    btnBar->setObjectName("settingsBtnBar");
    auto* btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(20, 12, 20, 12);
    btnLayout->setSpacing(10);

    auto* resetBtn = new QPushButton("Сбросить");
    resetBtn->setObjectName("dlgCancelBtn");
    auto* saveBtn  = new QPushButton("Сохранить");
    saveBtn->setObjectName("dlgOkBtn");

    connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::onReset);
    connect(saveBtn,  &QPushButton::clicked, this, &SettingsDialog::onSave);

    btnLayout->addStretch();
    btnLayout->addWidget(resetBtn);
    btnLayout->addWidget(saveBtn);

    rightLayout->addWidget(btnBar);

    root->addWidget(m_nav);
    root->addWidget(divider);
    root->addWidget(rightCol, 1);

    loadCurrentValues();
}

// ── Страница: Профиль ─────────────────────────────────────────────────────

void SettingsDialog::buildProfilePage(QWidget* page) {
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 16);
    layout->setSpacing(20);

    auto* title = new QLabel("Профиль");
    title->setObjectName("settingsPageTitle");

    // Аватар-заглушка
    auto* avatarRow = new QHBoxLayout();
    auto* avatar = new QLabel();
    avatar->setObjectName("settingsAvatar");
    avatar->setFixedSize(64, 64);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setText(Identity::instance().displayName().left(1).toUpper());

    auto* avatarInfo = new QLabel("Аватар генерируется\nавтоматически из имени");
    avatarInfo->setObjectName("settingsHint");
    avatarRow->addWidget(avatar);
    avatarRow->addWidget(avatarInfo, 1);
    avatarRow->addStretch();

    // Имя
    auto* nameLabel = new QLabel("Отображаемое имя");
    nameLabel->setObjectName("settingsFieldLabel");
    m_nameEdit = new QLineEdit();
    m_nameEdit->setObjectName("settingsInput");
    m_nameEdit->setPlaceholderText("Как тебя зовут?");
    m_nameEdit->setMaxLength(32);

    auto* nameHint = new QLabel("Имя видят все кто подключается к тебе");
    nameHint->setObjectName("settingsHint");

    // UUID (readonly)
    auto* uuidLabel = new QLabel("Твой UUID");
    uuidLabel->setObjectName("settingsFieldLabel");
    auto* uuidEdit = new QLineEdit(
        Identity::instance().uuid().toString(QUuid::WithoutBraces));
    uuidEdit->setObjectName("settingsInputMono");
    uuidEdit->setReadOnly(true);

    layout->addWidget(title);
    layout->addLayout(avatarRow);
    layout->addWidget(nameLabel);
    layout->addWidget(m_nameEdit);
    layout->addWidget(nameHint);
    layout->addWidget(uuidLabel);
    layout->addWidget(uuidEdit);
    layout->addStretch();
}

// ── Страница: Сеть ────────────────────────────────────────────────────────

void SettingsDialog::buildNetworkPage(QWidget* page) {
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 16);
    layout->setSpacing(16);

    auto* title = new QLabel("Сеть");
    title->setObjectName("settingsPageTitle");

    // Порт
    auto* portLabel = new QLabel("Порт прослушивания");
    portLabel->setObjectName("settingsFieldLabel");
    m_portSpin = new QSpinBox();
    m_portSpin->setObjectName("settingsInput");
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(47821);

    auto* portHint = new QLabel(
        "Порт на котором мессенджер принимает входящие подключения.\n"
        "Изменение вступает в силу после перезапуска.");
    portHint->setObjectName("settingsHint");
    portHint->setWordWrap(true);

    // Привязка IP
    auto* ipLabel = new QLabel("IP для привязки (необязательно)");
    ipLabel->setObjectName("settingsFieldLabel");
    m_ipEdit = new QLineEdit();
    m_ipEdit->setObjectName("settingsInput");
    m_ipEdit->setPlaceholderText("0.0.0.0  (все интерфейсы)");

    auto* ipHint = new QLabel(
        "Оставь пустым для привязки ко всем интерфейсам.\n"
        "Укажи конкретный IP если хочешь слушать только на нём.");
    ipHint->setObjectName("settingsHint");
    ipHint->setWordWrap(true);

    // Статус прокси
    auto* proxyBox = new QWidget();
    proxyBox->setObjectName("settingsInfoBox");
    auto* proxyLayout = new QHBoxLayout(proxyBox);
    proxyLayout->setContentsMargins(14, 10, 14, 10);

    const auto currentProxy = QNetworkProxy::applicationProxy();
    const bool proxyActive = (currentProxy.type() != QNetworkProxy::NoProxy);

    auto* proxyIcon = new QLabel(proxyActive ? "⚠" : "✓");
    proxyIcon->setObjectName(proxyActive ? "settingsWarn" : "settingsOk");
    m_proxyStatus = new QLabel(
        proxyActive
        ? QString("Системный прокси обнаружен (%1:%2) — мессенджер его НЕ использует")
              .arg(currentProxy.hostName()).arg(currentProxy.port())
        : "Прямое подключение — прокси не используется");
    m_proxyStatus->setObjectName("settingsHint");
    m_proxyStatus->setWordWrap(true);

    proxyLayout->addWidget(proxyIcon);
    proxyLayout->addWidget(m_proxyStatus, 1);

    layout->addWidget(title);
    layout->addWidget(portLabel);
    layout->addWidget(m_portSpin);
    layout->addWidget(portHint);
    layout->addWidget(ipLabel);
    layout->addWidget(m_ipEdit);
    layout->addWidget(ipHint);
    layout->addWidget(proxyBox);
    layout->addStretch();
}

// ── Страница: Интерфейс ───────────────────────────────────────────────────

void SettingsDialog::buildInterfacePage(QWidget* page) {
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 16);
    layout->setSpacing(16);

    auto* title = new QLabel("Интерфейс");
    title->setObjectName("settingsPageTitle");

    // Тема — три кнопки
    auto* themeLabel = new QLabel("Тема оформления");
    themeLabel->setObjectName("settingsFieldLabel");

    auto* themeRow = new QWidget();
    auto* themeLayout = new QHBoxLayout(themeRow);
    themeLayout->setContentsMargins(0, 0, 0, 0);
    themeLayout->setSpacing(8);

    const struct { Theme theme; QString label; QString desc; } themes[] = {
        { Theme::Dark,  "◐  Тёмная",  "Тёмные тона" },
        { Theme::Light, "○  Светлая", "Яркие цвета" },
        { Theme::BW,    "●  Ч/Б",     "Монохром"    },
    };

    const Theme current = ThemeManager::instance().currentTheme();
    for (const auto& t : themes) {
        auto* btn = new QPushButton(
            QString("%1\n%2").arg(t.label, t.desc));
        btn->setObjectName("settingsThemeCard");
        btn->setCheckable(true);
        btn->setChecked(t.theme == current);
        btn->setFixedHeight(64);

        connect(btn, &QPushButton::clicked,
                this, [btn, themeRow, t]() {
            for (auto* b : themeRow->findChildren<QPushButton*>())
                b->setChecked(false);
            btn->setChecked(true);
            ThemeManager::instance().setTheme(t.theme);
        });

        connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
                btn, [btn, t](Theme newTheme) {
            btn->setChecked(newTheme == t.theme);
        });

        themeLayout->addWidget(btn, 1);
    }

    // Язык
    auto* langLabel = new QLabel("Язык интерфейса");
    langLabel->setObjectName("settingsFieldLabel");
    m_langCombo = new QComboBox();
    m_langCombo->setObjectName("settingsInput");
    m_langCombo->addItem("🇷🇺  Русский",  "ru");
    m_langCombo->addItem("🇬🇧  English",  "en");

    auto* langHint = new QLabel("Смена языка вступает в силу после перезапуска");
    langHint->setObjectName("settingsHint");

    layout->addWidget(title);
    layout->addWidget(themeLabel);
    layout->addWidget(themeRow);
    layout->addWidget(langLabel);
    layout->addWidget(m_langCombo);
    layout->addWidget(langHint);
    layout->addStretch();
}

// ── Загрузка текущих значений ─────────────────────────────────────────────

void SettingsDialog::loadCurrentValues() {
    QSettings s;

    // Профиль
    m_nameEdit->setText(Identity::instance().displayName());

    // Сеть
    m_portSpin->setValue(s.value("network/port", 47821).toInt());
    m_ipEdit->setText(s.value("network/bindIp", "").toString());

    // Язык
    const QString lang = s.value("ui/language", "ru").toString();
    const int idx = m_langCombo->findData(lang);
    if (idx >= 0) m_langCombo->setCurrentIndex(idx);
}

// ── Сохранение ────────────────────────────────────────────────────────────

void SettingsDialog::onSave() {
    QSettings s;

    // Профиль
    const QString name = m_nameEdit->text().trimmed();
    if (!name.isEmpty() && name != Identity::instance().displayName()) {
        Identity::instance().setDisplayName(name);
        emit nameChanged(name);
    }

    // Сеть
    const quint16 port = static_cast<quint16>(m_portSpin->value());
    const QString ip   = m_ipEdit->text().trimmed();
    s.setValue("network/port",   port);
    s.setValue("network/bindIp", ip);
    emit networkChanged(ip, port);

    // Язык
    const QString lang = m_langCombo->currentData().toString();
    s.setValue("ui/language", lang);
    emit languageChanged(lang);

    accept();
}

void SettingsDialog::onReset() {
    QSettings s;
    m_nameEdit->setText(Identity::instance().displayName());
    m_portSpin->setValue(s.value("network/port", 47821).toInt());
    m_ipEdit->setText(s.value("network/bindIp", "").toString());
}
