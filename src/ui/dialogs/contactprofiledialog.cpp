#include "contactprofiledialog.h"
#include "../../core/network.h"
#include "../../core/storage.h"
#include "../../core/updatechecker.h"
#include "../../core/versionutils.h"
#include "../thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QFrame>
#include <QScrollArea>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QFile>
#include <QDateTime>
#include <QFont>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QScreen>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QCoreApplication>
#include <QEventLoop>
#include <QResizeEvent>
#include <QDate>
#include <QLocale>

// Возвращает отступ сверху от родителя (низ шапки чата)
static int chatHeaderBottom(QWidget* parent) {
    if (!parent) return 0;
    if (auto* h = parent->findChild<QWidget*>("chatHeader"))
        return h->mapTo(parent, QPoint(0, h->height())).y();
    return 0;
}

// ── Конструктор ──────────────────────────────────────────────────────────

ContactProfileDialog::ContactProfileDialog(const QUuid& peerUuid,
                                           NetworkManager* network,
                                           StorageManager* storage,
                                           QWidget* parent)
    : QWidget(parent)
    , m_uuid(peerUuid)
    , m_network(network)
    , m_storage(storage)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    setupUi();
    applyTheme();
    populateData();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](Theme) { applyTheme(); });

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(5000);
    connect(m_refreshTimer, &QTimer::timeout, this, &ContactProfileDialog::refreshData);
    m_refreshTimer->start();

    // Анимация карточки (вылет из-под шапки)
    m_anim = new QPropertyAnimation(m_card, "pos", this);
    m_anim->setDuration(280);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QPropertyAnimation::finished, this, [this]() {
        if (m_closing) {
            m_closing = false;
            m_trailPixmap = QPixmap();
            close();
        } else {
            m_card->clearMask();
        }
    });
    // Маска клипует карточку по нижней границе шапки во время анимации
    connect(m_anim, &QPropertyAnimation::valueChanged, this, [this](const QVariant& val) {
        if (!m_card || !parentWidget()) return;
        const int cardY   = val.value<QPoint>().y();
        const int yOffset = chatHeaderBottom(parentWidget());
        const int showFrom = qMax(0, yOffset - cardY);
        const int vh = m_card->height() - showFrom;
        if (vh <= 0) {
            if (m_card->isVisible()) m_card->hide();
        } else if (showFrom == 0) {
            if (!m_card->isVisible()) m_card->show();
            m_card->clearMask();
        } else {
            if (!m_card->isVisible()) m_card->show();
            m_card->setMask(QRegion(0, showFrom, m_card->width(), vh));
        }
    });

    // Анимация фона (fade in/out)
    m_fadeAnim = new QPropertyAnimation(this, "overlayOpacity", this);
    m_fadeAnim->setDuration(380);
    m_fadeAnim->setEasingCurve(QEasingCurve::OutQuad);

    if (parent)
        parent->installEventFilter(this);
}

// ── Оверлей: openPanel / closePanel / paint / resize / mouse ─────────────

