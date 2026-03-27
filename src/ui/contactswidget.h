#pragma once
#include <QWidget>
#include <QUuid>
#include <QList>
#include "../core/storage.h"

class QListWidget;
class QListWidgetItem;
class QLineEdit;

// ── Присутствие пира — 4 состояния ────────────────────────────────────────
// Отображается цветным кружком рядом с именем в списке контактов.
// Намеренно отделён от ConnectionState (network.h) чтобы UI не зависел от сетевого слоя.
enum class PeerPresence {
    Online,        // 🟢 Подключён, PONG получен
    Reconnecting,  // 🟡 Соединение потеряно, активно пытаемся переподключиться
    Offline,       // ⚫ Нет соединения / недостижим
};

class ContactsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ContactsWidget(QWidget* parent = nullptr);
    void setContacts(const QList<Contact>& contacts);
    // Установить присутствие пира — 4 состояния (Online / Reconnecting / Offline)
    void setPeerPresence(const QUuid& uuid, PeerPresence presence);
    // Совместимость со старым кодом: true → Online, false → Offline
    void setPeerOnline(const QUuid& uuid, bool online);
    void updateLastMessage(const QUuid& uuid, const QString& text);
    // Обновить отображаемое имя контакта без полной перезагрузки списка
    void updateContactName(const QUuid& uuid, const QString& name);

signals:
    void contactSelected(QUuid uuid);
    void profileRequested(QUuid uuid);         // ПКМ → Просмотр профиля
    void blockRequested(QUuid uuid);           // ПКМ → Заблокировать/Разблокировать
    void deleteChatRequested(QUuid uuid);      // ПКМ → Удалить чат
    void contactDeleteRequested(QUuid uuid);   // ПКМ → Удалить контакт

private slots:
    void onItemClicked(QListWidgetItem* item);
    void onSearchChanged(const QString& text);
    void onContextMenuRequested(const QPoint& pos);

private:
    void rebuildList(const QString& filter = {});

    QLineEdit*   m_search{nullptr};
    QListWidget* m_list{nullptr};
    QList<Contact> m_contacts;
    QMap<QUuid, PeerPresence> m_presence;   // Присутствие пиров (4 состояния)
    QMap<QUuid, QString>      m_lastMsg;
};
