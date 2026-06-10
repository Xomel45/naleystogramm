#include "sidedrawer.h"
#include "thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QApplication>
#include <QFile>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>

// ── DrawerGhost ───────────────────────────────────────────────────────────────
// Sibling overlay drawn behind the SideDrawer during its close animation.
// Holds a blurred snapshot of the drawer at its original (open) position and
// fades it out, producing a motion-trail "smear" as the drawer slides away.
class DrawerGhost : public QWidget {
public:
    DrawerGhost(QWidget* parent, const QPixmap& px, int durationMs)
        : QWidget(parent), m_px(px)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        resize(parent->size());
        show();

        const int interval = 16;
        const int steps    = qMax(1, durationMs / interval);
        m_step = 1.0 / steps;

        m_timer = new QTimer(this);
        m_timer->setInterval(interval);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            m_alpha -= m_step;
            if (m_alpha <= 0.0) { close(); return; }
            update();
        });
        m_timer->start();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (m_px.isNull() || m_alpha <= 0.0) return;
        QPainter p(this);
        // 4 ghost copies with decreasing opacity away from origin (rightward blur smear)
        constexpr int STEPS   = 4;
        constexpr int SPACING = 14;
        for (int i = STEPS; i >= 1; i--) {
            const qreal frac = qreal(STEPS - i + 1) / STEPS;
            p.setOpacity(m_alpha * frac * 0.5);
            // Copies sit at x = (i-1)*SPACING, so x=0 is most opaque (where drawer was)
            p.drawPixmap((i - 1) * SPACING, 0, m_px);
        }
    }

private:
    QPixmap m_px;
    qreal   m_alpha {1.0};
    qreal   m_step  {0.05};
    QTimer* m_timer {nullptr};
};

#ifndef APP_VERSION
#define APP_VERSION "0.8.1.1"
#endif
#ifndef APP_CODENAME
#define APP_CODENAME "Хномык"
#endif

SideDrawer::SideDrawer(QWidget* parent) : QWidget(parent) {
    setObjectName("sideDrawer");
    hide();

    m_anim = new QPropertyAnimation(this, "pos", this);
    m_anim->setDuration(220);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QPropertyAnimation::finished, this, [this]() {
        if (!m_open) { hide(); emit closed(); }
    });

    buildUi();
    applyTheme();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](Theme) { applyTheme(); });
}

QPushButton* SideDrawer::makeRow(const QString& iconPath, const QString& text) {
    auto* btn = new QPushButton(text);
    btn->setObjectName("drawerRow");
    btn->setIcon(ThemeManager::tintedIcon(iconPath));
    btn->setIconSize(QSize(22, 22));
    btn->setFlat(true);
    btn->setFixedHeight(48);
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}

