#include "storage.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QVariant>
#include <QDebug>

StorageManager::StorageManager(QObject* parent) : QObject(parent) {}

StorageManager::~StorageManager() {
    if (m_db.isOpen()) m_db.close();
}

bool StorageManager::open() {
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);

    m_db = QSqlDatabase::addDatabase("QSQLITE", "messenger");
    m_db.setDatabaseName(dir + "/data.db");

    if (!m_db.open()) {
        qCritical("[Storage] Cannot open DB: %s",
                  qPrintable(m_db.lastError().text()));
        return false;
    }

    migrate();
    return true;
}

void StorageManager::migrate() {
    QSqlQuery q(m_db);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA foreign_keys=ON");

    q.exec(R"(
        CREATE TABLE IF NOT EXISTS contacts (
            uuid         TEXT PRIMARY KEY,
            name         TEXT NOT NULL,
            ip           TEXT,
            port         INTEGER DEFAULT 0,
            identity_key BLOB
        )
    )");

    q.exec(R"(
        CREATE TABLE IF NOT EXISTS messages (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            peer_uuid  TEXT NOT NULL,
            outgoing   INTEGER NOT NULL DEFAULT 0,
            text       TEXT NOT NULL DEFAULT '',
            file_name  TEXT,
            file_size  INTEGER DEFAULT 0,
            ciphertext BLOB,
            timestamp  TEXT NOT NULL,
            delivered  INTEGER NOT NULL DEFAULT 0,
            FOREIGN KEY(peer_uuid) REFERENCES contacts(uuid)
        )
    )");

    q.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_msg_peer
        ON messages(peer_uuid, timestamp DESC)
    )");
}

// ── Contacts ──────────────────────────────────────────────────────────────

bool StorageManager::addContact(const Contact& c) {
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT OR REPLACE INTO contacts (uuid, name, ip, port, identity_key)
        VALUES (:uuid, :name, :ip, :port, :key)
    )");
    q.bindValue(":uuid", c.uuid.toString(QUuid::WithoutBraces));
    q.bindValue(":name", c.name);
    q.bindValue(":ip",   c.ip);
    q.bindValue(":port", c.port);
    q.bindValue(":key",  c.identityKey.isEmpty() ? QVariant() : QVariant(c.identityKey));
    if (!q.exec()) {
        qWarning("[Storage] addContact: %s", qPrintable(q.lastError().text()));
        return false;
    }
    return true;
}

bool StorageManager::updateContactAddress(const QUuid& uuid,
                                          const QString& ip, quint16 port) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE contacts SET ip=:ip, port=:port WHERE uuid=:uuid");
    q.bindValue(":ip",   ip);
    q.bindValue(":port", port);
    q.bindValue(":uuid", uuid.toString(QUuid::WithoutBraces));
    return q.exec();
}

bool StorageManager::updateContactKey(const QUuid& uuid, const QByteArray& key) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE contacts SET identity_key=:key WHERE uuid=:uuid");
    q.bindValue(":key",  key);
    q.bindValue(":uuid", uuid.toString(QUuid::WithoutBraces));
    return q.exec();
}

static Contact rowToContact(QSqlQuery& q) {
    Contact c;
    c.uuid        = QUuid(q.value("uuid").toString());
    c.name        = q.value("name").toString();
    c.ip          = q.value("ip").toString();
    c.port        = static_cast<quint16>(q.value("port").toUInt());
    c.identityKey = q.value("identity_key").toByteArray();
    return c;
}

Contact StorageManager::getContact(const QUuid& uuid) const {
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM contacts WHERE uuid=:uuid");
    q.bindValue(":uuid", uuid.toString(QUuid::WithoutBraces));
    q.exec();
    if (q.next()) return rowToContact(q);
    return {};
}

QList<Contact> StorageManager::allContacts() const {
    QSqlQuery q(m_db);
    q.exec("SELECT * FROM contacts ORDER BY name ASC");
    QList<Contact> list;
    while (q.next()) list.append(rowToContact(q));
    return list;
}

