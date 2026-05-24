#include "settingspanel.h"
#include "thememanager.h"
#include "wheelfilter.h"
#include "customthememanager.h"
#include "logpanel.h"
#include "dialogs/devicepairingdialog.h"
#include "dialogs/devicelinkdialog.h"
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
#include <QStackedWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QCheckBox>
#include <QSettings>
#include <QNetworkProxy>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QCoreApplication>
#include <QTextEdit>
#include <QTimer>
#include <QIcon>

// ── Статические вспомогательные виджеты ──────────────────────────────────

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

// Создаёт прокручиваемую страницу и возвращает layout её содержимого
static QScrollArea* makePage(QVBoxLayout*& outLayout) {
    auto* scroll = new QScrollArea();
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setObjectName("settingsScroll");

    auto* content = new QWidget();
    content->setObjectName("settingsContent");
    outLayout = new QVBoxLayout(content);
    outLayout->setContentsMargins(16, 16, 16, 24);
    outLayout->setSpacing(4);

    scroll->setWidget(content);
    return scroll;
}

// ── SettingsPanel ─────────────────────────────────────────────────────────

SettingsPanel::SettingsPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("settingsOverlay");
    hide();
    if (parent) parent->installEventFilter(this);

    // ── Карточка ──────────────────────────────────────────────────────────
    m_card = new QWidget(this);
    m_card->setObjectName("settingsCard");
    m_card->setAttribute(Qt::WA_StyledBackground, true);

    auto* cardLayout = new QVBoxLayout(m_card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    // ── Хедер карточки ────────────────────────────────────────────────────
    auto* header = new QWidget(m_card);
    header->setObjectName("settingsCardHeader");
    header->setFixedHeight(56);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(8, 0, 12, 0);
    hl->setSpacing(6);

    m_backBtn = new QPushButton(header);
    m_backBtn->setObjectName("iconBtn");
    m_backBtn->setFixedSize(36, 36);
    m_backBtn->setToolTip(tr("Назад"));
    m_backBtn->setFlat(true);
    ThemeManager::applyIcon(m_backBtn, QStringLiteral(":/icons/nav_back.png"), QSize(18, 18));
    m_backBtn->setVisible(false);
    connect(m_backBtn, &QPushButton::clicked, this, &SettingsPanel::showMainPage);

    m_cardTitleLbl = new QLabel(tr("Настройки"), header);
    m_cardTitleLbl->setObjectName("settingsCardTitle");

    m_saveBtn = new QPushButton(tr("Сохранить"), header);
    m_saveBtn->setObjectName("dlgOkBtn");
    m_saveBtn->setMinimumWidth(100);
    m_saveBtn->setVisible(false);
    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsPanel::onSave);

    auto* closeBtn = new QPushButton(header);
    closeBtn->setObjectName("iconBtn");
    closeBtn->setFixedSize(32, 32);
    closeBtn->setToolTip(tr("Закрыть"));
    ThemeManager::applyIcon(closeBtn, QStringLiteral(":/icons/ctx_cancel.png"), QSize(16, 16));
    connect(closeBtn, &QPushButton::clicked, this, &SettingsPanel::closePanel);

    hl->addWidget(m_backBtn);
    hl->addWidget(m_cardTitleLbl, 1);
    hl->addWidget(m_saveBtn);
    hl->addWidget(closeBtn);

    // ── Стек страниц ──────────────────────────────────────────────────────
    m_pageStack = new QStackedWidget(m_card);
    m_pageStack->addWidget(buildMainPage());      // 0 — главная
    m_pageStack->addWidget(buildProfilePage());   // 1 — профиль
    m_pageStack->addWidget(buildNetworkPage());   // 2 — сеть
    m_pageStack->addWidget(buildDemoPage());      // 3 — демо-режим
    m_pageStack->addWidget(buildPrivacyPage());   // 4 — конфиденциальность
    m_pageStack->addWidget(buildSecurityPage());  // 5 — безопасность
    m_pageStack->addWidget(buildInterfacePage()); // 6 — интерфейс
    m_pageStack->addWidget(buildDevicesPage());   // 7 — связанные устройства
    m_pageStack->addWidget(buildUpdatesPage());   // 8 — обновления
    m_pageStack->addWidget(buildDebugPage());     // 9 — отладка

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

    cardLayout->addWidget(header);
    cardLayout->addWidget(m_pageStack, 1);
    cardLayout->addWidget(versionFooter);

    // ── Анимация карточки ─────────────────────────────────────────────────
    m_anim = new QPropertyAnimation(m_card, "pos", this);
    m_anim->setDuration(280);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QPropertyAnimation::finished, this, [this]() {
        if (m_closing) { hide(); m_closing = false; }
    });

    // ── Анимация затемнения/блюра ─────────────────────────────────────────
    m_fadeAnim = new QPropertyAnimation(this, "overlayOpacity", this);
    m_fadeAnim->setDuration(380);
    m_fadeAnim->setEasingCurve(QEasingCurve::OutQuad);

    // ── Тема ──────────────────────────────────────────────────────────────
    applyCardTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](Theme) { applyCardTheme(); });

    reload();
}

// ── Главная страница (навигация) ──────────────────────────────────────────

