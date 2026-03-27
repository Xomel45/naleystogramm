#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QUuid>
#include <QDateTime>
#include <QList>
#include <QJsonObject>

struct Contact {
    QUuid      uuid;
    QString    name;
    QString    ip;
    quint16    port{0};
    QByteArray identityKey;   // открытый ключ пира для E2E
    QString    avatarHash {};  // SHA-256 hex кэшированного аватара (пусто = нет)
    QString    avatarPath {};  // абсолютный путь к файлу аватара в кэше
    bool       isBlocked {false}; // контакт заблокирован — все сообщения игнорируются
    QString    systemInfoJson {}; // JSON-снимок системной информации пира (CPU/RAM/OS)
    // Версия приложения, создавшего/обновившего запись. Позволяет обнаружить
    // несовместимость при запуске более старой версии на той же базе данных.
    // Для устаревших записей (до v0.5.1) устанавливается "0.1.0" по умолчанию.
    QString    versionCreated {"0.1.0"};
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
    bool     isVoice{false};        // true = голосовое сообщение
    int      voiceDurationMs{0};    // длительность голосового в миллисекундах
    // Версия приложения, сохранившего сообщение. Для устаревших записей — "0.1.0".
    QString  versionCreated {"0.1.0"};
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
    [[nodiscard]] bool        updateAvatar(const QUuid& uuid, const QString& hash, const QString& path);
    [[nodiscard]] bool        updateContactName(const QUuid& uuid, const QString& name);
    // Сохранить снимок системной информации пира (CPU/RAM/OS) — для показа в профиле офлайн
    [[nodiscard]] bool        updateContactSystemInfo(const QUuid& uuid, const QJsonObject& info);
    // Установить/снять блокировку контакта (все сообщения от него игнорируются)
    [[nodiscard]] bool        blockContact(const QUuid& uuid, bool blocked);
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