void ContactProfileDialog::openPanel() {
    QWidget* p = parentWidget();
    if (!p) return;

    const int yOffset = chatHeaderBottom(p);

    // Разворачиваем анимацию закрытия
    if (isVisible() && m_closing) {
        m_closing = false;
        m_anim->stop();
        const QPoint fromPos = m_card->pos();
        if (!m_card->isVisible()) m_card->show();
        updateCardGeometry();
        const QPoint toPos = m_card->pos();
        m_card->move(fromPos);
        m_anim->setStartValue(fromPos);
        m_anim->setEndValue(toPos);
        m_anim->start();
        m_fadeAnim->stop();
        m_fadeAnim->setStartValue(m_overlayOpacity);
        m_fadeAnim->setEndValue(1.0);
        m_fadeAnim->start();
        return;
    }

    if (isVisible()) return;

    // Захватываем фон всего родителя
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

    // Оверлей = весь родитель; блюр покрывает Layer 1 и Layer 3
    setGeometry(p->rect());
    updateCardGeometry();

    // Карточка стартует за нижним краем шапки (полностью скрыта маской),
    // выезжает вниз — эффект вылета из-под шапки
    const int targetY = m_card->y();
    m_card->move(m_card->x(), yOffset - m_card->height());
    m_card->hide();  // Скрываем до первого кадра анимации

    m_closing = false;
    m_overlayOpacity = 0.0;
    show();
    raise();
    update();

    // Карточка начинает движение сразу
    m_anim->stop();
    m_anim->setStartValue(m_card->pos());
    m_anim->setEndValue(QPoint(m_card->x(), targetY));
    m_anim->start();

    // Блюр и затемнение — с задержкой 70мс (сначала карточка, потом фон)
    QTimer::singleShot(70, this, [this]() {
        if (!m_closing) {
            m_fadeAnim->stop();
            m_fadeAnim->setStartValue(0.0);
            m_fadeAnim->setEndValue(1.0);
            m_fadeAnim->start();
        }
    });
}

void ContactProfileDialog::closePanel() {
    if (!isVisible() || m_closing) return;
    m_closing = true;

    // Grab blurred snapshot for motion trail (before card starts moving)
    const QPixmap raw = m_card->grab();
    const int f = 4;
    m_trailPixmap = raw.scaled(qMax(1, raw.width()/f), qMax(1, raw.height()/f),
                                Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                       .scaled(raw.width(), raw.height(),
                                Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    const int yOffset = chatHeaderBottom(parentWidget());
    m_anim->stop();
    m_anim->setStartValue(m_card->pos());
    // Карточка уходит под нижний край шапки — "залетает" в Layer 3
    m_anim->setEndValue(QPoint(m_card->x(), yOffset - m_card->height()));
    m_anim->start();

    m_fadeAnim->stop();
    m_fadeAnim->setStartValue(m_overlayOpacity);
    m_fadeAnim->setEndValue(0.0);
    m_fadeAnim->start();
}

void ContactProfileDialog::updateCardGeometry() {
    if (!m_card) return;
    const int yOffset = chatHeaderBottom(parentWidget());
    const int cardW = 400;
    const int availH = height() - yOffset;
    const int cardH = qMin(m_card->sizeHint().height(), availH - 80);
    const int x = (width()  - cardW) / 2;
    const int y = yOffset + (availH - cardH) / 2;
    m_card->setGeometry(x, y, cardW, cardH);
}

void ContactProfileDialog::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setOpacity(m_overlayOpacity);
    if (!m_blurredBg.isNull())
        p.drawPixmap(rect(), m_blurredBg);
    else
        p.fillRect(rect(), QColor(0, 0, 0, 200));
    p.fillRect(rect(), QColor(0, 0, 0, 130));

    // Motion trail: ghost copies below the card (card slides UP on close)
    if (m_closing && !m_trailPixmap.isNull() && m_card->isVisible()) {
        constexpr int STEPS   = 5;
        constexpr int SPACING = 20;
        const QPoint cpos = m_card->pos();
        for (int i = STEPS; i >= 1; i--) {
            const qreal frac = qreal(STEPS - i + 1) / STEPS;
            p.setOpacity(m_overlayOpacity * frac * 0.45);
            p.drawPixmap(cpos.x(), cpos.y() + i * SPACING, m_trailPixmap);
        }
    }
}

void ContactProfileDialog::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);
    if (m_card) updateCardGeometry();
}

void ContactProfileDialog::mousePressEvent(QMouseEvent* ev) {
    if (m_card && !m_card->geometry().contains(ev->pos()))
        closePanel();
    else
        QWidget::mousePressEvent(ev);
}

// ── Построение UI ────────────────────────────────────────────────────────

