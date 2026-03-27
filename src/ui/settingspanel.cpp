#include "settingspanel.h"
#include "thememanager.h"
#include "logpanel.h"
#include "../core/identity.h"
#include "../core/updatechecker.h"
#include "../core/demomode.h"
#include "../core/sessionmanager.h"
#include <QMessageBox>
#include <QRegularExpression>
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
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QEvent>

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
    backBtn->setToolTip(tr("Back"));
    connect(backBtn, &QPushButton::clicked, this, &SettingsPanel::backRequested);

    auto* titleLbl = new QLabel(tr("Settings"));
    titleLbl->setObjectName("myNameLabel");

    auto* saveBtn = new QPushButton(tr("Save"));
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
    contentLayout->addWidget(sectionTitle(tr("Profile")));
    contentLayout->addSpacing(8);

    // Аватар
    auto* avatarRow = new QHBoxLayout();
    m_avatarLabel = new QLabel();
    m_avatarLabel->setObjectName("settingsAvatar");
    m_avatarLabel->setFixedSize(52, 52);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    // Делаем аватар кликабельным
    m_avatarLabel->setCursor(Qt::PointingHandCursor);
    m_avatarLabel->installEventFilter(this);

    m_changeAvatarBtn = new QPushButton(tr("Изменить аватар"));
    m_changeAvatarBtn->setObjectName("dlgCancelBtn");
    connect(m_changeAvatarBtn, &QPushButton::clicked,
            this, &SettingsPanel::onAvatarClicked);

    auto* avatarCol = new QVBoxLayout();
    avatarCol->setSpacing(4);
    avatarCol->addWidget(hint(tr("Нажмите на аватар или кнопку для выбора изображения")));
    avatarCol->addWidget(m_changeAvatarBtn);

    avatarRow->addWidget(m_avatarLabel);
    avatarRow->addLayout(avatarCol, 1);
    contentLayout->addLayout(avatarRow);
    contentLayout->addSpacing(12);

    contentLayout->addWidget(fieldLabel(tr("Display name")));
    m_nameEdit = new QLineEdit();
    m_nameEdit->setObjectName("settingsInput");
    m_nameEdit->setPlaceholderText(tr("What is your name?"));
    m_nameEdit->setMaxLength(32);
    // Буква аватара обновляется при вводе имени (если нет загруженного изображения)
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
        if (m_avatarLabel->pixmap().isNull())
            m_avatarLabel->setText(t.isEmpty() ? "?" : t.left(1).toUpper());
    });
    contentLayout->addWidget(m_nameEdit);
    contentLayout->addWidget(hint(tr("Everyone who connects to you will see this name")));
    contentLayout->addSpacing(8);

    contentLayout->addWidget(fieldLabel(tr("Your UUID")));
    m_uuidEdit = new QLineEdit();
    m_uuidEdit->setObjectName("settingsInputMono");
    m_uuidEdit->setReadOnly(true);

    // Кнопка скопировать строку подключения
    auto* copyConnRow = new QHBoxLayout();
    auto* copyConnBtn = new QPushButton(tr("Copy connection string"));
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
    contentLayout->addWidget(sectionTitle(tr("Demo mode")));
    contentLayout->addSpacing(6);

    auto* demoRow = new QHBoxLayout();
    auto* demoToggle = new QPushButton(tr("Enable demo mode"));
    demoToggle->setObjectName("demoToggleBtn");
    demoToggle->setCheckable(true);
    demoToggle->setChecked(DemoMode::instance().enabled());
    demoToggle->setText(DemoMode::instance().enabled()
        ? tr("Demo mode enabled") : tr("Enable demo mode"));

    connect(demoToggle, &QPushButton::clicked, this, [this, demoToggle](bool checked) {
        DemoMode::instance().setEnabled(checked);
        demoToggle->setText(checked ? tr("Demo mode enabled") : tr("Enable demo mode"));
    });
    connect(&DemoMode::instance(), &DemoMode::toggled,
            demoToggle, [this, demoToggle](bool on) {
        demoToggle->setChecked(on);
        demoToggle->setText(on ? tr("Demo mode enabled") : tr("Enable demo mode"));
    });

    demoRow->addWidget(demoToggle);
    demoRow->addStretch();
    contentLayout->addLayout(demoRow);
    contentLayout->addWidget(hint(
        tr("Hides your real data in UI.\n"
           "Name -> User-0000  |  UUID -> 00000...  |  IP -> 0.0.0.0\n"
           "The other party still sees your real data.")));

    contentLayout->addSpacing(8);
    contentLayout->addWidget(separator());
    contentLayout->addSpacing(8);

    // ── СЕТЬ ──────────────────────────────────────────────────────────────
    contentLayout->addWidget(sectionTitle(tr("Network")));
    contentLayout->addSpacing(8);

    contentLayout->addWidget(fieldLabel(tr("Port")));
    m_portSpin = new QSpinBox();
    m_portSpin->setObjectName("settingsInput");
    m_portSpin->setRange(1024, 65535);
    contentLayout->addWidget(m_portSpin);
    contentLayout->addWidget(hint(tr("Requires restart to take effect")));
    contentLayout->addSpacing(8);

    contentLayout->addWidget(fieldLabel(tr("Bind IP")));
    m_ipEdit = new QLineEdit();
    m_ipEdit->setObjectName("settingsInput");
    m_ipEdit->setPlaceholderText(tr("0.0.0.0  (all interfaces)"));
    contentLayout->addWidget(m_ipEdit);
    contentLayout->addWidget(hint(tr("Leave empty for all interfaces")));
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
        ? tr("Proxy %1:%2 — NOT used").arg(proxy.hostName()).arg(proxy.port())
        : tr("Direct connection"));
    m_proxyStatus->setObjectName("settingsHint");
    m_proxyStatus->setWordWrap(true);
    proxyLayout->addWidget(proxyIcon);
    proxyLayout->addWidget(m_proxyStatus, 1);
    contentLayout->addWidget(proxyBox);

    contentLayout->addSpacing(10);

    // ── РЕЖИМ ПРОБРОСА ПОРТОВ ─────────────────────────────────────────────
    contentLayout->addWidget(fieldLabel(tr("Режим проброса портов")));
    contentLayout->addSpacing(4);

    m_pfModeCombo = new QComboBox();
    m_pfModeCombo->setObjectName("settingsInput");
    m_pfModeCombo->addItem(tr("UPnP (автоматически)"),  static_cast<int>(PortForwardingMode::UpnpAuto));
    m_pfModeCombo->addItem(tr("Вручную (VPN / статический IP)"), static_cast<int>(PortForwardingMode::Manual));
    m_pfModeCombo->addItem(tr("Отключено (только локальная сеть)"), static_cast<int>(PortForwardingMode::Disabled));
    contentLayout->addWidget(m_pfModeCombo);

    // Контейнер полей для режима Manual — скрывается в других режимах
    m_manualFields = new QWidget();
    auto* manLayout = new QVBoxLayout(m_manualFields);
    manLayout->setContentsMargins(0, 6, 0, 0);
    manLayout->setSpacing(4);

    manLayout->addWidget(fieldLabel(tr("Публичный IP (IPv4)")));
    m_manualIpEdit = new QLineEdit();
    m_manualIpEdit->setObjectName("settingsInput");
    m_manualIpEdit->setPlaceholderText("203.0.113.42");
    manLayout->addWidget(m_manualIpEdit);

    manLayout->addSpacing(4);
    manLayout->addWidget(fieldLabel(tr("Внешний порт")));
    m_manualPortSpin = new QSpinBox();
    m_manualPortSpin->setObjectName("settingsInput");
    m_manualPortSpin->setRange(1024, 65535);
    m_manualPortSpin->setValue(47821);
    manLayout->addWidget(m_manualPortSpin);
    manLayout->addWidget(hint(
        tr("Укажите порт, пробрасываемый роутером на ваше устройство.\n"
           "Требуется перезапуск для применения изменений.")));

    m_manualFields->hide();  // скрыт по умолчанию (UPnP режим)
    contentLayout->addWidget(m_manualFields);

    // Подсказки для каждого режима (показываем одну активную)
    contentLayout->addWidget(hint(
        tr("UPnP — автоматический проброс портов через роутер.\n"
           "Manual — задайте IP и порт вручную (для VPN, static IP, ручного NAT).\n"
           "Disabled — только LAN, пиры подключаются напрямую по локальному IP.")));

    // Показываем/скрываем поля Manual при смене режима
    connect(m_pfModeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        const auto mode = static_cast<PortForwardingMode>(
            m_pfModeCombo->currentData().toInt());
        m_manualFields->setVisible(mode == PortForwardingMode::Manual);
    });

    contentLayout->addSpacing(8);
    contentLayout->addWidget(separator());
    contentLayout->addSpacing(8);

    // ── БЕЗОПАСНОСТЬ ──────────────────────────────────────────────────────
    contentLayout->addWidget(sectionTitle(tr("Security")));
    contentLayout->addSpacing(6);

    auto* shellRow = new QHBoxLayout();
    m_shellToggle = new QPushButton(tr("Allow remote shell access"));
    m_shellToggle->setObjectName("demoToggleBtn");
    m_shellToggle->setCheckable(true);
    m_shellToggle->setChecked(SessionManager::instance().remoteShellEnabled());
    m_shellToggle->setText(SessionManager::instance().remoteShellEnabled()
        ? tr("Remote shell allowed") : tr("Remote shell blocked"));

    connect(m_shellToggle, &QPushButton::clicked, this, [this](bool checked) {
        SessionManager::instance().setRemoteShellEnabled(checked);
        m_shellToggle->setText(checked ? tr("Remote shell allowed")
                                       : tr("Remote shell blocked"));
    });

    shellRow->addWidget(m_shellToggle);
    shellRow->addStretch();
    contentLayout->addLayout(shellRow);
    contentLayout->addWidget(hint(
        tr("Разрешить контактам запрашивать доступ к терминалу на вашем устройстве.\n"
           "При отключении все входящие запросы удалённого шелла отклоняются автоматически.")));

    contentLayout->addSpacing(8);
    contentLayout->addWidget(separator());
    contentLayout->addSpacing(8);

    // ── ИНТЕРФЕЙС ─────────────────────────────────────────────────────────
    contentLayout->addWidget(sectionTitle(tr("Interface")));
    contentLayout->addSpacing(8);

    contentLayout->addWidget(fieldLabel(tr("Theme")));
    contentLayout->addSpacing(4);

    // Комбобокс с 7 темами (заменяет 3 pill-кнопки)
    m_themeCombo = new QComboBox();
    m_themeCombo->setObjectName("settingsInput");
    m_themeCombo->addItem("◐  " + tr("Dark"),      "dark");
    m_themeCombo->addItem("○  " + tr("Light"),     "light");
    m_themeCombo->addItem("●  " + tr("B&W"),       "bw");
    m_themeCombo->addItem("🌲  " + tr("Forest"),   "forest");
    m_themeCombo->addItem("🌃  " + tr("Cyberpunk"), "cyberpunk");
    m_themeCombo->addItem("❄  " + tr("Nordic"),    "nordic");
    m_themeCombo->addItem("🌅  " + tr("Sunset"),   "sunset");

    // Устанавливаем текущую тему
    {
        const QString cur = SessionManager::instance().theme();
        const int idx = m_themeCombo->findData(cur.isEmpty() ? "dark" : cur);
        if (idx >= 0) m_themeCombo->setCurrentIndex(idx);
    }

    // Переключение темы при выборе в комбобоксе
    connect(m_themeCombo, &QComboBox::currentIndexChanged,
            this, [this](int) {
        const QString s = m_themeCombo->currentData().toString();
        Theme t = Theme::Dark;
        if      (s == "light")     t = Theme::Light;
        else if (s == "bw")        t = Theme::BW;
        else if (s == "forest")    t = Theme::Forest;
        else if (s == "cyberpunk") t = Theme::Cyberpunk;
        else if (s == "nordic")    t = Theme::Nordic;
        else if (s == "sunset")    t = Theme::Sunset;
        ThemeManager::instance().setTheme(t);
    });

    // Синхронизация комбобокса при внешней смене темы
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            m_themeCombo, [this](Theme t) {
        QString s = "dark";
        switch (t) {
            case Theme::Light:     s = "light";     break;
            case Theme::BW:        s = "bw";        break;
            case Theme::Forest:    s = "forest";    break;
            case Theme::Cyberpunk: s = "cyberpunk"; break;
            case Theme::Nordic:    s = "nordic";    break;
            case Theme::Sunset:    s = "sunset";    break;
            default: break;
        }
        const int idx = m_themeCombo->findData(s);
        if (idx >= 0 && m_themeCombo->currentIndex() != idx)
            m_themeCombo->setCurrentIndex(idx);
    });

    contentLayout->addWidget(m_themeCombo);
    contentLayout->addSpacing(12);

    contentLayout->addWidget(fieldLabel(tr("Language")));
    m_langCombo = new QComboBox();
    m_langCombo->setObjectName("settingsInput");
    m_langCombo->addItem(tr("Russian"), "ru");
    m_langCombo->addItem(tr("English"),  "en");
    contentLayout->addWidget(m_langCombo);
    contentLayout->addWidget(hint(tr("Requires restart")));

    contentLayout->addSpacing(8);
    contentLayout->addWidget(separator());
    contentLayout->addSpacing(8);

    // ── ОБНОВЛЕНИЯ ────────────────────────────────────────────────────────
    contentLayout->addWidget(sectionTitle(tr("Updates")));
    contentLayout->addSpacing(8);

    auto* versionRow = new QHBoxLayout();
    auto* verLabel = new QLabel(tr("Current version"));
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

    m_updateStatusLabel = new QLabel(tr("Press button to check"));
    m_updateStatusLabel->setObjectName("settingsHint");
    m_updateStatusLabel->setWordWrap(true);

    auto* checkBtn = new QPushButton(tr("Check for updates"));
    checkBtn->setObjectName("dlgCancelBtn");

    auto* checker = new UpdateChecker(this);

    connect(checkBtn, &QPushButton::clicked, this, [this, checkBtn, checker]() {
        checkBtn->setEnabled(false);
        checkBtn->setText(tr("Checking..."));
        m_updateStatusLabel->setText("");
        checker->checkNow();
    });

    connect(checker, &UpdateChecker::updateAvailable,
            this, [this, checkBtn](const UpdateInfo& info) {
        checkBtn->setEnabled(true);
        checkBtn->setText(tr("Check for updates"));
        m_lastCheckedLabel->setText(tr("Checked: ") + UpdateChecker().lastChecked());
        m_updateStatusLabel->setText(
            tr("New version available: <b>%1</b>").arg(info.version));

        auto* openBtn = new QPushButton(tr("Open release page"));
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
        checkBtn->setText(tr("Check for updates"));
        m_lastCheckedLabel->setText(tr("Checked: ") + UpdateChecker().lastChecked());
        m_updateStatusLabel->setText(
            tr("Version %1 is up to date").arg(ver));
    });

    connect(checker, &UpdateChecker::checkFailed,
            this, [this, checkBtn](const QString& err) {
        checkBtn->setEnabled(true);
        checkBtn->setText(tr("Check for updates"));
        m_updateStatusLabel->setText(tr("Error: ") + err);
    });

    contentLayout->addWidget(m_lastCheckedLabel);
    contentLayout->addWidget(checkBtn);
    contentLayout->addWidget(m_updateStatusLabel);

    contentLayout->addSpacing(8);
    contentLayout->addWidget(separator());
    contentLayout->addSpacing(8);

    // ── ОТЛАДКА ──────────────────────────────────────────────────────────────
    contentLayout->addWidget(sectionTitle(tr("Debug")));
    contentLayout->addSpacing(8);

    contentLayout->addWidget(hint(
        tr("Log of network events and errors. Enable verbose mode for more details.")));
    contentLayout->addSpacing(6);

    // Панель логов (растягивается по вертикали)
    m_logPanel = new LogPanel();
    m_logPanel->setMinimumHeight(200);
    contentLayout->addWidget(m_logPanel, 1);

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
    m_uuidEdit->setText(sm.uuid().toString(QUuid::WithoutBraces));
    m_portSpin->setValue(sm.port());
    m_ipEdit->setText(sm.bindIp());

    const int idx = m_langCombo->findData(sm.language());
    if (idx >= 0) m_langCombo->setCurrentIndex(idx);

    // Синхронизируем тему в комбобоксе
    const QString themeStr = sm.theme();
    const int themeIdx = m_themeCombo->findData(themeStr.isEmpty() ? "dark" : themeStr);
    if (themeIdx >= 0) m_themeCombo->setCurrentIndex(themeIdx);

    // Режим проброса портов — синхронизируем комбобокс и поля Manual
    {
        const int modeVal = static_cast<int>(sm.portForwardingMode());
        for (int i = 0; i < m_pfModeCombo->count(); ++i) {
            if (m_pfModeCombo->itemData(i).toInt() == modeVal) {
                m_pfModeCombo->setCurrentIndex(i);
                break;
            }
        }
        m_manualIpEdit->setText(sm.manualPublicIp());
        m_manualPortSpin->setValue(sm.manualPublicPort() > 0
                                   ? sm.manualPublicPort() : 47821);
        m_manualFields->setVisible(sm.portForwardingMode() == PortForwardingMode::Manual);
    }

    // Синхронизируем переключатель удалённого шелла
    m_shellToggle->setChecked(sm.remoteShellEnabled());
    m_shellToggle->setText(sm.remoteShellEnabled()
        ? tr("Remote shell allowed") : tr("Remote shell blocked"));

    // Загружаем аватар из кэша или показываем букву
    const QString avatarPath = sm.avatarPath();
    if (!avatarPath.isEmpty() && QFile::exists(avatarPath)) {
        applyAvatarPixmap(avatarPath);
    } else {
        m_avatarLabel->setPixmap({});
        m_avatarLabel->setText(name.isEmpty() ? "?" : name.left(1).toUpper());
    }
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

    // Режим проброса портов
    const auto pfMode = static_cast<PortForwardingMode>(
        m_pfModeCombo->currentData().toInt());

    if (pfMode == PortForwardingMode::Manual) {
        // Валидация IPv4-адреса перед сохранением
        static const QRegularExpression kIpv4Re(
            R"(^(\d{1,3}\.){3}\d{1,3}$)");
        const QString manIp = m_manualIpEdit->text().trimmed();
        if (!kIpv4Re.match(manIp).hasMatch()) {
            QMessageBox::warning(this, tr("Ошибка ввода"),
                tr("Некорректный формат IPv4-адреса.\n"
                   "Пример: 203.0.113.42"));
            return;  // не сохраняем и не закрываем панель
        }
        sm.setManualPublicIp(manIp);
        sm.setManualPublicPort(
            static_cast<quint16>(m_manualPortSpin->value()));
    }
    sm.setPortForwardingMode(pfMode);

    emit backRequested();
}

