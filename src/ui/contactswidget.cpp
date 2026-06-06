#include "contactswidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStyledItemDelegate>
#include <QMenu>
#include <QAction>
#include <QFile>
#include <QPixmap>
#include <QPixmapCache>
#include <QPainter>
#include <QPainterPath>
#include <QIcon>
#include <QLocale>
#include <QStyle>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QDebug>
#include "thememanager.h"

// ── ContactItemDelegate ────────────────────────────────────────────────────

class ContactItemDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return {0, 64};
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);

        const auto& pal = ThemeManager::instance().palette();
        const bool selected = opt.state & QStyle::State_Selected;
        const bool hovered  = opt.state & QStyle::State_MouseOver;
        const QRect r = opt.rect;

        // Фон при наведении / выделении
        if (selected || hovered) {
            p->setPen(Qt::NoPen);
            p->setBrush(selected
                ? QColor(pal.bgElevated).lighter(108)
                : QColor(pal.bgElevated));
            p->drawRoundedRect(r.adjusted(6, 2, -6, -2), 10, 10);
        }

        // Аватар (круглый)
        const int avSz = 46;
        const int padL = 12;
        const QRect avRect(r.left() + padL, r.top() + (r.height() - avSz) / 2, avSz, avSz);
        const QPixmap avPix = loadAvatar(idx.data(AvatarPathRole).toString(), avSz);
        if (!avPix.isNull()) {
            QPainterPath clip;
            clip.addEllipse(QRectF(avRect));
            p->setClipPath(clip);
            p->drawPixmap(avRect, avPix);
            p->setClipping(false);
        }

        // Точка статуса (правый нижний угол аватара)
        const int dotSz = 12;
        const QRect dotRect(
            avRect.right() - dotSz + 2,
            avRect.bottom() - dotSz + 2,
            dotSz, dotSz);
        p->setPen(QPen(QColor(pal.bg), 2.0));
        p->setBrush(statusColor(idx.data(StatusRole).toInt(), pal));
        p->drawEllipse(dotRect);

        // Координаты области контента
        const int cx = r.left() + padL + avSz + 10;
        const int cr = r.right() - 10;

        // Дата / время (правый верх)
        const QString dateStr = idx.data(MsgTimeRole).toString();
        int dateLeftX = cr;
        if (!dateStr.isEmpty()) {
            QFont df = opt.font;
            df.setPixelSize(12);
            const int dw = QFontMetrics(df).horizontalAdvance(dateStr);
            const QRect dateRect(cr - dw, r.top() + 14, dw, 16);
            p->setPen(QColor(pal.textMuted));
            p->setFont(df);
            p->drawText(dateRect, Qt::AlignRight | Qt::AlignVCenter, dateStr);
            dateLeftX = cr - dw - 6;
        }

        // Счётчик непрочитанных (правый низ)
        const int unread = idx.data(UnreadRole).toInt();
        const bool muted = idx.data(IsMutedRole).toBool();
        int badgeLeftX = cr;
        if (unread > 0) {
            const QString badgeText = unread > 99 ? "99+" : QString::number(unread);
            QFont bf = opt.font;
            bf.setPixelSize(11);
            bf.setWeight(QFont::Bold);
            const int bh = 20;
            const int bw = qMax(bh, QFontMetrics(bf).horizontalAdvance(badgeText) + 10);
            const QRect badgeRect(cr - bw, r.top() + 36, bw, bh);
            p->setPen(Qt::NoPen);
            p->setBrush(muted ? QColor(pal.textMuted) : QColor(pal.accent));
            p->drawRoundedRect(badgeRect, bh / 2, bh / 2);
            p->setPen(muted ? QColor(pal.bg) : QColor(pal.textOnAccent));
            p->setFont(bf);
            p->drawText(badgeRect, Qt::AlignCenter, badgeText);
            badgeLeftX = badgeRect.left() - 4;
        }

        // Имя (левый верх контента)
        const QString name = idx.data(Qt::DisplayRole).toString();
        QFont nf = opt.font;
        nf.setPixelSize(14);
        nf.setWeight(QFont::DemiBold);
        const int nameMaxW = dateLeftX - cx;
        p->setPen(QColor(pal.textPrimary));
        p->setFont(nf);
        p->drawText(QRect(cx, r.top() + 12, nameMaxW, 20),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(nf).elidedText(name, Qt::ElideRight, nameMaxW));

        // Превью последнего сообщения (левый низ контента)
        const QString preview = idx.data(PreviewRole).toString();
        if (!preview.isEmpty()) {
            QFont pf = opt.font;
            pf.setPixelSize(13);
            const int prevMaxW = badgeLeftX - cx;
            p->setPen(QColor(pal.textSecondary));
            p->setFont(pf);
            p->drawText(QRect(cx, r.top() + 36, prevMaxW, 18),
                        Qt::AlignLeft | Qt::AlignVCenter,
                        QFontMetrics(pf).elidedText(preview, Qt::ElideRight, prevMaxW));
        }

        p->restore();
    }