QWidget* SettingsPanel::buildMainPage() {
    auto* page = new QWidget();
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Карточка профиля (кнопка → страница Профиль)
    auto* profileBtn = new QPushButton();
    profileBtn->setObjectName("settingsMainProfile");
    profileBtn->setFlat(true);
    profileBtn->setFixedHeight(88);
    profileBtn->setCursor(Qt::PointingHandCursor);
    connect(profileBtn, &QPushButton::clicked, this, [this]() {
        showSection(1, tr("Мой аккаунт"), true);
    });

    auto* pcl = new QHBoxLayout(profileBtn);
    pcl->setContentsMargins(16, 14, 16, 14);
    pcl->setSpacing(14);

    m_mainPageAvatar = new QLabel();
    m_mainPageAvatar->setObjectName("settingsMainAvatar");
    m_mainPageAvatar->setFixedSize(52, 52);
    m_mainPageAvatar->setAlignment(Qt::AlignCenter);
    m_mainPageAvatar->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* nameCol = new QVBoxLayout();
    nameCol->setSpacing(3);
    nameCol->setContentsMargins(0, 0, 0, 0);

    m_mainPageName = new QLabel();
    m_mainPageName->setObjectName("settingsMainName");
    m_mainPageName->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_mainPageUuid = new QLabel();
    m_mainPageUuid->setObjectName("settingsMainUuid");
    m_mainPageUuid->setAttribute(Qt::WA_TransparentForMouseEvents);

    nameCol->addWidget(m_mainPageName);
    nameCol->addWidget(m_mainPageUuid);

    auto* profileChevron = new QLabel("›");
    profileChevron->setObjectName("settingsNavChevron");
    profileChevron->setAttribute(Qt::WA_TransparentForMouseEvents);

    pcl->addWidget(m_mainPageAvatar);
    pcl->addLayout(nameCol, 1);
    pcl->addWidget(profileChevron);

    vl->addWidget(profileBtn);
    vl->addWidget(separator());

    // Список разделов (прокручиваемый)
    auto* listScroll = new QScrollArea();
    listScroll->setFrameShape(QFrame::NoFrame);
    listScroll->setWidgetResizable(true);
    listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listScroll->setObjectName("settingsScroll");

    auto* sectionsWidget = new QWidget();
    auto* sl = new QVBoxLayout(sectionsWidget);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(0);

    // Строка раздела: иконка + текст + шеврон
    auto makeNavRow = [&](const QString& iconPath, const QString& text,
                           int pageIdx, const QString& title, bool hasSave) {
        auto* btn = new QPushButton();
        btn->setObjectName("settingsNavRow");
        btn->setFixedHeight(52);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);

        auto* rhl = new QHBoxLayout(btn);
        rhl->setContentsMargins(16, 0, 16, 0);
        rhl->setSpacing(12);

        auto* ico = new QLabel();
        ico->setPixmap(ThemeManager::tintedIcon(iconPath).pixmap(20, 20));
        ico->setFixedSize(26, 26);
        ico->setAlignment(Qt::AlignCenter);
        ico->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto* lbl = new QLabel(text);
        lbl->setObjectName("settingsNavRowText");
        lbl->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto* chev = new QLabel("›");
        chev->setObjectName("settingsNavChevron");
        chev->setAttribute(Qt::WA_TransparentForMouseEvents);

        rhl->addWidget(ico);
        rhl->addWidget(lbl, 1);
        rhl->addWidget(chev);

        connect(btn, &QPushButton::clicked, this, [this, pageIdx, title, hasSave]() {
            showSection(pageIdx, title, hasSave);
        });
        return btn;
    };

    sl->addWidget(makeNavRow(":/icons/settings_network.png",    tr("Сеть"),                  2, tr("Сеть"),                  true));
    sl->addWidget(separator());
    sl->addWidget(makeNavRow(":/icons/dialogs_lock_on.png",     tr("Демо-режим"),             3, tr("Демо-режим"),            false));
    sl->addWidget(separator());
    sl->addWidget(makeNavRow(":/icons/settings_privacy.png",    tr("Конфиденциальность"),    4, tr("Конфиденциальность"),    true));
    sl->addWidget(separator());
    sl->addWidget(makeNavRow(":/icons/settings_security.png",   tr("Безопасность"),           5, tr("Безопасность"),          false));
    sl->addWidget(separator());
    sl->addWidget(makeNavRow(":/icons/settings_appearance.png", tr("Интерфейс"),             6, tr("Интерфейс"),             true));
    sl->addWidget(separator());
    sl->addWidget(makeNavRow(":/icons/input_link_settings.png", tr("Связанные устройства"),  7, tr("Связанные устройства"),  false));
    sl->addWidget(separator());
    sl->addWidget(makeNavRow(":/icons/install_update.png",      tr("Обновления"),            8, tr("Обновления"),            false));
    sl->addWidget(separator());
    sl->addWidget(makeNavRow(":/icons/settings_advanced.png",   tr("Отладка"),               9, tr("Отладка"),               false));
    sl->addStretch();

    listScroll->setWidget(sectionsWidget);
    vl->addWidget(listScroll, 1);

    return page;
}

// ── Страница: Мой аккаунт ─────────────────────────────────────────────────

