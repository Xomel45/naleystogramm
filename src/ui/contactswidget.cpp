#include "contactswidget.h"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QAction>
#include <QFile>
#include <QPixmap>
#include <QPixmapCache>
#include <QPainter>
#include <QIcon>
#include <QDebug>
#include "thememanager.h"

ContactsWidget::ContactsWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 0);
    layout->setSpacing(4);

    m_search = new QLineEdit();
    m_search->setPlaceholderText(tr("Search..."));
    m_search->setObjectName("searchInput");

    m_list = new QListWidget();
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSpacing(2);
    m_list->setIconSize(QSize(40, 40));
    m_list->setUniformItemSizes(true);

    applyTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](Theme) { applyTheme(); });

    connect(m_search, &QLineEdit::textChanged, this, &ContactsWidget::onSearchChanged);
    connect(m_list, &QListWidget::itemClicked, this, &ContactsWidget::onItemClicked);

    // Контекстное меню по правой кнопке
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested,
            this, &ContactsWidget::onContextMenuRequested);

    layout->addWidget(m_search);
    layout->addWidget(m_list, 1);
}

void ContactsWidget::setContacts(const QList<Contact>& contacts) {
    m_contacts = contacts;
    rebuildList(m_search->text());
}

void ContactsWidget::setPeerPresence(const QUuid& uuid, PeerPresence presence) {
    m_presence[uuid] = presence;
    rebuildList(m_search->text());
}

// Совместимость: true → Online, false → Offline (не затрагивает Reconnecting)
void ContactsWidget::setPeerOnline(const QUuid& uuid, bool online) {
    setPeerPresence(uuid, online ? PeerPresence::Online : PeerPresence::Offline);
}

void ContactsWidget::updateLastMessage(const QUuid& uuid, const QString& text) {
    m_lastMsg[uuid] = text;
    rebuildList(m_search->text());
}

void ContactsWidget::updateContactName(const QUuid& uuid, const QString& name) {
    // Находим контакт в локальном списке и обновляем имя
    for (auto& c : m_contacts) {
        if (c.uuid == uuid) {
            c.name = name;
            break;
        }
    }
    rebuildList(m_search->text());
}

// Создаёт круглую иконку аватара из указанного пути к файлу.
// Результат кэшируется в QPixmapCache (LRU, 10 МБ по умолчанию).
// Ключ = "av:" + path — аватары хранятся как avatars/{hash}.png,
// поэтому смена аватара даёт новый ключ автоматически.
static QIcon makeRoundAvatar(const QString& path) {
    if (path.isEmpty() || !QFile::exists(path))
        return {};

    // Проверяем кэш — O(1) на повторных вызовах
    const QString cacheKey = "av:" + path;
    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached))
        return QIcon(cached);

    QPixmap src(path);
    if (src.isNull())
        return {};

    // Масштабируем до 40×40 с высоким качеством
    const QPixmap scaled = src.scaled(40, 40,
        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // Рисуем в круглую маску (ellipse clip)
    QPixmap round(40, 40);
    round.fill(Qt::transparent);
    QPainter painter(&round);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QBrush(scaled));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, 40, 40);

    QPixmapCache::insert(cacheKey, round);
    return QIcon(round);
}

static QString formatLastSeen(const QDateTime& dt) {
    if (dt.isNull()) return {};
    const qint64 secs = dt.secsTo(QDateTime::currentDateTime());
    if (secs < 60)      return QObject::tr("только что");
    if (secs < 3600)    return QObject::tr("был(а) %1 мин назад").arg(secs / 60);
    if (secs < 86400)   return QObject::tr("был(а) %1 ч назад").arg(secs / 3600);
    if (secs < 604800)  return QObject::tr("был(а) %1 дн назад").arg(secs / 86400);
    return QObject::tr("был(а) %1").arg(dt.toString("dd.MM.yyyy"));
}