private:
    static QColor statusColor(int status, const ThemePalette& pal) {
        switch (status) {
        case 1:  return QColor(pal.online);
        case 2:  return QColor("#faa61a");
        case 3:  return QColor(pal.danger);
        default: return QColor(pal.offline);
        }
    }

    static QPixmap loadAvatar(const QString& path, int sz) {
        const QString key = "av_d:" + path + ":" + QString::number(sz);
        QPixmap cached;
        if (QPixmapCache::find(key, &cached)) return cached;

        const QString src = (!path.isEmpty() && QFile::exists(path))
            ? path : QStringLiteral(":/icons/not-avatar.png");
        QPixmap orig(src);
        if (orig.isNull()) return {};

        QPixmap round(sz, sz);
        round.fill(Qt::transparent);
        QPainter pr(&round);
        pr.setRenderHint(QPainter::Antialiasing);
        QPainterPath pp;
        pp.addEllipse(0.0, 0.0, double(sz), double(sz));
        pr.setClipPath(pp);
        pr.drawPixmap(0, 0, orig.scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        QPixmapCache::insert(key, round);
        return round;
    }
};

// ── ContactsWidget ─────────────────────────────────────────────────────────

ContactsWidget::ContactsWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(0);

    // ── Список контактов ──────────────────────────────────────────────────
    m_list = new QListWidget();
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSpacing(0);
    m_list->setMouseTracking(true);
    m_list->viewport()->setMouseTracking(true);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setItemDelegate(new ContactItemDelegate(m_list));

    applyTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](Theme) { applyTheme(); m_list->viewport()->update(); });

    connect(m_list, &QListWidget::itemClicked, this, &ContactsWidget::onItemClicked);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested,
            this, &ContactsWidget::onContextMenuRequested);

    layout->addWidget(m_list, 1);

    // ── Секция групп и каналов ────────────────────────────────────────────
    const ThemePalette& p = ThemeManager::instance().palette();

    auto* groupHeader = new QWidget;
    groupHeader->setFixedHeight(32);
    groupHeader->setStyleSheet(QString("background:%1;border-top:1px solid %2;")
                               .arg(p.bgSurface, p.border));
    auto* ghLay = new QHBoxLayout(groupHeader);
    ghLay->setContentsMargins(12, 0, 8, 0);
    ghLay->setSpacing(4);

    auto* ghLabel = new QLabel("Группы и каналы");
    ghLabel->setStyleSheet(QString("font-size:10px;font-weight:600;color:%1;"
                                   "letter-spacing:0.6px;text-transform:uppercase;")
                           .arg(p.textSecondary));
    auto* joinBtn = new QPushButton("+");
    joinBtn->setToolTip("Вступить в группу или канал");
    joinBtn->setFixedSize(22, 22);
    joinBtn->setObjectName("tinyIconBtn");
    joinBtn->setStyleSheet(QString(
        "QPushButton{background:transparent;color:%1;font-size:16px;border:none;padding:0;}"
        "QPushButton:hover{color:%2;}").arg(p.textSecondary, p.accent));

    ghLay->addWidget(ghLabel, 1);
    ghLay->addWidget(joinBtn);

    connect(joinBtn, &QPushButton::clicked, this, &ContactsWidget::joinGroupRequested);

    m_groupsList = new QListWidget();
    m_groupsList->setFrameShape(QFrame::NoFrame);
    m_groupsList->setSpacing(0);
    m_groupsList->setMouseTracking(true);
    m_groupsList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_groupsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_groupsList->setMaximumHeight(0);  // скрыт пока нет групп
    m_groupsList->setStyleSheet(m_list->styleSheet());

    connect(m_groupsList, &QListWidget::itemClicked, this, &ContactsWidget::onGroupItemClicked);
    m_groupsList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_groupsList, &QListWidget::customContextMenuRequested,
            this, &ContactsWidget::onGroupContextMenu);

    layout->addWidget(groupHeader);
    layout->addWidget(m_groupsList);
}

void ContactsWidget::setContacts(const QList<Contact>& contacts) {
    m_contacts = contacts;
    rebuildList();
}

void ContactsWidget::setPeerPresence(const QUuid& uuid, PeerPresence presence) {
    m_presence[uuid] = presence;
    rebuildList();
}

void ContactsWidget::setPeerOnline(const QUuid& uuid, bool online) {
    setPeerPresence(uuid, online ? PeerPresence::Online : PeerPresence::Offline);
}