QScrollArea* SettingsPanel::buildProfilePage() {
    QVBoxLayout* lay;
    auto* scroll = makePage(lay);
    lay->setSpacing(0);

    auto& sm = SessionManager::instance();

    // ── Шапка: аватар + имя + ID ─────────────────────────────────────────
    auto* header = new QWidget();
    header->setObjectName("accountHeader");
    auto* headerLay = new QVBoxLayout(header);
    headerLay->setContentsMargins(16, 28, 16, 20);
    headerLay->setSpacing(6);
    headerLay->setAlignment(Qt::AlignHCenter);

    // Контейнер аватара — кнопка-камера абсолютно поверх
    auto* avatarWrap = new QWidget();
    avatarWrap->setFixedSize(96, 96);

    m_avatarLabel = new QLabel(avatarWrap);
    m_avatarLabel->setObjectName("settingsAvatar");
    m_avatarLabel->setFixedSize(90, 90);
    m_avatarLabel->move(0, 0);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setCursor(Qt::PointingHandCursor);
    m_avatarLabel->installEventFilter(this);

    m_changeAvatarBtn = new QPushButton(avatarWrap);
    m_changeAvatarBtn->setObjectName("settingsAvatarCam");
    m_changeAvatarBtn->setFixedSize(28, 28);
    m_changeAvatarBtn->move(90 - 26, 90 - 26);
    m_changeAvatarBtn->setIcon(ThemeManager::tintedIcon(":/icons/settings_photo.png"));
    m_changeAvatarBtn->setIconSize(QSize(16, 16));
    m_changeAvatarBtn->raise();
    connect(m_changeAvatarBtn, &QPushButton::clicked, this, &SettingsPanel::onAvatarClicked);

    m_profileNameLbl = new QLabel();
    m_profileNameLbl->setObjectName("accountName");
    m_profileNameLbl->setAlignment(Qt::AlignHCenter);

    auto* idLbl = new QLabel(sm.uuid().toString(QUuid::WithoutBraces).left(8) + "…");
    idLbl->setObjectName("accountId");
    idLbl->setAlignment(Qt::AlignHCenter);

    headerLay->addWidget(avatarWrap, 0, Qt::AlignHCenter);
    headerLay->addWidget(m_profileNameLbl);
    headerLay->addWidget(idLbl);
    lay->addWidget(header);

    // ── Разделители ───────────────────────────────────────────────────────
    auto mkSep = [&]() {
        auto* f = new QFrame();
        f->setFrameShape(QFrame::HLine);
        f->setObjectName("settingsSeparator");
        return f;
    };
    auto mkInsetSep = [&]() {
        auto* f = new QFrame();
        f->setFrameShape(QFrame::HLine);
        f->setObjectName("settingsInsetSeparator");
        return f;
    };

    // ── О себе ────────────────────────────────────────────────────────────
    lay->addWidget(mkSep());

    auto* bioSection = new QWidget();
    bioSection->setObjectName("accountSection");
    auto* bioLay = new QVBoxLayout(bioSection);
    bioLay->setContentsMargins(16, 10, 16, 10);
    bioLay->setSpacing(6);

    m_bioEdit = new QTextEdit();
    m_bioEdit->setObjectName("accountBio");
    m_bioEdit->setPlaceholderText(
        tr("Любые подробности о себе: возраст, род занятий или город.\n"
           "Пример: 25 лет, разработчик из Москвы."));
    m_bioEdit->setAcceptRichText(false);
    m_bioEdit->setFixedHeight(72);
    m_bioEdit->document()->setDocumentMargin(0);
    bioLay->addWidget(m_bioEdit);
    lay->addWidget(bioSection);

    // ── Информационные строки ─────────────────────────────────────────────
    lay->addWidget(mkSep());

    // Вспомогательная лямбда — строка с иконкой, меткой и QLineEdit справа
    auto makeInfoField = [&](const QString& iconPath, const QString& label,
                              QLineEdit*& outEdit, bool readOnly = false) -> QWidget* {
        auto* row = new QWidget();
        row->setObjectName("accountInfoRow");
        row->setFixedHeight(52);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(16, 0, 16, 0);
        rl->setSpacing(12);

        auto* ico = new QLabel();
        ico->setPixmap(ThemeManager::tintedIcon(iconPath).pixmap(20, 20));
        ico->setFixedSize(24, 24);
        ico->setAlignment(Qt::AlignCenter);
        ico->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto* lbl = new QLabel(label);
        lbl->setObjectName("accountInfoLabel");
        lbl->setAttribute(Qt::WA_TransparentForMouseEvents);

        outEdit = new QLineEdit();
        outEdit->setObjectName("accountInfoValue");
        outEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        outEdit->setReadOnly(readOnly);
        if (readOnly) {
            outEdit->setCursor(Qt::PointingHandCursor);
            outEdit->setToolTip(tr("Нажмите чтобы скопировать"));
        }
        outEdit->setFrame(false);
        outEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        rl->addWidget(ico);
        rl->addWidget(lbl);
        rl->addStretch();
        rl->addWidget(outEdit, 1);
        return row;
    };

    lay->addWidget(makeInfoField(":/icons/ctx_edit.png", tr("Имя"), m_nameEdit));
    lay->addWidget(mkInsetSep());
    lay->addWidget(makeInfoField(":/icons/nav_profile.png", tr("Мой ID"), m_uuidEdit, true));

    // Копирование ID по клику на поле
    connect(m_uuidEdit, &QLineEdit::cursorPositionChanged, this, [this]() {
        const QString full = SessionManager::instance().uuid().toString(QUuid::WithoutBraces);
        QApplication::clipboard()->setText(full);
        m_uuidEdit->setPlaceholderText(tr("Скопировано!"));
        QTimer::singleShot(1500, m_uuidEdit, [this]() {
            if (m_uuidEdit) m_uuidEdit->setPlaceholderText({});
        });
    });

    lay->addWidget(hint(tr("   Поделитесь своим ID — другие смогут подключиться к вам")));
    lay->addWidget(mkSep());

    // ── Скопировать строку подключения ────────────────────────────────────
    auto* connRow = new QPushButton();
    connRow->setObjectName("accountActionRow");
    connRow->setFlat(true);
    connRow->setCursor(Qt::PointingHandCursor);
    connRow->setFixedHeight(52);
    {
        auto* rl = new QHBoxLayout(connRow);
        rl->setContentsMargins(16, 0, 16, 0);
        rl->setSpacing(12);
        auto* ico = new QLabel();
        ico->setPixmap(ThemeManager::tintedIcon(":/icons/ctx_copy.png").pixmap(20, 20));
        ico->setFixedSize(24, 24);
        ico->setAlignment(Qt::AlignCenter);
        ico->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* lbl = new QLabel(tr("Скопировать строку подключения"));
        lbl->setObjectName("accountInfoLabel");
        lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        rl->addWidget(ico);
        rl->addWidget(lbl);
        rl->addStretch();
    }
    connect(connRow, &QPushButton::clicked, this, []() {
        QApplication::clipboard()->setText(Identity::instance().connectionString("", 0));
    });
    lay->addWidget(connRow);
    lay->addWidget(mkSep());

    lay->addStretch();
    return scroll;
}

// ── Страница: Сеть ───────────────────────────────────────────────────────