void ContactProfileDialog::setupUi() {
    // Карточка — абсолютно позиционируется в openPanel/updateCardGeometry
    m_card = new QWidget(this);
    m_card->setObjectName("profileCard");

    auto* cardLay = new QVBoxLayout(m_card);
    cardLay->setContentsMargins(0, 0, 0, 0);
    cardLay->setSpacing(0);

    // ── Шапка: аватар + имя + статус ─────────────────────────────────────
    auto* header = new QWidget();
    header->setObjectName("profileHeader");

    // Кнопка закрытия абсолютно поверх шапки
    auto* closeBtn = new QPushButton("✕", header);
    closeBtn->setObjectName("profileCloseBtn");
    closeBtn->setFixedSize(30, 30);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &ContactProfileDialog::closePanel);
    QTimer::singleShot(0, this, [this, closeBtn]() {
        closeBtn->move(m_card->width() - 30 - 12, 12);
    });

    auto* headerLay = new QVBoxLayout(header);
    headerLay->setContentsMargins(20, 24, 20, 16);
    headerLay->setSpacing(6);
    headerLay->setAlignment(Qt::AlignHCenter);

    m_avatarLabel = new QLabel();
    m_avatarLabel->setObjectName("profileAvatar");
    m_avatarLabel->setFixedSize(80, 80);
    m_avatarLabel->setAlignment(Qt::AlignCenter);

    m_nameLabel = new QLabel();
    m_nameLabel->setObjectName("profileName");
    m_nameLabel->setAlignment(Qt::AlignHCenter);

    m_statusLabel = new QLabel();
    m_statusLabel->setObjectName("profileStatus");
    m_statusLabel->setAlignment(Qt::AlignHCenter);

    headerLay->addWidget(m_avatarLabel, 0, Qt::AlignHCenter);
    headerLay->addWidget(m_nameLabel);
    headerLay->addWidget(m_statusLabel);

    // ── Кнопки действий ───────────────────────────────────────────────────
    auto* actionsBar = new QWidget();
    actionsBar->setObjectName("profileActionsBar");
    auto* actionsLay = new QHBoxLayout(actionsBar);
    actionsLay->setContentsMargins(12, 10, 12, 14);
    actionsLay->setSpacing(8);

    auto makeActionBtn = [&](const QString& iconPath,
                              const QString& text) -> QToolButton* {
        auto* btn = new QToolButton();
        btn->setObjectName("profileActionBtn");
        btn->setIcon(ThemeManager::tintedIcon(iconPath));
        btn->setIconSize(QSize(22, 22));
        btn->setText(text);
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setFixedHeight(58);
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    auto* writeBtn = makeActionBtn(":/icons/dialogs_chat.png", tr("Написать"));
    connect(writeBtn, &QToolButton::clicked, this, &ContactProfileDialog::closePanel);

    m_callBtn = makeActionBtn(":/icons/nav_call.png", tr("Позвонить"));
    m_callBtn->setEnabled(false);
    connect(m_callBtn, &QToolButton::clicked, this, [this]() {
        emit callRequested(m_uuid);
        closePanel();
    });

    m_shellBtn = makeActionBtn(":/icons/settings_advanced.png", tr("Шелл"));
    m_shellBtn->setEnabled(false);
    connect(m_shellBtn, &QToolButton::clicked, this, [this]() {
        emit shellRequested(m_uuid);
        closePanel();
    });

    actionsLay->addWidget(writeBtn);
    actionsLay->addWidget(m_callBtn);
    actionsLay->addWidget(m_shellBtn);

    // ── Прокручиваемая область ────────────────────────────────────────────
    auto* scroll = new QScrollArea();
    scroll->setObjectName("profileScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMaximumHeight(380);

    auto* scrollContent = new QWidget();
    scrollContent->setObjectName("profileScrollContent");
    auto* scrollLay = new QVBoxLayout(scrollContent);
    scrollLay->setContentsMargins(0, 0, 0, 0);
    scrollLay->setSpacing(0);

    // Несовместимость версий
    m_compatWarning = new QLabel();
    m_compatWarning->setObjectName("profileCompatWarning");
    m_compatWarning->setWordWrap(true);
    m_compatWarning->setContentsMargins(16, 10, 16, 10);
    m_compatWarning->hide();
    scrollLay->addWidget(m_compatWarning);

    // ── Helpers ───────────────────────────────────────────────────────────
    auto mkSep = [&]() -> QFrame* {
        auto* f = new QFrame();
        f->setFrameShape(QFrame::HLine);
        f->setObjectName("profileSeparator");
        return f;
    };
    auto mkInsetSep = [&]() -> QFrame* {
        auto* f = new QFrame();
        f->setFrameShape(QFrame::HLine);
        f->setObjectName("profileInsetSeparator");
        return f;
    };

    // Строка: иконка | значение (жирным) / метка (серым)
    auto makeRow = [&](const QString& iconPath, const QString& labelText,
                        QLabel*& outValue, bool clickCopy = false) -> QWidget* {
        auto* row = new QWidget();
        row->setObjectName(clickCopy ? "profileInfoRowCopy" : "profileInfoRow");
        row->setFixedHeight(52);
        if (clickCopy) row->setCursor(Qt::PointingHandCursor);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(20, 0, 20, 0);
        rl->setSpacing(14);

        auto* ico = new QLabel();
        ico->setPixmap(ThemeManager::tintedIcon(iconPath).pixmap(20, 20));
        ico->setFixedSize(24, 24);
        ico->setAlignment(Qt::AlignCenter);
        ico->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto* textCol = new QVBoxLayout();
        textCol->setSpacing(1);

        outValue = new QLabel("—");
        outValue->setObjectName("profileInfoValue");
        outValue->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto* lbl = new QLabel(labelText);
        lbl->setObjectName("profileInfoLabel");
        lbl->setAttribute(Qt::WA_TransparentForMouseEvents);

        textCol->addWidget(outValue);
        textCol->addWidget(lbl);

        rl->addWidget(ico);
        rl->addLayout(textCol, 1);
        return row;
    };

    // ── ID ────────────────────────────────────────────────────────────────
    scrollLay->addWidget(mkSep());
    auto* idRow = makeRow(":/icons/nav_profile.png", tr("Идентификатор"), m_idRow, true);
    idRow->installEventFilter(this);
    scrollLay->addWidget(idRow);
    scrollLay->addWidget(mkInsetSep());
    scrollLay->addWidget(makeRow(":/icons/settings_profile.png", tr("День рождения"), m_birthdayRow));

    // ── Устройство ────────────────────────────────────────────────────────
    scrollLay->addWidget(mkSep());
    scrollLay->addWidget(makeRow(":/icons/settings_advanced.png", tr("Тип устройства"),       m_deviceType));
    scrollLay->addWidget(mkInsetSep());
    scrollLay->addWidget(makeRow(":/icons/settings_advanced.png", tr("Операционная система"), m_osRow));
    scrollLay->addWidget(mkInsetSep());
    scrollLay->addWidget(makeRow(":/icons/settings_advanced.png", tr("Процессор"),            m_cpuRow));
    scrollLay->addWidget(mkInsetSep());
    scrollLay->addWidget(makeRow(":/icons/settings_advanced.png", tr("Оперативная память"),   m_ramRow));

    // ── Соединение ────────────────────────────────────────────────────────
    scrollLay->addWidget(mkSep());
    scrollLay->addWidget(makeRow(":/icons/settings_network.png",    tr("IP-адрес"),   m_ipRow));
    scrollLay->addWidget(mkInsetSep());
    scrollLay->addWidget(makeRow(":/icons/input_link_settings.png", tr("Порт"),       m_portRow));
    scrollLay->addWidget(mkInsetSep());
    scrollLay->addWidget(makeRow(":/icons/settings_calls.png",      tr("Пинг"),       m_pingRow));
    scrollLay->addWidget(mkInsetSep());
    scrollLay->addWidget(makeRow(":/icons/contacts_online.png",     tr("Подключён"),  m_uptimeRow));

    // ── Безопасность ──────────────────────────────────────────────────────
    scrollLay->addWidget(mkSep());
    {
        auto* safetyWidget = new QWidget();
        safetyWidget->setObjectName("profileSafetySection");
        auto* sl = new QVBoxLayout(safetyWidget);
        sl->setContentsMargins(20, 12, 20, 14);
        sl->setSpacing(6);

        auto* safetyTitle = new QLabel(tr("Ключ безопасности E2E"));
        safetyTitle->setObjectName("profileInfoLabel");

        m_safetyLabel = new QLabel("—");
        m_safetyLabel->setObjectName("profileSafetyNumber");
        m_safetyLabel->setAlignment(Qt::AlignCenter);
        m_safetyLabel->setFont(QFont("Monospace", 11));
        m_safetyLabel->setWordWrap(true);

        m_safetyHint = new QLabel(
            tr("Сверьте с собеседником — если совпадает, связь защищена"));
        m_safetyHint->setObjectName("profileInfoLabel");
        m_safetyHint->setWordWrap(true);
        m_safetyHint->setAlignment(Qt::AlignCenter);

        sl->addWidget(safetyTitle, 0, Qt::AlignHCenter);
        sl->addWidget(m_safetyLabel);
        sl->addWidget(m_safetyHint);
        scrollLay->addWidget(safetyWidget);
    }

    // ── Заблокировать ─────────────────────────────────────────────────────
    scrollLay->addWidget(mkSep());
    auto* blockRow = new QPushButton();
    blockRow->setObjectName("profileBlockRow");
    blockRow->setFlat(true);
    blockRow->setFixedHeight(52);
    blockRow->setCursor(Qt::PointingHandCursor);
    {
        auto* rl = new QHBoxLayout(blockRow);
        rl->setContentsMargins(20, 0, 20, 0);
        rl->setSpacing(14);
        auto* ico = new QLabel();
        ico->setPixmap(ThemeManager::tintedIcon(":/icons/ctx_block.png").pixmap(20, 20));
        ico->setFixedSize(24, 24);
        ico->setAlignment(Qt::AlignCenter);
        ico->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* lbl = new QLabel(tr("Заблокировать"));
        lbl->setObjectName("profileBlockLabel");
        lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        rl->addWidget(ico);
        rl->addWidget(lbl);
        rl->addStretch();
    }
    connect(blockRow, &QPushButton::clicked, this, [this]() {
        emit blockRequested(m_uuid);
        closePanel();
    });
    scrollLay->addWidget(blockRow);
    scrollLay->addStretch();

    scroll->setWidget(scrollContent);

    // ── Сборка карточки ───────────────────────────────────────────────────
    cardLay->addWidget(header);
    cardLay->addWidget(actionsBar);
    cardLay->addWidget(scroll);
}

// ── Тема ─────────────────────────────────────────────────────────────────

void ContactProfileDialog::applyTheme() {
    if (!m_card) return;
    const auto& p = ThemeManager::instance().palette();
    m_card->setStyleSheet(QString(R"(
        QWidget#profileCard {
            background: %1;
            border-radius: 12px;
            border: 1px solid %2;
        }
        QWidget#profileHeader {
            background: %3;
            border-radius: 12px 12px 0 0;
        }
        QLabel#profileName {
            color: %4;
            font-size: 17px;
            font-weight: 700;
        }
        QLabel#profileStatus {
            color: %5;
            font-size: 13px;
        }
        QLabel#profileAvatar {
            background: %6;
            border-radius: 40px;
            color: %4;
            font-size: 26px;
            font-weight: 700;
        }
        QPushButton#profileCloseBtn {
            background: %7;
            color: %5;
            border: none;
            border-radius: 15px;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#profileCloseBtn:hover {
            background: %2;
            color: %4;
        }
        QWidget#profileActionsBar {
            background: %3;
        }
        QToolButton#profileActionBtn {
            background: %8;
            color: %4;
            font-size: 12px;
            border: none;
            border-radius: 8px;
        }
        QToolButton#profileActionBtn:hover {
            background: %2;
        }
        QToolButton#profileActionBtn:disabled {
            color: %5;
        }
        QWidget#profileScrollContent {
            background: %1;
        }
        QScrollArea#profileScroll {
            background: %1;
        }
        QWidget#profileInfoRow {
            background: %1;
        }
        QWidget#profileInfoRowCopy {
            background: %1;
        }
        QWidget#profileInfoRowCopy:hover {
            background: %3;
        }
        QLabel#profileInfoValue {
            color: %4;
            font-size: 14px;
        }
        QLabel#profileInfoLabel {
            color: %5;
            font-size: 12px;
        }
        QFrame#profileSeparator {
            color: %2;
        }
        QFrame#profileInsetSeparator {
            color: %2;
            margin-left: 58px;
        }
        QWidget#profileSafetySection {
            background: %1;
        }
        QLabel#profileSafetyNumber {
            color: %4;
            font-size: 13px;
            letter-spacing: 2px;
        }
        QLabel#profileCompatWarning {
            color: %9;
            background: transparent;
            font-size: 12px;
        }
        QPushButton#profileBlockRow {
            background: %1;
            border: none;
            text-align: left;
        }
        QPushButton#profileBlockRow:hover {
            background: %3;
        }
        QLabel#profileBlockLabel {
            color: %9;
            font-size: 14px;
        }
    )").arg(p.bgSurface, p.border, p.bgElevated, p.textPrimary,
            p.textMuted, p.accent, p.bgElevated, p.bgElevated, p.danger));
}