bool StorageManager::deleteContact(const QUuid& uuid) {
    const QString s = uuid.toString(QUuid::WithoutBraces);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM messages WHERE peer_uuid=:u");
    q.bindValue(":u", s); q.exec();
    q.prepare("DELETE FROM contacts WHERE uuid=:u");
    q.bindValue(":u", s);
    return q.exec();
}

// ── Messages ──────────────────────────────────────────────────────────────

qint64 StorageManager::saveMessage(const Message& msg) {
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO messages
            (peer_uuid, outgoing, text, file_name, file_size, ciphertext, timestamp, delivered)
        VALUES (:peer, :out, :text, :fname, :fsize, :cipher, :ts, :del)
    )");
    q.bindValue(":peer",   msg.peerUuid.toString(QUuid::WithoutBraces));
    q.bindValue(":out",    msg.outgoing ? 1 : 0);
    q.bindValue(":text",   msg.text);
    q.bindValue(":fname",  msg.fileName.isEmpty() ? QVariant() : msg.fileName);
    q.bindValue(":fsize",  msg.fileSize);
    q.bindValue(":cipher", msg.ciphertext.isEmpty() ? QVariant() : QVariant(msg.ciphertext));
    q.bindValue(":ts",     msg.timestamp.toString(Qt::ISODate));
    q.bindValue(":del",    msg.delivered ? 1 : 0);

    if (!q.exec()) {
        qWarning("[Storage] saveMessage: %s", qPrintable(q.lastError().text()));
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

static Message rowToMessage(QSqlQuery& q) {
    Message m;
    m.id         = q.value("id").toLongLong();
    m.peerUuid   = QUuid(q.value("peer_uuid").toString());
    m.outgoing   = q.value("outgoing").toInt() != 0;
    m.text       = q.value("text").toString();
    m.fileName   = q.value("file_name").toString();
    m.fileSize   = q.value("file_size").toLongLong();
    m.ciphertext = q.value("ciphertext").toByteArray();
    m.timestamp  = QDateTime::fromString(q.value("timestamp").toString(), Qt::ISODate);
    m.delivered  = q.value("delivered").toInt() != 0;
    return m;
}

QList<Message> StorageManager::getMessages(const QUuid& peerUuid, int limit) const {
    QSqlQuery q(m_db);
    q.prepare(R"(
        SELECT * FROM messages WHERE peer_uuid=:uuid
        ORDER BY timestamp ASC
        LIMIT :lim
    )");
    q.bindValue(":uuid", peerUuid.toString(QUuid::WithoutBraces));
    q.bindValue(":lim",  limit);
    q.exec();

    QList<Message> list;
    while (q.next()) list.append(rowToMessage(q));
    return list;
}

bool StorageManager::markDelivered(qint64 msgId) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE messages SET delivered=1 WHERE id=:id");
    q.bindValue(":id", msgId);
    return q.exec();
}

QString StorageManager::lastMessageText(const QUuid& peerUuid) const {
    QSqlQuery q(m_db);
    q.prepare(R"(
        SELECT text, file_name FROM messages WHERE peer_uuid=:uuid
        ORDER BY timestamp DESC LIMIT 1
    )");
    q.bindValue(":uuid", peerUuid.toString(QUuid::WithoutBraces));
    q.exec();
    if (!q.next()) return {};
    const QString fname = q.value("file_name").toString();
    return fname.isEmpty() ? q.value("text").toString()
                           : QString("[Файл: %1]").arg(fname);
}

QDateTime StorageManager::lastMessageTime(const QUuid& peerUuid) const {
    QSqlQuery q(m_db);
    q.prepare(R"(
        SELECT timestamp FROM messages WHERE peer_uuid=:uuid
        ORDER BY timestamp DESC LIMIT 1
    )");
    q.bindValue(":uuid", peerUuid.toString(QUuid::WithoutBraces));
    q.exec();
    if (!q.next()) return {};
    return QDateTime::fromString(q.value("timestamp").toString(), Qt::ISODate);
}
