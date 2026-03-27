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

ContactsWidget::ContactsWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 0);
    layout->setSpacing(4);

    m_search = new QLineEdit();
    m_search->setPlaceholderText(tr("Search..."));
    m_search->setObjectName("searchInput");
    m_search->setStyleSheet(R"(
        QLineEdit {
            background: #1e1e36; border: 1px solid #2a2a4a;
            border-radius: 16px; padding: 6px 14px;
            color: #e8e6ff; font-size: 13px;
        }
        QLineEdit:focus { border-color: #7c6aff; }
    )");

    m_list = new QListWidget();
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSpacing(2);
    // Размер иконки аватара в списке контактов (круглый 40×40 пикс.)
    m_list->setIconSize(QSize(40, 40));
    // Все элементы одинаковой высоты — Qt пропускает per-item sizeHint
    m_list->setUniformItemSizes(true);
    m_list->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; }
        QListWidget::item {
            background: transparent;
            border-radius: 10px;
            padding: 8px 6px;
            color: #e8e6ff;
        }
        QListWidget::item:hover    { background: #1e1e36; }
        QListWidget::item:selected { background: #1e1e36; border: 1px solid #2a2a4a; }
    )");

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

        // Заблокированные отображаем со значком и красным цветом
        const QString nameStr = c.isBlocked ? (c.name + tr(" 🚫")) : c.name;
        const QString preview = last.isEmpty() ? "" :
            QString("\n  %1").arg(last.left(40));
        item->setText(bullet + nameStr + preview);
        item->setData(Qt::UserRole, c.uuid.toString());
        if (c.isBlocked)
            item->setForeground(QColor("#e05555"));

        // Загружаем аватар из кэша если он есть
        const bool avatarExists = !c.avatarPath.isEmpty() && QFile::exists(c.avatarPath);
        qDebug("[Contacts] Аватар для \"%s\": путь=%s  существует=%s",
               qPrintable(c.name),
               c.avatarPath.isEmpty() ? "(нет)" : qPrintable(c.avatarPath),
               avatarExists ? "ДА" : "НЕТ");
        if (avatarExists) {
            const QIcon icon = makeRoundAvatar(c.avatarPath);
            if (!icon.isNull())
                item->setIcon(icon);
        }

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

void ContactsWidget::onContextMenuRequested(const QPoint& pos) {
    auto* item = m_list->itemAt(pos);
    if (!item) return;
    const QUuid uuid(item->data(Qt::UserRole).toString());
    if (uuid.isNull()) return;

    // Определяем текущий статус блокировки для правильной надписи пункта меню
    bool isBlocked = false;
    for (const auto& c : m_contacts) {
        if (c.uuid == uuid) { isBlocked = c.isBlocked; break; }
    }

    QMenu menu(this);
    auto* viewAct      = menu.addAction(tr("Просмотр профиля"));
    menu.addSeparator();
    // Надпись меняется в зависимости от текущего состояния
    auto* blockAct     = menu.addAction(isBlocked ? tr("Разблокировать") : tr("Заблокировать"));
    auto* deleteChatAct = menu.addAction(tr("Удалить чат"));
    menu.addSeparator();
    // Удаление контакта — деструктивное действие, отделено разделителем
    auto* deleteContactAct = menu.addAction(tr("Удалить контакт"));

    connect(viewAct, &QAction::triggered, this, [this, uuid]() {
        emit profileRequested(uuid);
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