// ── Заполнение данными ────────────────────────────────────────────────────

void ContactProfileDialog::populateData() {
    const Contact c = m_storage->getContact(m_uuid);
    const PeerPublicInfo info = m_network->getPeerInfo(m_uuid);

    // ── Имя ──────────────────────────────────────────────────────────────
    const QString name = c.name.isEmpty() ? tr("Неизвестный") : c.name;
    m_nameLabel->setText(name);

    // ── ID ───────────────────────────────────────────────────────────────
    m_idRow->setText(m_uuid.toString(QUuid::WithoutBraces).left(8) + "…");
    m_idRow->setProperty("fullUuid", m_uuid.toString(QUuid::WithoutBraces));

    // ── День рождения ─────────────────────────────────────────────────────
    const QString bday = !info.birthday.isEmpty() ? info.birthday : c.birthday;
    if (!bday.isEmpty()) {
        const QDate d = QDate::fromString(bday, Qt::ISODate);
        m_birthdayRow->setText(d.isValid()
            ? QLocale(QLocale::Russian).toString(d, "d MMMM yyyy") + " г."
            : "—");
    } else {
        m_birthdayRow->setText("—");
    }

    // ── Аватар ───────────────────────────────────────────────────────────
    const int sz = 80;
    if (!c.avatarPath.isEmpty() && QFile::exists(c.avatarPath)) {
        QPixmap src(c.avatarPath);
        if (!src.isNull()) {
            QPixmap rounded(sz, sz);
            rounded.fill(Qt::transparent);
            QPainter p(&rounded);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath pp;
            pp.addEllipse(0, 0, sz, sz);
            p.setClipPath(pp);
            p.drawPixmap(0, 0, src.scaled(sz, sz, Qt::IgnoreAspectRatio,
                                          Qt::SmoothTransformation));
            m_avatarLabel->setPixmap(rounded);
        }
    } else {
        m_avatarLabel->setPixmap({});
        m_avatarLabel->setText(name.left(1).toUpper());
    }

    // ── Совместимость версий ──────────────────────────────────────────────
    const QString currentVer = QLatin1String(UpdateChecker::kCurrentVersion);
    const bool versionMismatch = VersionUtils::isNewerThan(c.versionCreated, currentVer);
    if (versionMismatch) {
        m_compatWarning->setText(
            tr("⚠ Контакт использует более новую версию (v%1). "
               "Обновите приложение для полной совместимости.")
            .arg(c.versionCreated));
        m_compatWarning->show();
    }

    // ── Системная информация ──────────────────────────────────────────────
    const QJsonObject si = !info.systemInfo.isEmpty()
        ? info.systemInfo
        : QJsonDocument::fromJson(c.systemInfoJson.toUtf8()).object();

    if (versionMismatch) {
        m_deviceType->setText("—");
        m_cpuRow->setText("—");
        m_ramRow->setText("—");
        m_osRow->setText("—");
    } else {
        m_deviceType->setText(si["deviceType"].toString("—"));
        m_cpuRow->setText(si["cpuModel"].toString("—"));
        m_ramRow->setText(si["ramAmount"].toString("—"));
        m_osRow->setText(si["osName"].toString("—"));
    }

    // ── Соединение ────────────────────────────────────────────────────────
    m_ipRow->setText(info.ip.isEmpty() ? (c.ip.isEmpty() ? "—" : c.ip) : info.ip);
    m_portRow->setText(info.serverPort > 0
        ? QString::number(info.serverPort)
        : (c.port > 0 ? QString::number(c.port) : "—"));

    refreshData();
}