QScrollArea* SettingsPanel::buildNetworkPage() {
    QVBoxLayout* lay;
    auto* scroll = makePage(lay);

    m_portGroup = new QWidget();
    {
        auto* g = new QVBoxLayout(m_portGroup);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(4);
        g->addWidget(fieldLabel(tr("Port")));
        m_portSpin = new QSpinBox();
        m_portSpin->setObjectName("settingsInput");
        m_portSpin->setRange(1024, 65535);
        noScrollWheel(m_portSpin);
        g->addWidget(m_portSpin);
        g->addWidget(hint(tr("Requires restart to take effect")));
    }
    lay->addWidget(m_portGroup);
    lay->addSpacing(8);

    lay->addWidget(fieldLabel(tr("Bind IP")));
    m_ipEdit = new QLineEdit();
    m_ipEdit->setObjectName("settingsInput");
    m_ipEdit->setPlaceholderText(tr("0.0.0.0  (all interfaces)"));
    lay->addWidget(m_ipEdit);
    lay->addWidget(hint(tr("Leave empty for all interfaces")));
    lay->addSpacing(8);

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
    lay->addWidget(proxyBox);
    lay->addSpacing(10);

    lay->addWidget(fieldLabel(tr("Режим проброса портов")));
    lay->addSpacing(4);

    m_pfModeCombo = new QComboBox();
    m_pfModeCombo->setObjectName("settingsInput");
    noScrollWheel(m_pfModeCombo);
    m_pfModeCombo->addItem(tr("UPnP (автоматически)"),                  static_cast<int>(PortForwardingMode::UpnpAuto));
    m_pfModeCombo->addItem(tr("Разблокированный порт (ручной проброс)"), static_cast<int>(PortForwardingMode::OpenPort));
    m_pfModeCombo->addItem(tr("Вручную (VPN / статический IP)"),         static_cast<int>(PortForwardingMode::Manual));
    m_pfModeCombo->addItem(tr("Отключено (только локальная сеть)"),      static_cast<int>(PortForwardingMode::Disabled));
    m_pfModeCombo->addItem(tr("🖥 Client-Server (ретранслятор)"),        static_cast<int>(PortForwardingMode::ClientServer));
    lay->addWidget(m_pfModeCombo);

    m_manualFields = new QWidget();
    {
        auto* g = new QVBoxLayout(m_manualFields);
        g->setContentsMargins(0, 6, 0, 0);
        g->setSpacing(4);
        g->addWidget(fieldLabel(tr("Публичный IP (IPv4)")));
        m_manualIpEdit = new QLineEdit();
        m_manualIpEdit->setObjectName("settingsInput");
        m_manualIpEdit->setPlaceholderText("203.0.113.42");
        g->addWidget(m_manualIpEdit);
        g->addSpacing(4);
        g->addWidget(fieldLabel(tr("Внешний порт")));
        m_manualPortSpin = new QSpinBox();
        m_manualPortSpin->setObjectName("settingsInput");
        m_manualPortSpin->setRange(1024, 65535);
        m_manualPortSpin->setValue(47821);
        noScrollWheel(m_manualPortSpin);
        g->addWidget(m_manualPortSpin);
        g->addWidget(hint(tr("Укажите порт, пробрасываемый роутером на ваше устройство.\nТребуется перезапуск для применения изменений.")));
    }
    m_manualFields->hide();
    lay->addWidget(m_manualFields);

    m_openPortFields = new QWidget();
    {
        auto* g = new QVBoxLayout(m_openPortFields);
        g->setContentsMargins(0, 6, 0, 0);
        g->setSpacing(4);
        g->addWidget(fieldLabel(tr("Открытый (пробитый на роутере) порт")));
        m_openPortSpin = new QSpinBox();
        m_openPortSpin->setObjectName("settingsInput");
        m_openPortSpin->setRange(1024, 65535);
        m_openPortSpin->setValue(47821);
        noScrollWheel(m_openPortSpin);
        g->addWidget(m_openPortSpin);
        g->addWidget(hint(tr("⚠ Убедитесь что этот порт реально открыт и пробит в настройках роутера.\nТребуется перезапуск для применения изменений.")));
    }
    m_openPortFields->hide();
    lay->addWidget(m_openPortFields);

    m_relayFields = new QWidget();
    {
        auto* g = new QVBoxLayout(m_relayFields);
        g->setContentsMargins(0, 6, 0, 0);
        g->setSpacing(4);
        g->addWidget(fieldLabel(tr("IP-адрес relay-сервера")));
        m_relayIpEdit = new QLineEdit();
        m_relayIpEdit->setObjectName("settingsInput");
        m_relayIpEdit->setPlaceholderText("203.0.113.10");
        g->addWidget(m_relayIpEdit);
        g->addSpacing(4);
        g->addWidget(fieldLabel(tr("TCP-порт (сообщения)")));
        m_relayTcpPortSpin = new QSpinBox();
        m_relayTcpPortSpin->setObjectName("settingsInput");
        m_relayTcpPortSpin->setRange(1, 65535);
        m_relayTcpPortSpin->setValue(47822);
        noScrollWheel(m_relayTcpPortSpin);
        g->addWidget(m_relayTcpPortSpin);
        g->addSpacing(4);
        g->addWidget(fieldLabel(tr("UDP-порт (звонки)")));
        m_relayUdpPortSpin = new QSpinBox();
        m_relayUdpPortSpin->setObjectName("settingsInput");
        m_relayUdpPortSpin->setRange(1, 65535);
        m_relayUdpPortSpin->setValue(47823);
        noScrollWheel(m_relayUdpPortSpin);
        g->addWidget(m_relayUdpPortSpin);
        m_relayWarning = new QLabel(tr("⚠ Требуется перезапуск для применения изменений."));
        m_relayWarning->setObjectName("warningLabel");
        m_relayWarning->setWordWrap(true);
        g->addWidget(m_relayWarning);
    }
    m_relayFields->hide();
    lay->addWidget(m_relayFields);

    lay->addWidget(hint(
        tr("UPnP — автоматический проброс портов через роутер.\n"
           "Разблокированный порт — вы пробросили порт вручную, IP определяется автоматически.\n"
           "Вручную — задайте IP и порт вручную (для VPN, static IP, ручного NAT).\n"
           "Отключено — только LAN, пиры подключаются напрямую по локальному IP.\n"
           "Client-Server — все соединения через ваш relay-сервер (белый IP / VPS).")));

    connect(m_pfModeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        const auto mode = static_cast<PortForwardingMode>(m_pfModeCombo->currentData().toInt());
        m_portGroup->setVisible(mode != PortForwardingMode::OpenPort);
        m_manualFields->setVisible(mode == PortForwardingMode::Manual);
        m_openPortFields->setVisible(mode == PortForwardingMode::OpenPort);
        m_relayFields->setVisible(mode == PortForwardingMode::ClientServer);
    });

    lay->addStretch();
    return scroll;
}

// ── Страница: Демо-режим ─────────────────────────────────────────────────

QScrollArea* SettingsPanel::buildDemoPage() {
    QVBoxLayout* lay;
    auto* scroll = makePage(lay);

    auto* demoRow = new QHBoxLayout();
    auto* demoToggle = new QPushButton();
    demoToggle->setObjectName("demoToggleBtn");
    demoToggle->setCheckable(true);
    demoToggle->setChecked(DemoMode::instance().enabled());
    demoToggle->setText(DemoMode::instance().enabled()
        ? tr("Demo mode enabled") : tr("Enable demo mode"));

    connect(demoToggle, &QPushButton::clicked, this, [demoToggle](bool checked) {
        DemoMode::instance().setEnabled(checked);
        demoToggle->setText(checked ? tr("Demo mode enabled") : tr("Enable demo mode"));
    });
    connect(&DemoMode::instance(), &DemoMode::toggled, demoToggle, [demoToggle](bool on) {
        demoToggle->setChecked(on);
        demoToggle->setText(on ? tr("Demo mode enabled") : tr("Enable demo mode"));
    });

    demoRow->addWidget(demoToggle);
    demoRow->addStretch();
    lay->addLayout(demoRow);
    lay->addWidget(hint(
        tr("Hides your real data in UI.\n"
           "Name -> User-0000  |  UUID -> 00000...  |  IP -> 0.0.0.0\n"
           "The other party still sees your real data.")));
    lay->addStretch();
    return scroll;
}

// ── Страница: Конфиденциальность ─────────────────────────────────────────

QScrollArea* SettingsPanel::buildPrivacyPage() {
    QVBoxLayout* lay;
    auto* scroll = makePage(lay);

    auto makePrivacyRow = [&](const QString& label, QComboBox*& combo) {
        lay->addWidget(fieldLabel(label));
        lay->addSpacing(4);
        combo = new QComboBox();
        combo->addItem(tr("Все"),             static_cast<int>(PrivacyLevel::Everyone));
        combo->addItem(tr("Только контакты"), static_cast<int>(PrivacyLevel::ContactsOnly));
        combo->addItem(tr("Никто"),           static_cast<int>(PrivacyLevel::Nobody));
        noScrollWheel(combo);
        lay->addWidget(combo);
        lay->addSpacing(8);
    };

    makePrivacyRow(tr("Кто может писать"),           m_privacyMessages);
    makePrivacyRow(tr("Кто может отправлять файлы"), m_privacyFiles);
    makePrivacyRow(tr("Кто может звонить"),          m_privacyCalls);
    makePrivacyRow(tr("Кто может слать голосовые"),  m_privacyVoice);
    makePrivacyRow(tr("Кто видит аватар"),           m_privacyAvatar);
    makePrivacyRow(tr("Кто может запросить шелл"),   m_privacyShell);

    lay->addWidget(hint(
        tr("«Только контакты» — разрешает действие только от людей из вашего списка контактов.\n"
           "Изменения применяются после нажатия «Сохранить».")));
    lay->addStretch();
    return scroll;
}

// ── Страница: Безопасность ────────────────────────────────────────────────

