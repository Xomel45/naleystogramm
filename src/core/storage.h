#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include <QJsonObject>
#include "types.h"

class StorageManager : public QObject {
    Q_OBJECT
public:
    explicit StorageManager(QObject* parent = nullptr);
    ~StorageManager();

    bool        open();

    // Contacts
    [[nodiscard]] bool        addContact(const Contact& c);
    [[nodiscard]] bool        updateContactAddress(const QUuid& uuid, const QString& ip, quint16 port);
    [[nodiscard]] bool        updateContactKey(const QUuid& uuid, const QByteArray& identityKey);
    [[nodiscard]] bool        updateAvatar(const QUuid& uuid, const QString& hash, const QString& path);
    [[nodiscard]] bool        updateContactName(const QUuid& uuid, const QString& name);
    // Сохранить снимок системной информации пира (CPU/RAM/OS) — для показа в профиле офлайн
    [[nodiscard]] bool        updateContactSystemInfo(const QUuid& uuid, const QJsonObject& info);
    [[nodiscard]] bool        updateContactBirthday(const QUuid& uuid, const QString& birthday);
    // Установить/снять блокировку контакта (все сообщения от него игнорируются)
    [[nodiscard]] bool        blockContact(const QUuid& uuid, bool blocked);
    // Включить/отключить уведомления от контакта
    [[nodiscard]] bool        setContactMuted(const QUuid& uuid, bool muted);
    // Записать текущее время как «последний раз онлайн» для контакта
    bool        updateLastSeen(const QUuid& uuid);
    [[nodiscard]] Contact     getContact(const QUuid& uuid) const;
    [[nodiscard]] QList<Contact> allContacts() const;
    // Удалить контакт и переписку. Если был заблокирован — UUID сохраняется в blocked_list.
    [[nodiscard]] bool        deleteContact(const QUuid& uuid);
    // Проверить заблокирован ли UUID (существует ли в blocked_list — для удалённых контактов)
    [[nodiscard]] bool        isUuidBlocked(const QUuid& uuid) const;
    // Удалить только переписку, не удаляя сам контакт
    [[nodiscard]] bool        clearMessages(const QUuid& uuid);

    // Messages
    [[nodiscard]] qint64      saveMessage(const Message& msg);
    // limit=50 — последние N сообщений; offset — сколько с конца пропустить (для lazy loading)
    [[nodiscard]] QList<Message> getMessages(const QUuid& peerUuid, int limit = 50, int offset = 0) const;
    [[nodiscard]] bool        markDelivered(qint64 msgId);
    [[nodiscard]] QString     lastMessageText(const QUuid& peerUuid) const;
    [[nodiscard]] QDateTime   lastMessageTime(const QUuid& peerUuid) const;

private:
    void        migrate();
    QSqlDatabase m_db;
};
