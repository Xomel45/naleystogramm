#include "storage.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QVariant>
#include <QDebug>
#include <QJsonDocument>
#ifdef HAVE_SQLCIPHER
#  include "../crypto/keyprotector.h"
#endif

// Текущая версия приложения — проставляется в version_created при каждой записи.
// При версионном обновлении менять одновременно с updatechecker.h и sessionmanager.cpp.
static constexpr const char* kStorageAppVersion = "0.5.3";

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

#ifdef HAVE_SQLCIPHER
    // Шифрование at-rest: ключ выводится из случайного мастер-ключа через HKDF.
    // Это НАМНОГО безопаснее чем UUID: мастер-ключ — 256 бит случайных данных,
    // хранится отдельно в AppData/master.key с правами 0600.
    // PRAGMA key должен быть первой операцией после открытия соединения.
    {
        const QByteArray dbKey = KeyProtector::instance().deriveKey(
            QByteArrayLiteral("naleystogramm-db-key-v1"));
        if (dbKey.isEmpty()) {
            qCritical("[Storage] KeyProtector не готов — SQLCipher ключ не получен!");
        } else {
            const QString hexKey = QString::fromLatin1(dbKey.toHex());
            QSqlQuery keyQuery(m_db);
            // Формат SQLCipher для hex-ключа: x'<hex>'
            if (!keyQuery.exec(QString("PRAGMA key = \"x'%1'\"").arg(hexKey))) {
                qWarning("[Storage] SQLCipher PRAGMA key failed: %s",
                         qPrintable(keyQuery.lastError().text()));
            } else {
                qDebug("[Storage] SQLCipher шифрование включено (ключ из мастер-ключа)");
            }
        }
    }
#endif

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

    // id — AUTOINCREMENT, всегда монотонно возрастает → быстрее сортировки строкового timestamp
    q.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_msg_id
        ON messages(peer_uuid, id DESC)
    )");

    // Таблица заблокированных UUID — сохраняется при удалении заблокированного контакта.
    // Позволяет игнорировать входящие подключения от ранее заблокированных пиров.
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS blocked_list (
            uuid       TEXT PRIMARY KEY,
            blocked_at TEXT NOT NULL DEFAULT ''
        )
    )");

    // ── Безопасная миграция контактов: добавляем колонки если их нет ─────────
    QSqlQuery info(m_db);
    info.exec("PRAGMA table_info(contacts)");
    QStringList cols;
    while (info.next()) cols << info.value("name").toString();

    QSqlQuery alter(m_db);
    if (!cols.contains("avatar_hash"))
        alter.exec("ALTER TABLE contacts ADD COLUMN avatar_hash TEXT DEFAULT ''");
    if (!cols.contains("avatar_path"))
        alter.exec("ALTER TABLE contacts ADD COLUMN avatar_path TEXT DEFAULT ''");
    // Колонка блокировки — добавляем если её нет (обратная совместимость)
    if (!cols.contains("is_blocked"))
        alter.exec("ALTER TABLE contacts ADD COLUMN is_blocked INTEGER NOT NULL DEFAULT 0");
    // Системная информация пира — CPU/RAM/OS; хранится как JSON-строка
    if (!cols.contains("systeminfo_json"))
        alter.exec("ALTER TABLE contacts ADD COLUMN systeminfo_json TEXT NOT NULL DEFAULT '{}'");
    // Версия приложения, создавшего/обновившего запись контакта
    if (!cols.contains("version_created"))
        alter.exec("ALTER TABLE contacts ADD COLUMN version_created TEXT NOT NULL DEFAULT '0.1.0'");

    // ── Безопасная миграция сообщений: колонки для голосовых сообщений ───────
    QSqlQuery msgInfo(m_db);
    msgInfo.exec("PRAGMA table_info(messages)");
    QStringList msgCols;
    while (msgInfo.next()) msgCols << msgInfo.value("name").toString();

    if (!msgCols.contains("is_voice"))
        alter.exec("ALTER TABLE messages ADD COLUMN is_voice INTEGER NOT NULL DEFAULT 0");
    if (!msgCols.contains("voice_duration_ms"))
        alter.exec("ALTER TABLE messages ADD COLUMN voice_duration_ms INTEGER NOT NULL DEFAULT 0");
    // Версия приложения, сохранившего сообщение
    if (!msgCols.contains("version_created"))
        alter.exec("ALTER TABLE messages ADD COLUMN version_created TEXT NOT NULL DEFAULT '0.1.0'");
}

// ── Contacts ──────────────────────────────────────────────────────────────