QScrollArea* SettingsPanel::buildSecurityPage() {
    QVBoxLayout* lay;
    auto* scroll = makePage(lay);

    auto* shellRow = new QHBoxLayout();
    m_shellToggle = new QPushButton();
    m_shellToggle->setObjectName("demoToggleBtn");
    m_shellToggle->setCheckable(true);
    m_shellToggle->setChecked(SessionManager::instance().remoteShellEnabled());
    m_shellToggle->setText(SessionManager::instance().remoteShellEnabled()
        ? tr("Remote shell allowed") : tr("Remote shell blocked"));

    connect(m_shellToggle, &QPushButton::clicked, this, [this](bool checked) {
        SessionManager::instance().setRemoteShellEnabled(checked);
        m_shellToggle->setText(checked ? tr("Remote shell allowed") : tr("Remote shell blocked"));
    });

    shellRow->addWidget(m_shellToggle);
    shellRow->addStretch();
    lay->addLayout(shellRow);
    lay->addWidget(hint(
        tr("Разрешить контактам запрашивать доступ к терминалу на вашем устройстве.\n"
           "При отключении все входящие запросы удалённого шелла отклоняются автоматически.")));
    lay->addStretch();
    return scroll;
}

// ── Страница: Интерфейс ──────────────────────────────────────────────────

QScrollArea* SettingsPanel::buildInterfacePage() {
    QVBoxLayout* lay;
    auto* scroll = makePage(lay);

    lay->addWidget(fieldLabel(tr("Theme")));
    lay->addSpacing(4);

    m_themeCombo = new QComboBox();
    m_themeCombo->setObjectName("settingsInput");
    noScrollWheel(m_themeCombo);
    m_themeCombo->addItem("◐  " + tr("Dark"),       "dark");
    m_themeCombo->addItem("○  " + tr("Light"),      "light");
    m_themeCombo->addItem("●  " + tr("B&W"),        "bw");
    m_themeCombo->addItem("🌲  " + tr("Forest"),    "forest");
    m_themeCombo->addItem("🌃  " + tr("Cyberpunk"), "cyberpunk");
    m_themeCombo->addItem("❄  " + tr("Nordic"),     "nordic");
    m_themeCombo->addItem("🌅  " + tr("Sunset"),    "sunset");

    rebuildCustomThemeItems();

    {
        const QString cur = SessionManager::instance().theme();
        const QString key = cur.isEmpty() ? "dark" : cur;
        const int idx = m_themeCombo->findData(key);
        if (idx >= 0) m_themeCombo->setCurrentIndex(idx);
    }

    m_customRestartHint = hint(tr("Требуется перезапуск для применения темы"));
    m_customRestartHint->setVisible(false);

    connect(m_themeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        const QString s = m_themeCombo->currentData().toString();
        m_removeThemeBtn->setEnabled(s.startsWith("custom:"));

        if (s.startsWith("custom:")) {
            const QString folderName = s.mid(7);
            if (ThemeManager::instance().loadCustomTheme(folderName))
                ThemeManager::instance().applyCustomTheme();
            else
                QMessageBox::warning(this, tr("Ошибка темы"),
                    tr("Не удалось загрузить тему. Возможно, файл повреждён."));
            return;
        }

        Theme t = Theme::Dark;
        if      (s == "light")     t = Theme::Light;
        else if (s == "bw")        t = Theme::BW;
        else if (s == "forest")    t = Theme::Forest;
        else if (s == "cyberpunk") t = Theme::Cyberpunk;
        else if (s == "nordic")    t = Theme::Nordic;
        else if (s == "sunset")    t = Theme::Sunset;
        ThemeManager::instance().setTheme(t);
    });

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, m_themeCombo, [this](Theme t) {
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

    lay->addWidget(m_themeCombo);
    lay->addWidget(m_customRestartHint);

    m_importThemeBtn = new QPushButton(tr("Import theme..."));
    m_importThemeBtn->setObjectName("dlgCancelBtn");
    connect(m_importThemeBtn, &QPushButton::clicked, this, &SettingsPanel::onImportTheme);

    m_removeThemeBtn = new QPushButton(tr("Remove theme"));
    m_removeThemeBtn->setObjectName("dlgCancelBtn");
    m_removeThemeBtn->setEnabled(false);
    connect(m_removeThemeBtn, &QPushButton::clicked, this, &SettingsPanel::onRemoveTheme);

    auto* themeBtnRow = new QHBoxLayout();
    themeBtnRow->setSpacing(8);
    themeBtnRow->addWidget(m_importThemeBtn);
    themeBtnRow->addWidget(m_removeThemeBtn);
    themeBtnRow->addStretch();
    lay->addLayout(themeBtnRow);
    lay->addSpacing(12);

    lay->addWidget(fieldLabel(tr("Language")));
    m_langCombo = new QComboBox();
    m_langCombo->setObjectName("settingsInput");
    noScrollWheel(m_langCombo);
    m_langCombo->addItem(tr("Russian"), "ru");
    m_langCombo->addItem(tr("English"), "en");
    lay->addWidget(m_langCombo);
    lay->addWidget(hint(tr("Requires restart")));
    lay->addSpacing(12);

    m_enterSendsCheck = new QCheckBox(tr("Enter отправляет сообщение"));
    m_enterSendsCheck->setChecked(SessionManager::instance().enterSends());
    connect(m_enterSendsCheck, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        const bool checked = (state == Qt::Checked);
        SessionManager::instance().setEnterSends(checked);
        emit enterSendsChanged(checked);
    });
    lay->addWidget(m_enterSendsCheck);
    lay->addWidget(hint(tr("Shift+Enter — новая строка. "
                           "Если выключено: Enter — новая строка, Ctrl+Enter — отправить.")));
    lay->addStretch();
    return scroll;
}

// ── Страница: Связанные устройства ────────────────────────────────────────

QScrollArea* SettingsPanel::buildDevicesPage() {
    QVBoxLayout* lay;
    auto* scroll = makePage(lay);

    lay->addWidget(hint(
        tr("Link multiple devices to one account. "
           "The primary device holds all encrypted sessions; "
           "secondary devices relay through it.")));
    lay->addSpacing(8);

    auto* pairingBtnRow = new QHBoxLayout();
    pairingBtnRow->setSpacing(8);

    auto* primaryBtn = new QPushButton(tr("This is primary device"));
    primaryBtn->setObjectName("dlgCancelBtn");
    connect(primaryBtn, &QPushButton::clicked, this, [this]() {
        DevicePairingDialog dlg(this);
        (void)dlg.exec();
    });

    auto* secondaryBtn = new QPushButton(tr("Link to primary device"));
    secondaryBtn->setObjectName("dlgCancelBtn");
    connect(secondaryBtn, &QPushButton::clicked, this, [this]() {
        DeviceLinkDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted)
            emit connectToDeviceRequested(
                dlg.host(), static_cast<quint16>(dlg.port()), dlg.code());
    });

    pairingBtnRow->addWidget(primaryBtn);
    pairingBtnRow->addWidget(secondaryBtn);
    pairingBtnRow->addStretch();
    lay->addLayout(pairingBtnRow);
    lay->addStretch();
    return scroll;
}