void ContactsWidget::updateLastMessage(const QUuid& uuid, const QString& text) {
    m_lastMsg[uuid] = text;
    if (!text.isEmpty())
        m_lastMsgTime[uuid] = QDateTime::currentDateTime();
    rebuildList();
}

void ContactsWidget::updateContactName(const QUuid& uuid, const QString& name) {
    for (auto& c : m_contacts) {
        if (c.uuid == uuid) { c.name = name; break; }
    }
    rebuildList();
}

void ContactsWidget::incrementUnread(const QUuid& uuid) {
    m_unreadCounts[uuid]++;
    rebuildList();
}

void ContactsWidget::clearUnread(const QUuid& uuid) {
    if (m_unreadCounts.remove(uuid) > 0)
        rebuildList();
}

void ContactsWidget::setFilter(const QString& text) {
    m_filter = text;
    rebuildList();
}

void ContactsWidget::rebuildList() {
    m_list->clear();
    for (const auto& c : m_contacts) {
        if (!m_filter.isEmpty() && !c.name.contains(m_filter, Qt::CaseInsensitive))
            continue;

        auto* item = new QListWidgetItem();
        item->setText(c.name);

        int statusVal = 0;
        if (c.isBlocked) {
            statusVal = 3;
        } else {
            switch (m_presence.value(c.uuid, PeerPresence::Offline)) {
            case PeerPresence::Online:       statusVal = 1; break;
            case PeerPresence::Reconnecting: statusVal = 2; break;
            default:                         statusVal = 0; break;
            }
        }

        const bool avatarOk = !c.avatarPath.isEmpty() && QFile::exists(c.avatarPath);
        item->setData(UuidRole,       c.uuid.toString());
        item->setData(StatusRole,     statusVal);
        item->setData(PreviewRole,    m_lastMsg.value(c.uuid));
        item->setData(UnreadRole,     m_unreadCounts.value(c.uuid, 0));
        item->setData(IsMutedRole,    c.isMuted);
        item->setData(AvatarPathRole, avatarOk ? c.avatarPath
                                               : QString(":/icons/not-avatar.png"));

        // Форматируем время последнего сообщения
        const QDateTime dt = m_lastMsgTime.value(c.uuid);
        QString dateStr;
        if (dt.isValid()) {
            const qint64 days = dt.daysTo(QDateTime::currentDateTime());
            if (days == 0)      dateStr = dt.toString("HH:mm");
            else if (days == 1) dateStr = tr("вчера");
            else if (days < 7)  dateStr = QLocale().toString(dt.date(), "ddd");
            else                dateStr = dt.toString("dd.MM.yy");
        }
        item->setData(MsgTimeRole, dateStr);

        m_list->addItem(item);
    }
}

void ContactsWidget::onItemClicked(QListWidgetItem* item) {
    if (!item) return;
    const QUuid uuid(item->data(UuidRole).toString());
    if (!uuid.isNull()) emit contactSelected(uuid);
}

