#include "settingspage_main.h"
#include "settingshelpers.h"
#include "../settingspanel.h"
#include "../thememanager.h"
#include "../../core/sessionmanager.h"
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QFile>

SettingsMainPage::SettingsMainPage(SettingsPanel* panel) : QWidget(panel) {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Профиль-кнопка ────────────────────────────────────────────────────────
    auto* profileBtn = new QPushButton();
    profileBtn->setObjectName("settingsMainProfile");
    profileBtn->setFlat(true);
    profileBtn->setFixedHeight(88);
    profileBtn->setCursor(Qt::PointingHandCursor);
    connect(profileBtn, &QPushButton::clicked, panel, [panel]() {
        panel->showSection(1, QObject::tr("Мой аккаунт"), true);
    });

    auto* pcl = new QHBoxLayout(profileBtn);
    pcl->setContentsMargins(16, 14, 16, 14);
    pcl->setSpacing(14);

    m_avatar = new QLabel();
    m_avatar->setObjectName("settingsMainAvatar");
    m_avatar->setFixedSize(52, 52);
    m_avatar->setAlignment(Qt::AlignCenter);
    m_avatar->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* nameCol = new QVBoxLayout();
    nameCol->setSpacing(3);
    nameCol->setContentsMargins(0, 0, 0, 0);

    m_name = new QLabel();
    m_name->setObjectName("settingsMainName");
    m_name->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_uuid = new QLabel();
    m_uuid->setObjectName("settingsMainUuid");
    m_uuid->setAttribute(Qt::WA_TransparentForMouseEvents);

    nameCol->addWidget(m_name);
    nameCol->addWidget(m_uuid);

    auto* chevron = new QLabel("›");
    chevron->setObjectName("settingsNavChevron");
    chevron->setAttribute(Qt::WA_TransparentForMouseEvents);

    pcl->addWidget(m_avatar);
    pcl->addLayout(nameCol, 1);
    pcl->addWidget(chevron);
    vl->addWidget(profileBtn);

    // ── Разделитель ───────────────────────────────────────────────────────────
    vl->addWidget(spSeparator());

    // ── Список разделов ───────────────────────────────────────────────────────
    auto* listScroll = new QScrollArea();
    listScroll->setFrameShape(QFrame::NoFrame);
    listScroll->setWidgetResizable(true);
    listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listScroll->setObjectName("settingsScroll");

    auto* sectionsWidget = new QWidget();
    auto* sl = new QVBoxLayout(sectionsWidget);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(0);

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

        connect(btn, &QPushButton::clicked, panel, [panel, pageIdx, title, hasSave]() {
            panel->showSection(pageIdx, title, hasSave);
        });
        return btn;
    };

    sl->addWidget(makeNavRow(":/icons/settings_network.png",    tr("Сеть"),                 2, tr("Сеть"),                 true));
    sl->addWidget(spSeparator());
    sl->addWidget(makeNavRow(":/icons/dialogs_lock_on.png",     tr("Демо-режим"),            3, tr("Демо-режим"),           false));
    sl->addWidget(spSeparator());
    sl->addWidget(makeNavRow(":/icons/settings_privacy.png",    tr("Конфиденциальность"),   4, tr("Конфиденциальность"),   true));
    sl->addWidget(spSeparator());
    sl->addWidget(makeNavRow(":/icons/settings_security.png",   tr("Безопасность"),          5, tr("Безопасность"),         false));
    sl->addWidget(spSeparator());
    sl->addWidget(makeNavRow(":/icons/settings_appearance.png", tr("Интерфейс"),            6, tr("Интерфейс"),            true));
    sl->addWidget(spSeparator());
    sl->addWidget(makeNavRow(":/icons/input_link_settings.png", tr("Связанные устройства"), 7, tr("Связанные устройства"), false));
    sl->addWidget(spSeparator());
    sl->addWidget(makeNavRow(":/icons/install_update.png",      tr("Обновления"),           8, tr("Обновления"),           false));
    sl->addWidget(spSeparator());
    sl->addWidget(makeNavRow(":/icons/settings_advanced.png",   tr("Отладка"),              9, tr("Отладка"),              false));
    sl->addWidget(spSeparator());
    sl->addWidget(makeNavRow(":/icons/nav_attach.png",          tr("Плагины"),             10, tr("Плагины"),             false));
    sl->addStretch();

    listScroll->setWidget(sectionsWidget);
    vl->addWidget(listScroll, 1);
}

void SettingsMainPage::reload() {
    auto& sm = SessionManager::instance();
    const QString name = QString::fromStdString(sm.displayName());
    m_name->setText(name.isEmpty() ? tr("(без имени)") : name);
    m_uuid->setText(QString::fromStdString(sm.uuid()).left(8) + "…");

    const QString avatarPath = QString::fromStdString(sm.avatarPath());
    if (!avatarPath.isEmpty() && QFile::exists(avatarPath)) {
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
        m_avatar->setPixmap(round);
        m_avatar->setText({});
    } else {
        m_avatar->setPixmap({});
        m_avatar->setText(name.isEmpty() ? "?" : name.left(1).toUpper());
    }
}