void SettingsPanel::onReset() {
    reload();
}

bool SettingsPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_avatarLabel && event->type() == QEvent::MouseButtonRelease) {
        onAvatarClicked();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void SettingsPanel::onAvatarClicked() {
    const QString file = QFileDialog::getOpenFileName(
        this,
        tr("Выбрать аватар"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        tr("Изображения (*.png *.jpg *.jpeg)"),
        nullptr,
        QFileDialog::DontUseNativeDialog);

    if (file.isEmpty()) return;

    // Масштабируем до 128×128 и сохраняем в кэш
    QPixmap src(file);
    if (src.isNull()) return;

    const QPixmap scaled = src.scaled(128, 128,
        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // Создаём каталог кэша если нет
    QDir cacheDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    cacheDir.mkpath(".");

    const QString savePath = cacheDir.filePath("avatar.png");
    if (!scaled.save(savePath, "PNG")) return;

    SessionManager::instance().setAvatarPath(savePath);
    applyAvatarPixmap(savePath);
}

void SettingsPanel::applyAvatarPixmap(const QString& path) {
    QPixmap src(path);
    if (src.isNull()) return;

    const int sz = m_avatarLabel->width();
    const QPixmap scaled = src.scaled(sz, sz,
        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // Рисуем с эллиптической маской
    QPixmap rounded(sz, sz);
    rounded.fill(Qt::transparent);
    QPainter p(&rounded);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path2;
    path2.addEllipse(0, 0, sz, sz);
    p.setClipPath(path2);
    p.drawPixmap(0, 0, scaled);

    m_avatarLabel->setText({});
    m_avatarLabel->setPixmap(rounded);
}
