#pragma once
#include <QWidget>
#include <QUuid>
#include <QList>
#include <QDateTime>
#include <QMap>
#include "../core/types.h"

class QListWidget;
class QListWidgetItem;

// ── Присутствие пира — 4 состояния ────────────────────────────────────────
enum class PeerPresence {
    Online,        // зелёный: подключён, PONG получен
    Reconnecting,  // жёлтый: соединение потеряно, пытаемся переподключиться
    Offline,       // серый: нет соединения
};

// ── Кастомные роли элементов списка ───────────────────────────────────────
enum ContactItemRole {
    UuidRole       = Qt::UserRole,
    PreviewRole    = Qt::UserRole + 1,
    UnreadRole     = Qt::UserRole + 2,
    StatusRole     = Qt::UserRole + 3,  // 0=офлайн 1=онлайн 2=переподключение 3=заблокирован
    IsMutedRole    = Qt::UserRole + 4,
    MsgTimeRole    = Qt::UserRole + 5,  // QString — отформатированное время
    AvatarPathRole = Qt::UserRole + 6,
};

class ContactsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ContactsWidget(QWidget* parent = nullptr);

    void setContacts(const QList<Contact>& contacts);
    void setPeerPresence(const QUuid& uuid, PeerPresence presence);
    void setPeerOnline(const QUuid& uuid, bool online);
    void updateLastMessage(const QUuid& uuid, const QString& text);
    void updateContactName(const QUuid& uuid, const QString& name);
    void incrementUnread(const QUuid& uuid);
    void clearUnread(const QUuid& uuid);

public slots:
    void setFilter(const QString& text);

signals:
    void contactSelected(QUuid uuid);
    void profileRequested(QUuid uuid);
    void blockRequested(QUuid uuid);
    void muteRequested(QUuid uuid);
    void deleteChatRequested(QUuid uuid);
    void contactDeleteRequested(QUuid uuid);

private slots:
    void onItemClicked(QListWidgetItem* item);
    void onContextMenuRequested(const QPoint& pos);

private:
    void rebuildList();
    void applyTheme();

    QListWidget*              m_list        {nullptr};
    QList<Contact>            m_contacts;
    QMap<QUuid, PeerPresence> m_presence;
    QMap<QUuid, QString>      m_lastMsg;
    QMap<QUuid, QDateTime>    m_lastMsgTime;
    QMap<QUuid, int>          m_unreadCounts;
    QString                   m_filter;
};