void ContactsWidget::onContextMenuRequested(const QPoint& pos) {
    auto* item = m_list->itemAt(pos);
    if (!item) return;
    const QUuid uuid(item->data(UuidRole).toString());
    if (uuid.isNull()) return;

    bool isBlocked = false, isMuted = false;
    for (const auto& c : m_contacts) {
        if (c.uuid == uuid) { isBlocked = c.isBlocked; isMuted = c.isMuted; break; }
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    auto* viewAct = menu->addAction(
        ThemeManager::tintedIcon(":/icons/ctx_info.png"), tr("Просмотр профиля"));
    menu->addSeparator();
    auto* muteAct = menu->addAction(
        ThemeManager::tintedIcon(isMuted ? ":/icons/ctx_unmute.png" : ":/icons/ctx_mute.png"),
        isMuted ? tr("Включить уведомления") : tr("Заглушить"));
    menu->addSeparator();
    auto* blockAct = menu->addAction(
        ThemeManager::tintedIcon(isBlocked ? ":/icons/ctx_unblock.png" : ":/icons/ctx_block.png"),
        isBlocked ? tr("Разблокировать") : tr("Заблокировать"));
    auto* deleteChatAct = menu->addAction(
        ThemeManager::tintedIcon(":/icons/ctx_clear.png"), tr("Удалить чат"));
    menu->addSeparator();
    auto* deleteContactAct = menu->addAction(
        ThemeManager::tintedIcon(":/icons/ctx_delete.png"), tr("Удалить контакт"));

    connect(viewAct,          &QAction::triggered, this, [this, uuid]{ emit profileRequested(uuid); });
    connect(muteAct,          &QAction::triggered, this, [this, uuid]{ emit muteRequested(uuid); });
    connect(blockAct,         &QAction::triggered, this, [this, uuid]{ emit blockRequested(uuid); });
    connect(deleteChatAct,    &QAction::triggered, this, [this, uuid]{ emit deleteChatRequested(uuid); });
    connect(deleteContactAct, &QAction::triggered, this, [this, uuid]{ emit contactDeleteRequested(uuid); });
    menu->popup(m_list->mapToGlobal(pos));
}

void ContactsWidget::applyTheme() {
    const QString listSS =
        "QListWidget {"
        "  background: transparent; border: none; padding: 4px 0; outline: none;"
        "}"
        "QListWidget::item {"
        "  background: transparent; border: none; padding: 0; margin: 0;"
        "}"
        "QListWidget::item:hover    { background: transparent; }"
        "QListWidget::item:selected { background: transparent; }";
    m_list->setStyleSheet(listSS);
    if (m_groupsList) m_groupsList->setStyleSheet(listSS);
}

// ── Groups ──────────────────────────────────────────────────────────────────

static constexpr int kGroupItemRole = Qt::UserRole + 100;
static constexpr int kGroupConnRole = Qt::UserRole + 101;
static constexpr int kGroupUnreadRole = Qt::UserRole + 102;

void ContactsWidget::setGroups(const QList<Group>& groups) {
    m_groups = groups;
    rebuildGroupList();
}

void ContactsWidget::rebuildGroupList() {
    if (!m_groupsList) return;
    m_groupsList->clear();

    for (const Group& g : m_groups) {
        const bool connected = m_groupConnected.value(g.id, false);
        const int unread     = m_groupUnread.value(g.id, 0);
        const QString lastMsg = m_groupLastMsg.value(g.id);

        const QString icon = g.type == GroupType::Channel ? "📢" : "👥";
        const QString status = connected ? "●" : "○";
        const ThemePalette& p = ThemeManager::instance().palette();

        QString label = icon + " " + g.name;
        if (unread > 0) label += QString("  [%1]").arg(unread);
        if (!lastMsg.isEmpty()) label += "\n" + lastMsg;

        auto* item = new QListWidgetItem(label, m_groupsList);
        item->setData(kGroupItemRole, g.id);
        item->setData(kGroupConnRole, connected);
        item->setData(kGroupUnreadRole, unread);
        item->setForeground(connected ? QColor(p.textPrimary) : QColor(p.textSecondary));
        item->setSizeHint({0, 52});
    }

    // Высота: 52px * N + немного паддинга, максимум 260px
    const int h = qMin(52 * m_groups.size() + 4, 260);
    m_groupsList->setMaximumHeight(m_groups.isEmpty() ? 0 : h);
    m_groupsList->setMinimumHeight(m_groups.isEmpty() ? 0 : qMin(h, 52));
}

void ContactsWidget::addOrUpdateGroup(const Group& g) {
    for (auto& existing : m_groups) {
        if (existing.id == g.id) { existing = g; rebuildGroupList(); return; }
    }
    m_groups.append(g);
    rebuildGroupList();
}

void ContactsWidget::removeGroup(const QString& groupId) {
    m_groups.erase(std::remove_if(m_groups.begin(), m_groups.end(),
        [&](const Group& g) { return g.id == groupId; }), m_groups.end());
    m_groupConnected.remove(groupId);
    m_groupUnread.remove(groupId);
    m_groupLastMsg.remove(groupId);
    rebuildGroupList();
}

void ContactsWidget::setGroupConnected(const QString& groupId, bool connected) {
    m_groupConnected[groupId] = connected;
    rebuildGroupList();
}

void ContactsWidget::incrementGroupUnread(const QString& groupId) {
    m_groupUnread[groupId]++;
    rebuildGroupList();
}

void ContactsWidget::clearGroupUnread(const QString& groupId) {
    m_groupUnread[groupId] = 0;
    rebuildGroupList();
}

void ContactsWidget::updateGroupLastMessage(const QString& groupId, const QString& text) {
    m_groupLastMsg[groupId] = text.length() > 40 ? text.left(40) + "…" : text;
    rebuildGroupList();
}

void ContactsWidget::onGroupItemClicked(QListWidgetItem* item) {
    if (!item) return;
    const QString id = item->data(kGroupItemRole).toString();
    if (!id.isEmpty()) {
        clearGroupUnread(id);
        emit groupSelected(id);
    }
}

void ContactsWidget::onGroupContextMenu(const QPoint& pos) {
    QListWidgetItem* item = m_groupsList->itemAt(pos);
    if (!item) return;
    const QString id = item->data(kGroupItemRole).toString();
    if (id.isEmpty()) return;

    QMenu menu(this);
    const auto* leaveAct = menu.addAction("Покинуть группу");
    if (menu.exec(m_groupsList->viewport()->mapToGlobal(pos)) == leaveAct)
        emit leaveGroupRequested(id);
}
