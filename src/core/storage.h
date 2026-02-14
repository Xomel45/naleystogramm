#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QUuid>
#include <QDateTime>
#include <QList>

struct Contact {
    QUuid    uuid;
    QString  name;
    QString  ip;
    quint16  port{0};
    QByteArray identityKey; // their public key for E2E
};

struct Message {
    qint64   id{0};
    QUuid    peerUuid;
    bool     outgoing{false};
    QString  text;
    QString  fileName;
    qint64   fileSize{0};
    QByteArray ciphertext;
    QDateTime  timestamp;
    bool     delivered{false};
};

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
    [[nodiscard]] Contact     getContact(const QUuid& uuid) const;
    [[nodiscard]] QList<Contact> allContacts() const;
    [[nodiscard]] bool        deleteContact(const QUuid& uuid);

    // Messages
    [[nodiscard]] qint64      saveMessage(const Message& msg);
    [[nodiscard]] QList<Message> getMessages(const QUuid& peerUuid, int limit = 100) const;
    [[nodiscard]] bool        markDelivered(qint64 msgId);
    [[nodiscard]] QString     lastMessageText(const QUuid& peerUuid) const;
    [[nodiscard]] QDateTime   lastMessageTime(const QUuid& peerUuid) const;

private:
    void        migrate();
    QSqlDatabase m_db;
};