void ContactsWidget::rebuildList(const QString& filter) {
    m_list->clear();
    for (const auto& c : m_contacts) {
        if (!filter.isEmpty() &&
            !c.name.contains(filter, Qt::CaseInsensitive)) continue;

        const QString last = m_lastMsg.value(c.uuid, "");

        auto* item = new QListWidgetItem();

        // ── Индикатор присутствия ────────────────────────────────────────────
        // Заблокированный — красный, перекрывает все остальные состояния.
        // Остальные: зелёный (онлайн), жёлтый (переподключение), серый (офлайн).
        QString bullet;
        if (c.isBlocked) {
            bullet = "🔴 ";
        } else {
            switch (m_presence.value(c.uuid, PeerPresence::Offline)) {
            case PeerPresence::Online:       bullet = "🟢 "; break;
            case PeerPresence::Reconnecting: bullet = "🟡 "; break;
            default:                         bullet = "⚫ "; break;
            }
        }

        // Заблокированные отображаем со значком; заглушённые — со значком 🔕
        const QString nameStr = c.isBlocked ? (c.name + tr(" 🚫"))
                                            : (c.isMuted ? (QString("🔕 ") + c.name) : c.name);
        const int unread = m_unreadCounts.value(c.uuid, 0);
        const QString badge = unread > 0 ? QString("  [%1]").arg(unread) : QString();
        const QString preview = last.isEmpty() ? "" :
            QString("\n  %1").arg(last.left(40));
        const bool isOnline = m_presence.value(c.uuid, PeerPresence::Offline) != PeerPresence::Offline;
        const QString lastSeenStr = (!isOnline && !c.lastSeen.isNull())
            ? QString("\n  %1").arg(formatLastSeen(c.lastSeen))
            : QString();
        item->setText(bullet + nameStr + badge + preview + lastSeenStr);
        if (unread > 0)
            item->setForeground(QColor("#b8a0ff"));
        item->setData(Qt::UserRole, c.uuid.toString());
        if (c.isBlocked)
            item->setForeground(QColor("#e05555"));

        // Аватар контакта — своё фото или заглушка
        const bool avatarExists = !c.avatarPath.isEmpty() && QFile::exists(c.avatarPath);
        const QString iconPath = avatarExists
            ? c.avatarPath
            : QStringLiteral(":/icons/not-avatar.png");
        const QIcon icon = makeRoundAvatar(iconPath);
        if (!icon.isNull())
            item->setIcon(icon);

        m_list->addItem(item);
    }
}

void ContactsWidget::onItemClicked(QListWidgetItem* item) {
    if (!item) return;
    const QUuid uuid(item->data(Qt::UserRole).toString());
    if (!uuid.isNull()) emit contactSelected(uuid);
}

void ContactsWidget::onSearchChanged(const QString& text) {
    rebuildList(text);
}

void ContactsWidget::incrementUnread(const QUuid& uuid) {
    m_unreadCounts[uuid]++;
    rebuildList(m_search->text());
}

void ContactsWidget::clearUnread(const QUuid& uuid) {
    if (m_unreadCounts.remove(uuid) > 0)
        rebuildList(m_search->text());
}

void ContactsWidget::onContextMenuRequested(const QPoint& pos) {
    auto* item = m_list->itemAt(pos);
    if (!item) return;
    const QUuid uuid(item->data(Qt::UserRole).toString());
    if (uuid.isNull()) return;

    // Определяем текущий статус блокировки и заглушения для правильных надписей меню
    bool isBlocked = false;
    bool isMuted   = false;
    for (const auto& c : m_contacts) {
        if (c.uuid == uuid) { isBlocked = c.isBlocked; isMuted = c.isMuted; break; }
    }

    QMenu menu(this);
    auto* viewAct      = menu.addAction(ThemeManager::tintedIcon(":/icons/ctx_info.png"),    tr("Просмотр профиля"));
    menu.addSeparator();
    auto* muteAct      = menu.addAction(
        ThemeManager::tintedIcon(isMuted ? ":/icons/ctx_unmute.png" : ":/icons/ctx_mute.png"),
        isMuted ? tr("Включить уведомления") : tr("Заглушить"));
    menu.addSeparator();
    auto* blockAct     = menu.addAction(
        ThemeManager::tintedIcon(isBlocked ? ":/icons/ctx_unblock.png" : ":/icons/ctx_block.png"),
        isBlocked ? tr("Разблокировать") : tr("Заблокировать"));
    auto* deleteChatAct = menu.addAction(ThemeManager::tintedIcon(":/icons/ctx_clear.png"),  tr("Удалить чат"));
    menu.addSeparator();
    auto* deleteContactAct = menu.addAction(ThemeManager::tintedIcon(":/icons/ctx_delete.png"), tr("Удалить контакт"));

    connect(viewAct, &QAction::triggered, this, [this, uuid]() {
        emit profileRequested(uuid);
    });
    connect(muteAct, &QAction::triggered, this, [this, uuid]() {
        emit muteRequested(uuid);
    });
    connect(blockAct, &QAction::triggered, this, [this, uuid]() {
        emit blockRequested(uuid);
    });
    connect(deleteChatAct, &QAction::triggered, this, [this, uuid]() {
        emit deleteChatRequested(uuid);
    });
    connect(deleteContactAct, &QAction::triggered, this, [this, uuid]() {
        emit contactDeleteRequested(uuid);
    });
    menu.exec(m_list->mapToGlobal(pos));
}

void ContactsWidget::applyTheme() {
    const auto& p = ThemeManager::instance().palette();
    m_search->setStyleSheet(QString(R"(
        QLineEdit {
            background: %1; border: 1px solid %2;
            border-radius: 16px; padding: 6px 14px;
            color: %3; font-size: 13px;
        }
        QLineEdit:focus { border-color: %4; }
    )").arg(p.bgInput, p.border, p.textPrimary, p.borderFocus));

    m_list->setStyleSheet(QString(R"(
        QListWidget { background: transparent; border: none; }
        QListWidget::item {
            background: transparent;
            border-radius: 10px;
            padding: 8px 6px;
            color: %1;
        }
        QListWidget::item:hover    { background: %2; }
        QListWidget::item:selected { background: %2; border: 1px solid %3; }
    )").arg(p.textPrimary, p.bgElevated, p.border));
}