// ── Event filter — копирование ID + resize родителя ──────────────────────

bool ContactProfileDialog::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == parentWidget() && ev->type() == QEvent::Resize && isVisible()) {
        setGeometry(parentWidget()->rect());
        updateCardGeometry();
        update();
    }
    if (ev->type() == QEvent::MouseButtonPress) {
        if (auto* w = qobject_cast<QWidget*>(obj)) {
            if (w->objectName() == QLatin1String("profileInfoRowCopy")) {
                const QString full = m_idRow->property("fullUuid").toString();
                if (!full.isEmpty()) {
                    QApplication::clipboard()->setText(full);
                    const QString prev = m_idRow->text();
                    m_idRow->setText(tr("Скопировано!"));
                    QTimer::singleShot(1500, m_idRow, [this, prev]() {
                        if (m_idRow) m_idRow->setText(prev);
                    });
                }
            }
        }
    }
    return QWidget::eventFilter(obj, ev);
}

// ── Горячее обновление ────────────────────────────────────────────────────

void ContactProfileDialog::refreshData() {
    const PeerPublicInfo info = m_network->getPeerInfo(m_uuid);
    const bool online = info.state == ConnectionState::Connected;

    m_statusLabel->setText(online ? tr("● В сети") : tr("○ Не в сети"));
    m_statusLabel->setObjectName(online ? "profileStatusOnline" : "profileStatus");

    if (m_callBtn)  m_callBtn->setEnabled(online);
    if (m_shellBtn) m_shellBtn->setEnabled(online);

    m_pingRow->setText(info.latencyMs >= 0
        ? QString("%1 мс").arg(info.latencyMs) : "—");

    m_uptimeRow->setText(info.connectedSince.isValid() && online
        ? formatUptime(info.connectedSince) : "—");
}

// ── Safety Number ─────────────────────────────────────────────────────────

void ContactProfileDialog::setSafetyNumber(const QString& safetyNum) {
    if (safetyNum.isEmpty()) {
        m_safetyLabel->setText("—");
        m_safetyHint->setText(tr("Сессия E2E ещё не установлена"));
    } else {
        m_safetyLabel->setText(safetyNum);
        m_safetyHint->setText(
            tr("Сверьте с собеседником — если совпадает, связь защищена"));
    }
}

// ── Форматирование времени ────────────────────────────────────────────────

QString ContactProfileDialog::formatUptime(const QDateTime& since) const {
    const qint64 secs = since.secsTo(QDateTime::currentDateTime());
    if (secs < 0) return "—";
    const int h = static_cast<int>(secs / 3600);
    const int m = static_cast<int>((secs % 3600) / 60);
    return h > 0 ? QString("%1ч %2м").arg(h).arg(m) : QString("%1м").arg(m);
}