void SideDrawer::buildUi() {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Профиль (шапка) ─────────────────────────────────────────────────
    auto* profileSection = new QWidget();
    profileSection->setObjectName("drawerProfile");
    auto* pLayout = new QVBoxLayout(profileSection);
    pLayout->setContentsMargins(20, 22, 20, 16);
    pLayout->setSpacing(8);

    m_avatarLabel = new QLabel();
    m_avatarLabel->setFixedSize(62, 62);
    m_avatarLabel->setObjectName("drawerAvatar");
    m_avatarLabel->setAlignment(Qt::AlignCenter);

    m_nameLabel = new QLabel();
    m_nameLabel->setObjectName("drawerName");

    auto* versionLabel = new QLabel(
        QString("Naleystogramm %1 «%2»").arg(APP_VERSION).arg(APP_CODENAME));
    versionLabel->setObjectName("drawerVersion");

    pLayout->addWidget(m_avatarLabel);
    pLayout->addWidget(m_nameLabel);
    pLayout->addWidget(versionLabel);

    // ── Разделитель ──────────────────────────────────────────────────────
    auto mkSep = []() {
        auto* f = new QFrame();
        f->setFrameShape(QFrame::HLine);
        f->setObjectName("settingsSeparator");
        return f;
    };

    // ── Пункты меню ──────────────────────────────────────────────────────
    auto* editNameBtn   = makeRow(":/icons/ctx_edit.png",          tr("Изменить имя"));
    auto* myIdBtn       = makeRow(":/icons/nav_profile.png",       tr("Мой ID"));
    auto* addContactBtn = makeRow(":/icons/profile_add_member.png",tr("Добавить контакт"));
    auto* settingsBtn   = makeRow(":/icons/settings_btn.png",      tr("Настройки"));

    connect(editNameBtn,   &QPushButton::clicked, this, [this]{ closeDrawer(); emit editNameRequested();   });
    connect(myIdBtn,       &QPushButton::clicked, this, [this]{ closeDrawer(); emit showIdRequested();     });
    connect(addContactBtn, &QPushButton::clicked, this, [this]{ closeDrawer(); emit addContactRequested(); });
    connect(settingsBtn,   &QPushButton::clicked, this, [this]{ closeDrawer(); emit settingsRequested();   });

    // ── Сборка ───────────────────────────────────────────────────────────
    vl->addWidget(profileSection);
    vl->addWidget(mkSep());
    vl->addWidget(editNameBtn);
    vl->addWidget(myIdBtn);
    vl->addWidget(mkSep());
    vl->addWidget(addContactBtn);
    vl->addWidget(mkSep());
    vl->addWidget(settingsBtn);
    vl->addStretch(1);

    // ── Нижний колонтитул — отдельный виджет, позиционируется абсолютно ─
    m_footer = new QWidget(this);   // явный parent = this → двигается с дровером
    m_footer->setObjectName("drawerFooter");
    auto* fl = new QVBoxLayout(m_footer);
    fl->setContentsMargins(0, 0, 0, 0);
    fl->setSpacing(0);

    auto* footerSep = new QFrame();
    footerSep->setFrameShape(QFrame::HLine);
    footerSep->setObjectName("settingsSeparator");
    fl->addWidget(footerSep);

    auto* footerBody = new QWidget();
    footerBody->setObjectName("drawerFooterBody");
    auto* fbl = new QVBoxLayout(footerBody);
    fbl->setContentsMargins(12, 8, 12, 14);
    fbl->setSpacing(2);

    auto* appNameLbl = new QLabel("Naleystogramm");
    appNameLbl->setObjectName("drawerFooterApp");
    appNameLbl->setAlignment(Qt::AlignCenter);

    auto* linkRow = new QWidget();
    auto* lrl = new QHBoxLayout(linkRow);
    lrl->setContentsMargins(0, 0, 0, 0);
    lrl->setSpacing(0);

    auto* versionBtn = new QPushButton(QString("Версия %1").arg(APP_VERSION));
    versionBtn->setObjectName("drawerFooterLink");
    versionBtn->setFlat(true);
    versionBtn->setCursor(Qt::PointingHandCursor);
    connect(versionBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(
            QString("https://github.com/Xomel45/naleystogramm/releases/tag/%1").arg(APP_VERSION)));
    });

    auto* sepLbl = new QLabel(" — ");
    sepLbl->setObjectName("drawerFooterSep");
    sepLbl->setAlignment(Qt::AlignCenter);

    auto* aboutBtn = new QPushButton(tr("О программе"));
    aboutBtn->setObjectName("drawerFooterLink");
    aboutBtn->setFlat(true);
    aboutBtn->setCursor(Qt::PointingHandCursor);
    connect(aboutBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/Xomel45/naleystogramm/blob/main/README.md"));
    });

    lrl->addStretch();
    lrl->addWidget(versionBtn);
    lrl->addWidget(sepLbl);
    lrl->addWidget(aboutBtn);
    lrl->addStretch();

    fbl->addWidget(appNameLbl);
    fbl->addWidget(linkRow);
    fl->addWidget(footerBody);
}