// ── Страница: Обновления ──────────────────────────────────────────────────

QScrollArea* SettingsPanel::buildUpdatesPage() {
    QVBoxLayout* lay;
    auto* scroll = makePage(lay);

    auto* versionRow = new QHBoxLayout();
    auto* verLabel = new QLabel(tr("Current version"));
    verLabel->setObjectName("settingsFieldLabel");
    auto* verVal = new QLabel(UpdateChecker::kCurrentVersion);
    verVal->setObjectName("settingsHint");
    versionRow->addWidget(verLabel);
    versionRow->addStretch();
    versionRow->addWidget(verVal);
    lay->addLayout(versionRow);
    lay->addSpacing(6);

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

    connect(checker, &UpdateChecker::updateAvailable, this, [this, checkBtn](const UpdateInfo& info) {
        checkBtn->setEnabled(true);
        checkBtn->setText(tr("Check for updates"));
        m_lastCheckedLabel->setText(tr("Checked: ") + UpdateChecker().lastChecked());
        m_updateStatusLabel->setText(tr("New version available: <b>%1</b>").arg(info.version));

        auto* openBtn = new QPushButton(tr("Open release page"));
        openBtn->setObjectName("dlgOkBtn");
        connect(openBtn, &QPushButton::clicked, this, [info]() {
            QDesktopServices::openUrl(QUrl(info.url));
        });
        qobject_cast<QVBoxLayout*>(m_updateStatusLabel->parentWidget()->layout())->addWidget(openBtn);
    });

    connect(checker, &UpdateChecker::noUpdateAvailable, this, [this, checkBtn](const QString& ver) {
        checkBtn->setEnabled(true);
        checkBtn->setText(tr("Check for updates"));
        m_lastCheckedLabel->setText(tr("Checked: ") + UpdateChecker().lastChecked());
        m_updateStatusLabel->setText(tr("Version %1 is up to date").arg(ver));
    });

    connect(checker, &UpdateChecker::checkFailed, this, [this, checkBtn](const QString& err) {
        checkBtn->setEnabled(true);
        checkBtn->setText(tr("Check for updates"));
        m_updateStatusLabel->setText(tr("Error: ") + err);
    });

    lay->addWidget(m_lastCheckedLabel);
    lay->addWidget(checkBtn);
    lay->addWidget(m_updateStatusLabel);
    lay->addStretch();
    return scroll;
}

// ── Страница: Отладка ─────────────────────────────────────────────────────

QScrollArea* SettingsPanel::buildDebugPage() {
    QVBoxLayout* lay;
    auto* scroll = makePage(lay);

    lay->addWidget(hint(
        tr("Log of network events and errors. Enable verbose mode for more details.")));
    lay->addSpacing(6);

    m_logPanel = new LogPanel();
    m_logPanel->setMinimumHeight(200);
    lay->addWidget(m_logPanel, 1);
    connect(m_logPanel, &LogPanel::verboseChanged, this, &SettingsPanel::verboseLoggingChanged);

    lay->addStretch();
    return scroll;
}

// ── Навигация ─────────────────────────────────────────────────────────────

void SettingsPanel::showMainPage() {
    m_pageStack->setCurrentIndex(0);
    m_cardTitleLbl->setText(tr("Настройки"));
    m_backBtn->setVisible(false);
    m_saveBtn->setVisible(false);
}

void SettingsPanel::showSection(int pageIdx, const QString& title, bool hasSave) {
    m_pageStack->setCurrentIndex(pageIdx);
    m_cardTitleLbl->setText(title);
    m_backBtn->setVisible(true);
    m_saveBtn->setVisible(hasSave);
}

void SettingsPanel::reload() {
    auto& sm = SessionManager::instance();
    const QString name = sm.displayName();
    m_nameEdit->setText(name);
    m_uuidEdit->setText(sm.uuid().toString(QUuid::WithoutBraces).left(8) + "…");
    m_bioEdit->setPlainText(sm.bio());
    if (m_profileNameLbl)
        m_profileNameLbl->setText(name.isEmpty() ? tr("(без имени)") : name);
    m_portSpin->setValue(sm.port());
    m_ipEdit->setText(sm.bindIp());

    const int idx = m_langCombo->findData(sm.language());
    if (idx >= 0) m_langCombo->setCurrentIndex(idx);

    // Пересканируем пользовательские темы (могли добавиться после открытия настроек)
    rebuildCustomThemeItems();

    // Синхронизируем тему в комбобоксе
    const QString themeStr = sm.theme();
    const QString themeKey = themeStr.isEmpty() ? "dark" : themeStr;
    const int themeIdx = m_themeCombo->findData(themeKey);
    if (themeIdx >= 0) m_themeCombo->setCurrentIndex(themeIdx);
    m_removeThemeBtn->setEnabled(themeKey.startsWith("custom:"));

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

        m_openPortSpin->setValue(sm.manualPublicPort() > 0
                                 ? sm.manualPublicPort() : 47821);
        m_openPortFields->setVisible(sm.portForwardingMode() == PortForwardingMode::OpenPort);
        m_portGroup->setVisible(sm.portForwardingMode() != PortForwardingMode::OpenPort);

        m_relayIpEdit->setText(sm.relayServerIp());
        m_relayTcpPortSpin->setValue(sm.relayTcpPort() > 0 ? sm.relayTcpPort() : 47822);
        m_relayUdpPortSpin->setValue(sm.relayUdpPort() > 0 ? sm.relayUdpPort() : 47823);
        m_relayFields->setVisible(sm.portForwardingMode() == PortForwardingMode::ClientServer);
    }

    // Синхронизируем переключатель удалённого шелла
    m_shellToggle->setChecked(sm.remoteShellEnabled());
    m_shellToggle->setText(sm.remoteShellEnabled()
        ? tr("Remote shell allowed") : tr("Remote shell blocked"));

    // Синхронизируем настройки конфиденциальности
    auto syncPrivacyCombo = [](QComboBox* combo, PrivacyLevel level) {
        const int val = static_cast<int>(level);
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toInt() == val) {
                combo->setCurrentIndex(i);
                return;
            }
        }
    };
    syncPrivacyCombo(m_privacyMessages, sm.privacyMessages());
    syncPrivacyCombo(m_privacyFiles,    sm.privacyFiles());
    syncPrivacyCombo(m_privacyCalls,    sm.privacyCalls());
    syncPrivacyCombo(m_privacyVoice,    sm.privacyVoice());
    syncPrivacyCombo(m_privacyAvatar,   sm.privacyAvatar());
    syncPrivacyCombo(m_privacyShell,    sm.privacyShell());

    // Синхронизируем переключатель Enter-отправки
    if (m_enterSendsCheck)
        m_enterSendsCheck->setChecked(sm.enterSends());

    // Загружаем аватар из кэша или показываем букву (обновляем оба лейбла)
    const QString avatarPath = sm.avatarPath();
    if (!avatarPath.isEmpty() && QFile::exists(avatarPath)) {
        applyAvatarPixmap(avatarPath);

        // Круглый аватар для главной страницы (52px)
        QPixmap src(avatarPath);
        const int sz = 52;
        QPixmap round(sz, sz);
        round.fill(Qt::transparent);
        QPainter pr(&round);
        pr.setRenderHint(QPainter::Antialiasing);
        QPainterPath pp;
        pp.addEllipse(0.0, 0.0, sz, sz);
        pr.setClipPath(pp);
        pr.drawPixmap(0, 0, src.scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        m_mainPageAvatar->setPixmap(round);
        m_mainPageAvatar->setText({});
    } else {
        m_avatarLabel->setPixmap({});
        m_avatarLabel->setText(name.isEmpty() ? "?" : name.left(1).toUpper());
        m_mainPageAvatar->setPixmap({});
        m_mainPageAvatar->setText(name.isEmpty() ? "?" : name.left(1).toUpper());
    }

    m_mainPageName->setText(name.isEmpty() ? tr("(без имени)") : name);
    m_mainPageUuid->setText(sm.uuid().toString(QUuid::WithoutBraces).left(8) + "…");
}