bool StorageManager::addContact(const Contact& c) {
    const QString uuidStr = c.uuid.toString(QUuid::WithoutBraces);

    // Проверяем существование — INSERT OR REPLACE ломает FK-ссылки из messages (удаляет строку)
    QSqlQuery check(m_db);
    check.prepare("SELECT COUNT(*) FROM contacts WHERE uuid=:uuid");
    check.bindValue(":uuid", uuidStr);
    check.exec();
    const bool exists = check.next() && check.value(0).toInt() > 0;

    QSqlQuery q(m_db);
    if (exists) {
        // Контакт уже есть — обновляем адрес, имя и версию (не трогаем ключи/аватар/блокировку)
        q.prepare("UPDATE contacts SET name=:name, ip=:ip, port=:port, version_created=:ver WHERE uuid=:uuid");
        q.bindValue(":ver", QString(kStorageAppVersion));
    } else {
        q.prepare(R"(
            INSERT INTO contacts
                (uuid, name, ip, port, identity_key, avatar_hash, avatar_path,
                 is_blocked, systeminfo_json, version_created)
            VALUES (:uuid, :name, :ip, :port, :key, :ahash, :apath, 0, :sysinfo, :ver)
        )");
        q.bindValue(":key",     c.identityKey.isEmpty() ? QVariant() : QVariant(c.identityKey));
        q.bindValue(":ahash",   c.avatarHash);
        q.bindValue(":apath",   c.avatarPath);
        q.bindValue(":sysinfo", c.systemInfoJson.isEmpty() ? QStringLiteral("{}") : c.systemInfoJson);
        q.bindValue(":ver",     QString(kStorageAppVersion));
    }
    q.bindValue(":uuid", uuidStr);
    q.bindValue(":name", c.name);
    q.bindValue(":ip",   c.ip);
    q.bindValue(":port", c.port);
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

bool StorageManager::updateContactName(const QUuid& uuid, const QString& name) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE contacts SET name=:name WHERE uuid=:uuid");
    q.bindValue(":name", name);
    q.bindValue(":uuid", uuid.toString(QUuid::WithoutBraces));
    if (!q.exec()) {
        qWarning("[Storage] updateContactName: %s", qPrintable(q.lastError().text()));
        return false;
    }
    return true;
}

bool StorageManager::updateAvatar(const QUuid& uuid,
                                   const QString& hash, const QString& path) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE contacts SET avatar_hash=:hash, avatar_path=:path WHERE uuid=:uuid");
    q.bindValue(":hash", hash);
    q.bindValue(":path", path);
    q.bindValue(":uuid", uuid.toString(QUuid::WithoutBraces));
    if (!q.exec()) {
        qWarning("[Storage] updateAvatar: %s", qPrintable(q.lastError().text()));
        return false;
    }
    return true;
}

static Contact rowToContact(QSqlQuery& q) {
    Contact c;
    c.uuid        = QUuid(q.value("uuid").toString());
    c.name        = q.value("name").toString();
    c.ip          = q.value("ip").toString();
    c.port        = static_cast<quint16>(q.value("port").toUInt());
    c.identityKey = q.value("identity_key").toByteArray();
    c.avatarHash      = q.value("avatar_hash").toString();
    c.avatarPath      = q.value("avatar_path").toString();
    c.isBlocked       = q.value("is_blocked").toInt() != 0;
    c.systemInfoJson  = q.value("systeminfo_json").toString();
    // versionCreated: для старых записей (до v0.5.1) — дефолтное значение "0.1.0"
    const QString vc = q.value("version_created").toString();
    c.versionCreated  = vc.isEmpty() ? QStringLiteral("0.1.0") : vc;
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

    // Если контакт был заблокирован — сохраняем UUID в blocked_list до удаления.
    // Это позволит отклонять входящие сообщения от него даже после удаления из contacts.
    {
        QSqlQuery checkBlocked(m_db);
        checkBlocked.prepare("SELECT is_blocked FROM contacts WHERE uuid=:u");
        checkBlocked.bindValue(":u", s);
        if (checkBlocked.exec() && checkBlocked.next() && checkBlocked.value(0).toInt() != 0) {
            QSqlQuery bl(m_db);
            bl.prepare("INSERT OR REPLACE INTO blocked_list (uuid, blocked_at) VALUES (:u, :t)");
            bl.bindValue(":u", s);
            bl.bindValue(":t", QDateTime::currentDateTime().toString(Qt::ISODate));
            bl.exec();
            qDebug("[Storage] deleteContact: UUID %s сохранён в blocked_list", qPrintable(s));
        }
    }

    q.prepare("DELETE FROM messages WHERE peer_uuid=:u");
    q.bindValue(":u", s); q.exec();
    q.prepare("DELETE FROM contacts WHERE uuid=:u");
    q.bindValue(":u", s);
    const bool ok = q.exec();
    if (ok) qDebug("[Storage] Контакт %s удалён", qPrintable(s));
    else    qWarning("[Storage] deleteContact: %s", qPrintable(q.lastError().text()));
    return ok;
}