void SideDrawer::open(const QString& name, const QPixmap& avatar) {
    m_nameLabel->setText(name);

    // Круглый аватар
    if (!avatar.isNull()) {
        const int sz = 62;
        QPixmap round(sz, sz);
        round.fill(Qt::transparent);
        QPainter pr(&round);
        pr.setRenderHint(QPainter::Antialiasing);
        QPainterPath pp;
        pp.addEllipse(0.0, 0.0, sz, sz);
        pr.setClipPath(pp);
        pr.drawPixmap(0, 0, avatar.scaled(sz, sz,
            Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        m_avatarLabel->setPixmap(round);
    }

    QWidget* p = parentWidget();
    const int w = p->width();
    const int h = p->height();
    setGeometry(-w, 0, w, h);
    show();
    raise();
    m_open = true;

    m_anim->stop();
    m_anim->setStartValue(QPoint(-w, 0));
    m_anim->setEndValue(QPoint(0, 0));
    m_anim->start();

    qApp->installEventFilter(this);
}

void SideDrawer::closeDrawer() {
    if (!m_open) return;
    m_open = false;
    qApp->removeEventFilter(this);

    // Spawn motion-trail ghost before animating away
    if (parentWidget()) {
        const QPixmap raw = grab();
        const int f = 4;
        const QPixmap blurred =
            raw.scaled(qMax(1, raw.width()/f), qMax(1, raw.height()/f),
                       Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
               .scaled(raw.width(), raw.height(),
                       Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        auto* ghost = new DrawerGhost(parentWidget(), blurred, m_anim->duration());
        ghost->resize(parentWidget()->size());
        ghost->show();
        raise();  // keep drawer on top of its ghost
    }

    m_anim->stop();
    m_anim->setStartValue(QPoint(0, 0));
    m_anim->setEndValue(QPoint(-width(), 0));
    m_anim->start();
}

void SideDrawer::closeInstant() {
    if (!m_open) return;
    m_open = false;
    m_anim->stop();
    qApp->removeEventFilter(this);
    hide();
}

void SideDrawer::positionFooter() {
    if (!m_footer) return;
    const int fh = m_footer->sizeHint().height();
    m_footer->setGeometry(0, height() - fh, width(), fh);
    m_footer->raise();
}

void SideDrawer::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);
    positionFooter();
}

bool SideDrawer::eventFilter(QObject* obj, QEvent* ev) {
    if (!m_open) return false;
    if (ev->type() == QEvent::MouseButtonPress) {
        const auto* me = static_cast<QMouseEvent*>(ev);
        const QPoint gp = me->globalPosition().toPoint();
        const QRect myGlobal(mapToGlobal(QPoint(0, 0)), size());
        if (!myGlobal.contains(gp))
            closeDrawer();
    } else if (obj == parentWidget() && ev->type() == QEvent::Resize) {
        // Родитель поменял размер — подгоняем ширину и высоту дровера
        QWidget* p = parentWidget();
        resize(p->width(), p->height()); // resizeEvent → positionFooter
    }
    return false;
}

void SideDrawer::applyTheme() {
    const auto& p = ThemeManager::instance().palette();
    setStyleSheet(QString(R"(
        QWidget#sideDrawer {
            background: %1;
            border-right: 1px solid %2;
        }
        QWidget#drawerProfile {
            background: %3;
        }
        QLabel#drawerName {
            color: %4;
            font-size: 15px;
            font-weight: 700;
        }
        QLabel#drawerVersion {
            color: %5;
            font-size: 11px;
        }
        QPushButton#drawerRow {
            background: transparent;
            color: %4;
            font-size: 14px;
            text-align: left;
            padding-left: 18px;
            border: none;
            border-radius: 0;
        }
        QPushButton#drawerRow:hover {
            background: %6;
        }
        QWidget#drawerFooter {
            background: transparent;
        }
        QLabel#drawerFooterApp {
            color: %4;
            font-size: 12px;
            font-weight: 600;
        }
        QLabel#drawerFooterSep {
            color: %5;
            font-size: 11px;
        }
        QPushButton#drawerFooterLink {
            background: transparent;
            color: %5;
            font-size: 11px;
            border: none;
            padding: 0 2px;
        }
        QPushButton#drawerFooterLink:hover {
            color: %4;
            text-decoration: underline;
        }
    )").arg(p.bgSurface, p.border, p.bgElevated,
            p.textPrimary, p.textMuted, p.bgElevated));
}