void SettingsPanel::onSave() {
    auto& sm = SessionManager::instance();

    const QString name = m_nameEdit->text().trimmed();
    if (!name.isEmpty() && name != sm.displayName()) {
        Identity::instance().setDisplayName(name);
        emit nameChanged(name);
    }

    const QString bio = m_bioEdit->toPlainText().trimmed();
    if (bio != sm.bio())
        sm.setBio(bio);

    const quint16 port = static_cast<quint16>(m_portSpin->value());
    const QString ip   = m_ipEdit->text().trimmed();
    sm.setPort(port);
    sm.setBindIp(ip);
    emit networkChanged(ip, port);

    sm.setLanguage(m_langCombo->currentData().toString());

    // Режим проброса портов
    const auto pfMode = static_cast<PortForwardingMode>(
        m_pfModeCombo->currentData().toInt());

    static const QRegularExpression kIpv4Re(R"(^(\d{1,3}\.){3}\d{1,3}$)");

    if (pfMode == PortForwardingMode::Manual) {
        const QString manIp = m_manualIpEdit->text().trimmed();
        if (!kIpv4Re.match(manIp).hasMatch()) {
            QMessageBox::warning(this, tr("Ошибка ввода"),
                tr("Некорректный формат IPv4-адреса.\n"
                   "Пример: 203.0.113.42"));
            return;
        }
        sm.setManualPublicIp(manIp);
        sm.setManualPublicPort(static_cast<quint16>(m_manualPortSpin->value()));
    }

    if (pfMode == PortForwardingMode::ClientServer) {
        const QString relayIp = m_relayIpEdit->text().trimmed();
        if (relayIp.isEmpty() || !kIpv4Re.match(relayIp).hasMatch()) {
            QMessageBox::warning(this, tr("Ошибка ввода"),
                tr("Укажите корректный IPv4-адрес relay-сервера.\n"
                   "Пример: 203.0.113.10"));
            return;
        }
        sm.setRelayServerIp(relayIp);
        sm.setRelayTcpPort(static_cast<quint16>(m_relayTcpPortSpin->value()));
        sm.setRelayUdpPort(static_cast<quint16>(m_relayUdpPortSpin->value()));
    }

    sm.setPortForwardingMode(pfMode);
    if (pfMode == PortForwardingMode::OpenPort) {
        const quint16 openPort = static_cast<quint16>(m_openPortSpin->value());
        sm.setManualPublicPort(openPort);
        sm.setPort(openPort);  // локальный порт = внешний порт, один на всё
    }

    sm.setPrivacyMessages(static_cast<PrivacyLevel>(m_privacyMessages->currentData().toInt()));
    sm.setPrivacyFiles   (static_cast<PrivacyLevel>(m_privacyFiles->currentData().toInt()));
    sm.setPrivacyCalls   (static_cast<PrivacyLevel>(m_privacyCalls->currentData().toInt()));
    sm.setPrivacyVoice   (static_cast<PrivacyLevel>(m_privacyVoice->currentData().toInt()));
    sm.setPrivacyAvatar  (static_cast<PrivacyLevel>(m_privacyAvatar->currentData().toInt()));
    sm.setPrivacyShell   (static_cast<PrivacyLevel>(m_privacyShell->currentData().toInt()));

    // Обновляем главную страницу и шапку аккаунта, возвращаемся на главную
    const QString savedName = m_nameEdit->text().trimmed();
    m_mainPageName->setText(savedName);
    if (m_profileNameLbl)
        m_profileNameLbl->setText(savedName.isEmpty() ? tr("(без имени)") : savedName);
    showMainPage();
}

void SettingsPanel::onReset() {
    reload();
}

bool SettingsPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible()) {
        setGeometry(parentWidget()->rect());
        updateCardGeometry();
        update();
    }
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
    emit avatarChanged(savePath);
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

// ── Пользовательские темы ─────────────────────────────────────────────────

void SettingsPanel::rebuildCustomThemeItems() {
    // Удаляем все пункты начиная с сепаратора (индекс 7 и далее)
    while (m_themeCombo->count() > 7)
        m_themeCombo->removeItem(m_themeCombo->count() - 1);

    const QList<CustomThemeMeta> customs = CustomThemeManager::scan();
    if (customs.isEmpty()) return;

    m_themeCombo->insertSeparator(m_themeCombo->count());
    for (const CustomThemeMeta& meta : customs) {
        const QString label = "🎨  " + meta.displayName
                              + (meta.author.isEmpty() ? "" : " (" + meta.author + ")");
        m_themeCombo->addItem(label, "custom:" + meta.folderName);
    }
}

void SettingsPanel::onImportTheme() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Импортировать тему"),
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
        tr("Архивы тем (*.zip *.tar.gz *.tgz *.7z)"),
        nullptr,
        QFileDialog::DontUseNativeDialog);

    if (path.isEmpty()) return;

    QString error;
    QString folderName;
    if (!CustomThemeManager::importArchive(path, error, &folderName)) {
        QMessageBox::critical(this, tr("Ошибка импорта"), error);
        return;
    }

    rebuildCustomThemeItems();

    // Автовыбор и немедленное применение импортированной темы
    const int idx = m_themeCombo->findData("custom:" + folderName);
    if (idx >= 0)
        m_themeCombo->setCurrentIndex(idx);  // currentIndexChanged применяет тему
}

// ── Overlay: openPanel / closePanel / paint / resize / mouse ─────────────────

