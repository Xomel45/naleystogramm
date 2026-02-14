#include "settingspanel.h"
#include "thememanager.h"
#include "../core/identity.h"
#include "../core/updatechecker.h"
#include "../core/demomode.h"
#include "../core/sessionmanager.h"
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QSettings>
#include <QNetworkProxy>

// ── Helper: секция с заголовком ───────────────────────────────────────────

static QLabel* sectionTitle(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setObjectName("settingsPageTitle");
    return lbl;
}

static QLabel* fieldLabel(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setObjectName("settingsFieldLabel");
    return lbl;
}

static QLabel* hint(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setObjectName("settingsHint");
    lbl->setWordWrap(true);
    return lbl;
}

static QFrame* separator() {
    auto* f = new QFrame();
    f->setFrameShape(QFrame::HLine);
    f->setObjectName("settingsSeparator");
    return f;
}

// ── SettingsPanel ─────────────────────────────────────────────────────────

SettingsPanel::SettingsPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("settingsPanel");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ── Хедер с кнопкой назад ────────────────────────────────────────────
    auto* header = new QWidget();
    header->setObjectName("headerBar");
    header->setFixedHeight(62);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(10, 0, 14, 0);
    hl->setSpacing(8);

    auto* backBtn = new QPushButton("←");
    backBtn->setObjectName("iconBtn");
    backBtn->setFixedSize(32, 32);
    backBtn->setToolTip("Назад");
    connect(backBtn, &QPushButton::clicked, this, &SettingsPanel::backRequested);

    auto* titleLbl = new QLabel("Настройки");
    titleLbl->setObjectName("myNameLabel");

    auto* saveBtn = new QPushButton("Сохранить");
    saveBtn->setObjectName("dlgOkBtn");
    saveBtn->setFixedHeight(30);
    connect(saveBtn, &QPushButton::clicked, this, &SettingsPanel::onSave);

    hl->addWidget(backBtn);
    hl->addWidget(titleLbl, 1);
    hl->addWidget(saveBtn);

    // ── Скролл-область с контентом ───────────────────────────────────────
    auto* scroll = new QScrollArea();
    scroll->setObjectName("settingsScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget();
    content->setObjectName("settingsContent");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(16, 16, 16, 24);
    contentLayout->setSpacing(4);

    // ── ПРОФИЛЬ ───────────────────────────────────────────────────────────
    contentLayout->addWidget(sectionTitle("Профиль"));
    contentLayout->addSpacing(8);

    // Аватар
    auto* avatarRow = new QHBoxLayout();
    m_avatarLabel = new QLabel();
    m_avatarLabel->setObjectName("settingsAvatar");
    m_avatarLabel->setFixedSize(52, 52);
    m_avatarLabel->setAlignment(Qt::AlignCenter);

    auto* avatarCol = new QVBoxLayout();
    avatarCol->setSpacing(2);
    avatarCol->addWidget(new QLabel("Аватар из первой буквы имени") );
    avatarCol->addWidget(hint("Меняется автоматически"));

    avatarRow->addWidget(m_avatarLabel);
    avatarRow->addLayout(avatarCol, 1);
    contentLayout->addLayout(avatarRow);
    contentLayout->addSpacing(12);

    contentLayout->addWidget(fieldLabel("Отображаемое имя"));
    m_nameEdit = new QLineEdit();
    m_nameEdit->setObjectName("settingsInput");
    m_nameEdit->setPlaceholderText("Как тебя зовут?");
    m_nameEdit->setMaxLength(32);
    // Аватар обновляется при вводе имени
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
        m_avatarLabel->setText(t.isEmpty() ? "?" : t.left(1).toUpper());
    });
    contentLayout->addWidget(m_nameEdit);
    contentLayout->addWidget(hint("Имя видят все кто подключается к тебе"));
    contentLayout->addSpacing(8);

    contentLayout->addWidget(fieldLabel("Твой UUID"));
    m_uuidEdit = new QLineEdit();
    m_uuidEdit->setObjectName("settingsInputMono");
    m_uuidEdit->setReadOnly(true);

    // Кнопка скопировать строку подключения
    auto* copyConnRow = new QHBoxLayout();
    auto* copyConnBtn = new QPushButton("⊕  Скопировать строку подключения");
    copyConnBtn->setObjectName("dlgCancelBtn");
    connect(copyConnBtn, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(
            Identity::instance().connectionString("", 0));
    });
    copyConnRow->addWidget(copyConnBtn);
    copyConnRow->addStretch();

    contentLayout->addWidget(m_uuidEdit);
    contentLayout->addLayout(copyConnRow);

    contentLayout->addSpacing(8);
    contentLayout->addWidget(separator());
    contentLayout->addSpacing(8);

    // ── DEMO MODE ─────────────────────────────────────────────────────────
    contentLayout->addWidget(sectionTitle("Режим демонстрации"));
    contentLayout->addSpacing(6);

    auto* demoRow = new QHBoxLayout();
    auto* demoToggle = new QPushButton("Включить demo-mode");
    demoToggle->setObjectName("demoToggleBtn");
    demoToggle->setCheckable(true);
    demoToggle->setChecked(DemoMode::instance().enabled());
    demoToggle->setText(DemoMode::instance().enabled()
        ? "✓ Demo-mode включён" : "Включить demo-mode");

    connect(demoToggle, &QPushButton::clicked, this, [demoToggle](bool checked) {
        DemoMode::instance().setEnabled(checked);
        demoToggle->setText(checked ? "✓ Demo-mode включён" : "Включить demo-mode");
    });
    connect(&DemoMode::instance(), &DemoMode::toggled,
            demoToggle, [demoToggle](bool on) {
        demoToggle->setChecked(on);
        demoToggle->setText(on ? "✓ Demo-mode включён" : "Включить demo-mode");
    });

    demoRow->addWidget(demoToggle);
    demoRow->addStretch();
    contentLayout->addLayout(demoRow);
    contentLayout->addWidget(hint(
        "Скрывает твои реальные данные в UI.\n"
        "Имя → User-0000  •  UUID → 00000…  •  IP → 0.0.0.0\n"
        "Собеседник по-прежнему видит реальные данные."));

    contentLayout->addSpacing(8);
    contentLayout->addWidget(separator());
    contentLayout->addSpacing(8);

    // ── СЕТЬ ──────────────────────────────────────────────────────────────
    contentLayout->addWidget(sectionTitle("Сеть"));
    contentLayout->addSpacing(8);

    contentLayout->addWidget(fieldLabel("Порт"));
    m_portSpin = new QSpinBox();
    m_portSpin->setObjectName("settingsInput");
    m_portSpin->setRange(1024, 65535);
    contentLayout->addWidget(m_portSpin);
    contentLayout->addWidget(hint("Вступает в силу после перезапуска"));
    contentLayout->addSpacing(8);

    contentLayout->addWidget(fieldLabel("IP привязки"));
    m_ipEdit = new QLineEdit();
    m_ipEdit->setObjectName("settingsInput");
    m_ipEdit->setPlaceholderText("0.0.0.0  (все интерфейсы)");
    contentLayout->addWidget(m_ipEdit);
    contentLayout->addWidget(hint("Оставь пустым для всех интерфейсов"));
    contentLayout->addSpacing(8);

    // Статус прокси
    auto* proxyBox = new QWidget();
    proxyBox->setObjectName("settingsInfoBox");
    auto* proxyLayout = new QHBoxLayout(proxyBox);
    proxyLayout->setContentsMargins(12, 8, 12, 8);
    proxyLayout->setSpacing(8);

    const auto proxy = QNetworkProxy::applicationProxy();
    const bool hasProxy = (proxy.type() != QNetworkProxy::NoProxy);
    auto* proxyIcon = new QLabel(hasProxy ? "⚠" : "✓");
    proxyIcon->setObjectName(hasProxy ? "settingsWarn" : "settingsOk");
    m_proxyStatus = new QLabel(
        hasProxy
        ? QString("Прокси %1:%2 — НЕ используется")
              .arg(proxy.hostName()).arg(proxy.port())
        : "Прямое подключение");
    m_proxyStatus->setObjectName("settingsHint");
    m_proxyStatus->setWordWrap(true);
    proxyLayout->addWidget(proxyIcon);
    proxyLayout->addWidget(m_proxyStatus, 1);
    contentLayout->addWidget(proxyBox);

    contentLayout->addSpacing(8);
    contentLayout->addWidget(separator());
    contentLayout->addSpacing(8);

    // ── ИНТЕРФЕЙС ─────────────────────────────────────────────────────────
    contentLayout->addWidget(sectionTitle("Интерфейс"));
    contentLayout->addSpacing(8);

    contentLayout->addWidget(fieldLabel("Тема"));
    contentLayout->addSpacing(4);

    // Три карточки тем — вертикально (панель узкая 280px)
    const struct { Theme theme; QString icon; QString label; } themes[] = {
        { Theme::Dark,  "◐", "Тёмная"  },
        { Theme::Light, "○", "Светлая" },
        { Theme::BW,    "●", "Ч/Б"     },
    };

    auto* themeGroup = new QWidget();
    auto* themeLayout = new QVBoxLayout(themeGroup);
    themeLayout->setContentsMargins(0, 0, 0, 0);
    themeLayout->setSpacing(6);

    const Theme current = ThemeManager::instance().currentTheme();
    for (const auto& t : themes) {
        auto* btn = new QPushButton(
            QString("  %1   %2").arg(t.icon, t.label));
        btn->setObjectName("settingsThemeRow");
        btn->setCheckable(true);
        btn->setChecked(t.theme == current);
        btn->setFixedHeight(38);
        btn->setStyleSheet("text-align: left;");

        connect(btn, &QPushButton::clicked,
                this, [btn, themeGroup, t]() {
            for (auto* b : themeGroup->findChildren<QPushButton*>())
                b->setChecked(false);
            btn->setChecked(true);
            ThemeManager::instance().setTheme(t.theme);
        });
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
                btn, [btn, t](Theme nt) { btn->setChecked(nt == t.theme); });

        themeLayout->addWidget(btn);
    }
    contentLayout->addWidget(themeGroup);
    contentLayout->addSpacing(12);

    contentLayout->addWidget(fieldLabel("Язык"));
    m_langCombo = new QComboBox();
    m_langCombo->setObjectName("settingsInput");
    m_langCombo->addItem("🇷🇺  Русский", "ru");
    m_langCombo->addItem("🇬🇧  English",  "en");
    contentLayout->addWidget(m_langCombo);
    contentLayout->addWidget(hint("Нужен перезапуск"));

    contentLayout->addSpacing(8);
    contentLayout->addWidget(separator());
    contentLayout->addSpacing(8);

    // ── ОБНОВЛЕНИЯ ────────────────────────────────────────────────────────
    contentLayout->addWidget(sectionTitle("Обновления"));
    contentLayout->addSpacing(8);

    auto* versionRow = new QHBoxLayout();
    auto* verLabel = new QLabel("Текущая версия");
    verLabel->setObjectName("settingsFieldLabel");
    auto* verVal = new QLabel(UpdateChecker::kCurrentVersion);
    verVal->setObjectName("settingsHint");
    versionRow->addWidget(verLabel);
    versionRow->addStretch();
    versionRow->addWidget(verVal);
    contentLayout->addLayout(versionRow);
    contentLayout->addSpacing(6);

    m_lastCheckedLabel = new QLabel();
    m_lastCheckedLabel->setObjectName("settingsHint");

    m_updateStatusLabel = new QLabel("Нажми кнопку чтобы проверить");
    m_updateStatusLabel->setObjectName("settingsHint");
    m_updateStatusLabel->setWordWrap(true);

    auto* checkBtn = new QPushButton("↻  Проверить обновления");
    checkBtn->setObjectName("dlgCancelBtn");

    auto* checker = new UpdateChecker(this);

    connect(checkBtn, &QPushButton::clicked, this, [this, checkBtn, checker]() {
        checkBtn->setEnabled(false);
        checkBtn->setText("Проверяем...");
        m_updateStatusLabel->setText("");
        checker->checkNow();
    });

    connect(checker, &UpdateChecker::updateAvailable,
            this, [this, checkBtn](const UpdateInfo& info) {
        checkBtn->setEnabled(true);
        checkBtn->setText("↻  Проверить обновления");
        m_lastCheckedLabel->setText("Проверено: " + UpdateChecker().lastChecked());
        m_updateStatusLabel->setText(
            QString("🆕 Доступна версия <b>%1</b>").arg(info.version));

        auto* openBtn = new QPushButton("Открыть страницу релиза →");
        openBtn->setObjectName("dlgOkBtn");
        connect(openBtn, &QPushButton::clicked, this, [info]() {
            QDesktopServices::openUrl(QUrl(info.url));
        });
        // Добавляем кнопку динамически
        qobject_cast<QVBoxLayout*>(m_updateStatusLabel->parentWidget()->layout())
            ->addWidget(openBtn);
    });

    connect(checker, &UpdateChecker::noUpdateAvailable,
            this, [this, checkBtn](const QString& ver) {
        checkBtn->setEnabled(true);
        checkBtn->setText("↻  Проверить обновления");
        m_lastCheckedLabel->setText("Проверено: " + UpdateChecker().lastChecked());
        m_updateStatusLabel->setText(
            QString("✓ Версия %1 — актуальная").arg(ver));
    });

    connect(checker, &UpdateChecker::checkFailed,
            this, [this, checkBtn](const QString& err) {
        checkBtn->setEnabled(true);
        checkBtn->setText("↻  Проверить обновления");
        m_updateStatusLabel->setText("⚠ Ошибка: " + err);
    });

    contentLayout->addWidget(m_lastCheckedLabel);
    contentLayout->addWidget(checkBtn);
    contentLayout->addWidget(m_updateStatusLabel);

    contentLayout->addStretch();

    scroll->setWidget(content);

    // ── Footer с версией ──────────────────────────────────────────────────
    auto* versionFooter = new QWidget();
    versionFooter->setObjectName("settingsVersionFooter");
    auto* vfl = new QHBoxLayout(versionFooter);
    vfl->setContentsMargins(16, 8, 16, 10);

    auto* versionLbl = new QLabel(
        QString("naleystogramm  v%1").arg(UpdateChecker::kCurrentVersion));
    versionLbl->setObjectName("settingsVersionLabel");
    versionLbl->setAlignment(Qt::AlignCenter);

    vfl->addWidget(versionLbl);

    rootLayout->addWidget(header);
    rootLayout->addWidget(scroll, 1);
    rootLayout->addWidget(versionFooter);

    reload();
}

void SettingsPanel::reload() {
    auto& sm = SessionManager::instance();
    const QString name = sm.displayName();
    m_nameEdit->setText(name);
    m_avatarLabel->setText(name.isEmpty() ? "?" : name.left(1).toUpper());
    m_uuidEdit->setText(sm.uuid().toString(QUuid::WithoutBraces));
    m_portSpin->setValue(sm.port());
    m_ipEdit->setText(sm.bindIp());

    const int idx = m_langCombo->findData(sm.language());
    if (idx >= 0) m_langCombo->setCurrentIndex(idx);
}

void SettingsPanel::onSave() {
    auto& sm = SessionManager::instance();

    const QString name = m_nameEdit->text().trimmed();
    if (!name.isEmpty() && name != sm.displayName()) {
        Identity::instance().setDisplayName(name);
        emit nameChanged(name);
    }

    const quint16 port = static_cast<quint16>(m_portSpin->value());
    const QString ip   = m_ipEdit->text().trimmed();
    sm.setPort(port);
    sm.setBindIp(ip);
    emit networkChanged(ip, port);

    sm.setLanguage(m_langCombo->currentData().toString());

    emit backRequested();
}

void SettingsPanel::onReset() {
    reload();
}