bool StorageManager::isUuidBlocked(const QUuid& uuid) const {
    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM blocked_list WHERE uuid=:u");
    q.bindValue(":u", uuid.toString(QUuid::WithoutBraces));
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

bool StorageManager::blockContact(const QUuid& uuid, bool blocked) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE contacts SET is_blocked=:v WHERE uuid=:uuid");
    q.bindValue(":v",    blocked ? 1 : 0);
    q.bindValue(":uuid", uuid.toString(QUuid::WithoutBraces));
    if (!q.exec()) {
        qWarning("[Storage] blockContact: %s", qPrintable(q.lastError().text()));
        return false;
    }
    return true;
}

bool StorageManager::updateContactSystemInfo(const QUuid& uuid, const QJsonObject& info) {
    // Сериализуем QJsonObject в компактную JSON-строку для хранения в одной колонке
    const QString json = QString::fromUtf8(
        QJsonDocument(info).toJson(QJsonDocument::Compact));
    QSqlQuery q(m_db);
    q.prepare("UPDATE contacts SET systeminfo_json=:v WHERE uuid=:uuid");
    q.bindValue(":v",    json);
    q.bindValue(":uuid", uuid.toString(QUuid::WithoutBraces));
    if (!q.exec()) {
        qWarning("[Storage] updateContactSystemInfo: %s",
                 qPrintable(q.lastError().text()));
        return false;
    }
    return true;
}

bool StorageManager::clearMessages(const QUuid& uuid) {
    // Удаляем только переписку, контакт остаётся
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM messages WHERE peer_uuid=:u");
    q.bindValue(":u", uuid.toString(QUuid::WithoutBraces));
    if (!q.exec()) {
        qWarning("[Storage] clearMessages: %s", qPrintable(q.lastError().text()));
        return false;
    }
    return true;
}

// ── Messages ──────────────────────────────────────────────────────────────

qint64 StorageManager::saveMessage(const Message& msg) {
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO messages
            (peer_uuid, outgoing, text, file_name, file_size, ciphertext,
             timestamp, delivered, is_voice, voice_duration_ms, version_created)
        VALUES (:peer, :out, :text, :fname, :fsize, :cipher,
                :ts, :del, :voice, :vdur, :ver)
    )");
    q.bindValue(":peer",   msg.peerUuid.toString(QUuid::WithoutBraces));
    q.bindValue(":out",    msg.outgoing ? 1 : 0);
    q.bindValue(":text",   msg.text);
    q.bindValue(":fname",  msg.fileName.isEmpty() ? QVariant() : msg.fileName);
    q.bindValue(":fsize",  msg.fileSize);
    q.bindValue(":cipher", msg.ciphertext.isEmpty() ? QVariant() : QVariant(msg.ciphertext));
    q.bindValue(":ts",     msg.timestamp.toString(Qt::ISODate));
    q.bindValue(":del",    msg.delivered ? 1 : 0);
    q.bindValue(":voice",  msg.isVoice ? 1 : 0);
    q.bindValue(":vdur",   msg.voiceDurationMs);
    q.bindValue(":ver",    QString(kStorageAppVersion));

    if (!q.exec()) {
        qWarning("[Storage] saveMessage: %s", qPrintable(q.lastError().text()));
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

static Message rowToMessage(QSqlQuery& q) {
    Message m;
    m.id              = q.value("id").toLongLong();
    m.peerUuid        = QUuid(q.value("peer_uuid").toString());
    m.outgoing        = q.value("outgoing").toInt() != 0;
    m.text            = q.value("text").toString();
    m.fileName        = q.value("file_name").toString();
    m.fileSize        = q.value("file_size").toLongLong();
    m.ciphertext      = q.value("ciphertext").toByteArray();
    m.timestamp       = QDateTime::fromString(q.value("timestamp").toString(), Qt::ISODate);
    m.delivered       = q.value("delivered").toInt() != 0;
    m.isVoice         = q.value("is_voice").toInt() != 0;
    m.voiceDurationMs = q.value("voice_duration_ms").toInt();
    // versionCreated: для старых записей (до v0.5.1) — дефолтное значение "0.1.0"
    const QString mvc = q.value("version_created").toString();
    m.versionCreated  = mvc.isEmpty() ? QStringLiteral("0.1.0") : mvc;
    return m;
}

QList<Message> StorageManager::getMessages(const QUuid& peerUuid,
                                            int limit, int offset) const {
    QSqlQuery q(m_db);
    // Получаем последние limit сообщений (с пропуском offset от конца),
    // затем разворачиваем в хронологический порядок для отображения.
    q.prepare(R"(
        SELECT * FROM (
            SELECT * FROM messages WHERE peer_uuid=:uuid
            ORDER BY id DESC
            LIMIT :lim OFFSET :off
        ) ORDER BY id ASC
    )");
    q.bindValue(":uuid", peerUuid.toString(QUuid::WithoutBraces));
    q.bindValue(":lim",  limit);
    q.bindValue(":off",  offset);
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