void SettingsPanel::openPanel() {
    QWidget* p = parentWidget();
    if (!p) return;

    // Если идёт анимация закрытия — разворачиваем её на месте
    if (isVisible() && m_closing) {
        m_closing = false;
        m_anim->stop();
        updateCardGeometry();
        const int targetY = (height() - m_card->height()) / 2;
        m_anim->setStartValue(m_card->pos());
        m_anim->setEndValue(QPoint((width() - m_card->width()) / 2, targetY));
        m_anim->start();
        m_fadeAnim->stop();
        m_fadeAnim->setStartValue(m_overlayOpacity);
        m_fadeAnim->setEndValue(1.0);
        m_fadeAnim->start();
        return;
    }

    // Если уже открыта (не закрывается) — ничего не делаем
    if (isVisible()) return;

    reload();
    showMainPage();

    // Захватываем фон — явно скрываем дочерние виджеты с objectName "sideDrawer",
    // чтобы они не попали в блюр даже при гонке с анимацией закрытия
    QList<QWidget*> hiddenForGrab;
    for (auto* child : p->findChildren<QWidget*>()) {
        if (child->objectName() == QLatin1String("sideDrawer") && child->isVisible()) {
            child->hide();
            hiddenForGrab.append(child);
        }
    }
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QPixmap grab(p->size());
    grab.fill(Qt::black);
    p->render(&grab);

    for (auto* w : hiddenForGrab) w->show();

    const int f = 14;
    const QSize small(qMax(1, grab.width() / f), qMax(1, grab.height() / f));
    m_blurredBg = grab.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                      .scaled(grab.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    setGeometry(p->rect());
    updateCardGeometry();

    // Карточка стартует выше экрана
    const int targetY = m_card->y();
    m_card->move(m_card->x(), -m_card->height());

    m_closing = false;
    m_overlayOpacity = 0.0;
    show();
    raise();
    update();

    m_anim->stop();
    m_anim->setStartValue(m_card->pos());
    m_anim->setEndValue(QPoint(m_card->x(), targetY));
    m_anim->start();

    m_fadeAnim->stop();
    m_fadeAnim->setStartValue(0.0);
    m_fadeAnim->setEndValue(1.0);
    m_fadeAnim->start();
}

void SettingsPanel::closePanel() {
    if (!isVisible() || m_closing) return;
    m_closing = true;

    m_anim->stop();
    m_anim->setStartValue(m_card->pos());
    m_anim->setEndValue(QPoint(m_card->x(), -m_card->height()));
    m_anim->start();

    m_fadeAnim->stop();
    m_fadeAnim->setStartValue(m_overlayOpacity);
    m_fadeAnim->setEndValue(0.0);
    m_fadeAnim->start();
}

void SettingsPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setOpacity(m_overlayOpacity);
    if (!m_blurredBg.isNull())
        p.drawPixmap(rect(), m_blurredBg);
    else
        p.fillRect(rect(), QColor(0, 0, 0, 200));
    p.fillRect(rect(), QColor(0, 0, 0, 130));
}

void SettingsPanel::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);
    if (m_card) updateCardGeometry();
}

void SettingsPanel::mousePressEvent(QMouseEvent* ev) {
    if (m_card && !m_card->geometry().contains(ev->pos()))
        closePanel();
    else
        QWidget::mousePressEvent(ev);
}

void SettingsPanel::updateCardGeometry() {
    if (!m_card) return;
    const int margin = 36;
    const int w = qMin(600, width() - margin * 2);
    const int h = qMin(height() - margin * 2, 820);
    const int x = (width() - w) / 2;
    const int y = (height() - h) / 2;
    m_card->setGeometry(x, y, w, h);
}

void SettingsPanel::applyCardTheme() {
    if (!m_card) return;
    const auto& p = ThemeManager::instance().palette();
    const QString hover = p.bgElevated;
    m_card->setStyleSheet(QString(R"(
        QWidget#settingsCard {
            background: %1;
            border: 1px solid %2;
        }
        QWidget#settingsCardHeader {
            background: %3;
            border-bottom: 1px solid %2;
        }
        QLabel#settingsCardTitle {
            color: %4;
            font-size: 16px;
            font-weight: 700;
        }
        QPushButton#settingsMainProfile {
            background: %3;
            border: none;
            text-align: left;
        }
        QPushButton#settingsMainProfile:hover {
            background: %5;
        }
        QLabel#settingsMainName {
            color: %4;
            font-size: 15px;
            font-weight: 600;
        }
        QLabel#settingsMainUuid {
            color: %6;
            font-size: 11px;
        }
        QLabel#settingsMainAvatar {
            color: %4;
            background: %7;
            border-radius: 26px;
            font-size: 20px;
            font-weight: 700;
        }
        QLabel#settingsAvatar {
            color: %4;
            background: %7;
            border-radius: 45px;
            font-size: 28px;
            font-weight: 700;
        }
        QPushButton#settingsNavRow {
            background: transparent;
            border: none;
            text-align: left;
        }
        QPushButton#settingsNavRow:hover {
            background: %5;
        }
        QLabel#settingsNavRowText {
            color: %4;
            font-size: 14px;
        }
        QLabel#settingsNavChevron {
            color: %6;
            font-size: 20px;
        }

        QWidget#accountHeader {
            background: %3;
        }
        QLabel#accountName {
            color: %4;
            font-size: 17px;
            font-weight: 700;
        }
        QLabel#accountId {
            color: %6;
            font-size: 12px;
        }
        QWidget#accountSection {
            background: %1;
        }
        QTextEdit#accountBio {
            background: transparent;
            color: %4;
            font-size: 14px;
            border: none;
            padding: 0;
        }
        QWidget#accountInfoRow {
            background: %1;
        }
        QWidget#accountInfoRow:hover {
            background: %5;
        }
        QLabel#accountInfoLabel {
            color: %4;
            font-size: 14px;
        }
        QLineEdit#accountInfoValue {
            background: transparent;
            color: %6;
            font-size: 14px;
            border: none;
            padding: 0;
        }
        QPushButton#accountActionRow {
            background: %1;
            border: none;
            text-align: left;
        }
        QPushButton#accountActionRow:hover {
            background: %5;
        }
        QPushButton#settingsAvatarCam {
            background: %7;
            border-radius: 14px;
            border: 2px solid %3;
        }
        QPushButton#settingsAvatarCam:hover {
            background: %8;
        }
        QFrame#settingsInsetSeparator {
            color: %2;
            margin-left: 52px;
        }
    )").arg(p.bgSurface, p.border, p.bgElevated, p.textPrimary,
            hover, p.textMuted, p.accent, p.bgElevated));
}

void SettingsPanel::onRemoveTheme() {
    const QString s = m_themeCombo->currentData().toString();
    if (!s.startsWith("custom:")) return;

    const QString folderName = s.mid(7);
    const QString displayName = ThemeManager::instance().customThemeDisplayName();

    const int ret = QMessageBox::question(this,
        tr("Удалить тему"),
        tr("Удалить тему \"%1\"?").arg(displayName),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    // Переключаемся на Dark перед удалением активной темы
    if (ThemeManager::instance().currentTheme() == Theme::Custom)
        m_themeCombo->setCurrentIndex(0);

    if (!CustomThemeManager::removeTheme(folderName)) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось удалить тему"));
        return;
    }

    rebuildCustomThemeItems();
}
