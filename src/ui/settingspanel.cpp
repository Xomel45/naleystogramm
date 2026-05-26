#include "settingspanel.h"
#include "thememanager.h"
#include "settings/settingspage_main.h"
#include "settings/settingspage_profile.h"
#include "settings/settingspage_network.h"
#include "settings/settingspage_demo.h"
#include "settings/settingspage_privacy.h"
#include "settings/settingspage_security.h"
#include "settings/settingspage_interface.h"
#include "settings/settingspage_devices.h"
#include "settings/settingspage_updates.h"
#include "settings/settingspage_debug.h"
#include "../core/updatechecker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QCoreApplication>
#include <QApplication>
#include <QKeyEvent>

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

    // ── Страницы ──────────────────────────────────────────────────────────
    m_mainPage      = new SettingsMainPage(this);
    m_profilePage   = new SettingsProfilePage();
    m_networkPage   = new SettingsNetworkPage();
    m_demoPage      = new SettingsDemoPage();
    m_privacyPage   = new SettingsPrivacyPage();
    m_securityPage  = new SettingsSecurityPage();
    m_interfacePage = new SettingsInterfacePage();
    m_devicesPage   = new SettingsDevicesPage();
    m_updatesPage   = new SettingsUpdatesPage();
    m_debugPage     = new SettingsDebugPage();

    // ── Стек страниц ──────────────────────────────────────────────────────
    m_pageStack = new QStackedWidget(m_card);
    m_pageStack->addWidget(m_mainPage);      // 0
    m_pageStack->addWidget(m_profilePage);   // 1
    m_pageStack->addWidget(m_networkPage);   // 2
    m_pageStack->addWidget(m_demoPage);      // 3
    m_pageStack->addWidget(m_privacyPage);   // 4
    m_pageStack->addWidget(m_securityPage);  // 5
    m_pageStack->addWidget(m_interfacePage); // 6
    m_pageStack->addWidget(m_devicesPage);   // 7
    m_pageStack->addWidget(m_updatesPage);   // 8
    m_pageStack->addWidget(m_debugPage);     // 9

    // ── Сигналы страниц ───────────────────────────────────────────────────
    connect(m_profilePage,   &SettingsProfilePage::nameChanged,
            this,            &SettingsPanel::nameChanged);
    connect(m_profilePage,   &SettingsProfilePage::avatarChanged,
            this,            &SettingsPanel::avatarChanged);
    connect(m_profilePage,   &SettingsProfilePage::avatarChanged,
            this,            [this](const QString&) { m_mainPage->reload(); });
    connect(m_networkPage,   &SettingsNetworkPage::networkChanged,
            this,            &SettingsPanel::networkChanged);
    connect(m_interfacePage, &SettingsInterfacePage::enterSendsChanged,
            this,            &SettingsPanel::enterSendsChanged);
    connect(m_devicesPage,   &SettingsDevicesPage::connectToDeviceRequested,
            this,            &SettingsPanel::connectToDeviceRequested);
    connect(m_debugPage,     &SettingsDebugPage::verboseLoggingChanged,
            this,            &SettingsPanel::verboseLoggingChanged);

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
        if (m_closing) { hide(); m_closing = false; m_trailPixmap = QPixmap(); }
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

void SettingsPanel::setExternalAddress(const QString& ip, quint16 port) {
    m_profilePage->setExternalAddress(ip, port);
}

void SettingsPanel::reload() {
    m_mainPage->reload();
    m_profilePage->reload();
    m_networkPage->reload();
    m_securityPage->reload();
    m_privacyPage->reload();
    m_interfacePage->reload();
    m_updatesPage->reload();
}

void SettingsPanel::onSave() {
    const int idx = m_pageStack->currentIndex();
    bool ok = true;
    if      (idx == 1) ok = m_profilePage->save();
    else if (idx == 2) ok = m_networkPage->save();
    else if (idx == 4) ok = m_privacyPage->save();
    else if (idx == 6) ok = m_interfacePage->save();
    if (ok) {
        m_mainPage->reload();
        showMainPage();
    }
}

bool SettingsPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible()) {
        setGeometry(parentWidget()->rect());
        updateCardGeometry();
        update();
    }
    if (event->type() == QEvent::KeyPress && isVisible() && !m_closing
        && !QApplication::activeModalWidget())
    {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) {
            if (m_pageStack->currentIndex() != 0)
                showMainPage();
            else
                closePanel();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ── Overlay: openPanel / closePanel / paint / resize / mouse ─────────────

void SettingsPanel::openPanel() {
    QWidget* p = parentWidget();
    if (!p) return;

    if (isVisible() && m_closing) {
        m_closing = false;
        qApp->installEventFilter(this);
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

    if (isVisible()) return;

    reload();
    showMainPage();

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

    const int targetY = m_card->y();
    m_card->move(m_card->x(), -m_card->height());

    m_closing = false;
    m_overlayOpacity = 0.0;
    show();
    raise();
    update();
    qApp->installEventFilter(this);

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
    qApp->removeEventFilter(this);
    m_closing = true;

    // Grab blurred snapshot for motion trail (before card starts moving)
    const QPixmap raw = m_card->grab();
    const int f = 4;
    m_trailPixmap = raw.scaled(qMax(1, raw.width()/f), qMax(1, raw.height()/f),
                                Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                       .scaled(raw.width(), raw.height(),
                                Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

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

    // Motion trail: N ghost copies below the card (card slides UP on close)
    if (m_closing && !m_trailPixmap.isNull() && m_card->isVisible()) {
        constexpr int STEPS   = 5;
        constexpr int SPACING = 20;
        const QPoint cpos = m_card->pos();
        for (int i = STEPS; i >= 1; i--) {
            // i=STEPS → farthest (most transparent), i=1 → closest (most opaque)
            const qreal frac = qreal(STEPS - i + 1) / STEPS;
            p.setOpacity(m_overlayOpacity * frac * 0.45);
            p.drawPixmap(cpos.x(), cpos.y() + i * SPACING, m_trailPixmap);
        }
    }
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
