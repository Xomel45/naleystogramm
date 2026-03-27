#include "sessionmanager.h"
#include "../ui/thememanager.h"  // для Theme enum если нужен
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QUuid>
#include <QCryptographicHash>
#include <QDebug>

static constexpr const char* kAppDirName  = "naleystogramm";
static constexpr const char* kFileName    = "session.json";
static constexpr const char* kVersion     = "0.5.3";

// ── Singleton ─────────────────────────────────────────────────────────────

SessionManager& SessionManager::instance() {
    static SessionManager inst;
    return inst;
}

SessionManager::SessionManager(QObject* parent) : QObject(parent) {
    initFilePath();
}

// ── Путь к файлу ──────────────────────────────────────────────────────────

void SessionManager::initFilePath() {
#ifdef Q_OS_WIN
    // Windows: %LOCALAPPDATA%\naleystogramm\session.json
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
#else
    // Linux/macOS: ~/.cache/naleystogramm/session.json
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation);
#endif

    // Qt добавляет org/app в путь — берём родительскую папку и добавляем наш dir
    // Чтобы путь был именно .cache/naleystogramm а не .cache/naleystogramm/naleystogramm
    QDir dir(base);
    dir.cdUp();                          // выходим из naleystogramm
    dir.mkpath(kAppDirName);             // создаём naleystogramm/
    dir.cd(kAppDirName);

    m_filePath = dir.filePath(kFileName);
    qDebug("[Session] Config File Located");
}

// ── Load ──────────────────────────────────────────────────────────────────

void SessionManager::load() {
    QFile f(m_filePath);

    if (!f.exists()) {
        qDebug("[Session] No session.json — creating new");
        generateIdentityIfNeeded();
        save();  // создаём файл с дефолтами
        emit loaded();
        return;
    }

    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("[Session] Cannot open %s: %s",
                 qPrintable(m_filePath), qPrintable(f.errorString()));
        generateIdentityIfNeeded();
        emit loaded();
        return;
    }

    const auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    if (doc.isNull() || !doc.isObject()) {
        qWarning("[Session] session.json is malformed — resetting");
        generateIdentityIfNeeded();
        save();
        emit loaded();
        return;
    }

    fromJson(doc.object());
    generateIdentityIfNeeded();  // на случай если uuid пустой
    emit loaded();
    qDebug("[Session] Session Loaded");
}

// ── Save ──────────────────────────────────────────────────────────────────

void SessionManager::save() {
    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("[Session] Cannot write %s: %s",
                 qPrintable(m_filePath), qPrintable(f.errorString()));
        return;
    }

    const QJsonDocument doc(toJson());
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    emit saved();
    qDebug("[Session] Session Saved");
}

// ── JSON сериализация ─────────────────────────────────────────────────────

QJsonObject SessionManager::toJson() const {
    QJsonObject identity;
    identity["uuid"]       = m_uuid.toString(QUuid::WithoutBraces);
    identity["name"]       = m_displayName;
    identity["avatarPath"] = m_avatarPath;

    QJsonObject network;
    network["port"]               = static_cast<int>(m_port);
    network["bindIp"]             = m_bindIp;
    network["portForwardingMode"] = static_cast<int>(m_portForwardingMode);
    network["manualPublicIp"]     = m_manualPublicIp;
    network["manualPublicPort"]   = static_cast<int>(m_manualPublicPort);

    QJsonObject ui;
    ui["theme"]          = m_theme;
    ui["language"]       = m_language;
    ui["demoMode"]       = m_demoMode;
    ui["leftPanelWidth"] = m_leftPanelWidth;

    QJsonObject updates;
    updates["lastChecked"] = m_lastUpdateCheck;

    QJsonObject meta;
    meta["version"] = kVersion;
    meta["savedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonObject security;
    security["remoteShell"] = m_remoteShellEnabled;

    QJsonObject root;
    root["identity"] = identity;
    root["network"]  = network;
    root["ui"]       = ui;
    root["updates"]  = updates;
    root["security"] = security;
    root["meta"]     = meta;

    return root;
}

void SessionManager::fromJson(const QJsonObject& obj) {
    // Identity
    const auto id = obj["identity"].toObject();
    const QString uuidStr = id["uuid"].toString();
    m_uuid        = uuidStr.isEmpty() ? QUuid() : QUuid(uuidStr);
    m_displayName = id["name"].toString("User");
    m_avatarPath  = id["avatarPath"].toString();

    // Network
    const auto net = obj["network"].toObject();
    m_port               = static_cast<quint16>(net["port"].toInt(47821));
    m_bindIp             = net["bindIp"].toString();
    m_portForwardingMode = static_cast<PortForwardingMode>(
        net["portForwardingMode"].toInt(static_cast<int>(PortForwardingMode::UpnpAuto)));
    m_manualPublicIp     = net["manualPublicIp"].toString();
    m_manualPublicPort   = static_cast<quint16>(net["manualPublicPort"].toInt(47821));

    // UI
    const auto ui = obj["ui"].toObject();
    m_theme          = ui["theme"].toString("dark");
    m_language       = ui["language"].toString("ru");
    m_demoMode       = ui["demoMode"].toBool(false);
    m_leftPanelWidth = ui["leftPanelWidth"].toInt(280);

    // Updates
    const auto upd = obj["updates"].toObject();
    m_lastUpdateCheck = upd["lastChecked"].toString();

    // Security
    const auto sec = obj["security"].toObject();
    m_remoteShellEnabled = sec["remoteShell"].toBool(true);
}

void SessionManager::generateIdentityIfNeeded() {
    if (m_uuid.isNull()) {
        m_uuid = QUuid::createUuid();
        qDebug("[Session] Identity Generated");
    }
    if (m_displayName.isEmpty())
        m_displayName = "User";
}

// ── Setters (автосохранение) ──────────────────────────────────────────────

void SessionManager::setUuid(const QUuid& uuid)         { m_uuid = uuid;         save(); }
void SessionManager::setDisplayName(const QString& name) { m_displayName = name; save(); }
void SessionManager::setPort(quint16 port)               { m_port = port;         save(); }
void SessionManager::setBindIp(const QString& ip)        { m_bindIp = ip;         save(); }
void SessionManager::setTheme(const QString& theme)      { m_theme = theme;       save(); }
void SessionManager::setLanguage(const QString& lang)    { m_language = lang;     save(); }
void SessionManager::setDemoMode(bool on)                { m_demoMode = on;       save(); }
void SessionManager::setLeftPanelWidth(int w)            { m_leftPanelWidth = w;  save(); }
void SessionManager::setLastUpdateCheck(const QString& iso) {
    m_lastUpdateCheck = iso;
    save();
}

void SessionManager::setPortForwardingMode(PortForwardingMode mode) {
    m_portForwardingMode = mode;
    save();
}

void SessionManager::setManualPublicIp(const QString& ip) {
    m_manualPublicIp = ip;
    save();
}

void SessionManager::setManualPublicPort(quint16 port) {
    m_manualPublicPort = port;
    save();
}

void SessionManager::setRemoteShellEnabled(bool on) {
    m_remoteShellEnabled = on;
    save();
}

void SessionManager::setAvatarPath(const QString& path) {
    m_avatarPath = path;
    save();
}

QByteArray SessionManager::computeAvatarHash(const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex();
}

// ── Создание директорий ────────────────────────────────────────────────────
// Гарантирует существование всех папок до инициализации подсистем (Storage, Logger).
// Вызывается один раз при старте из main.cpp сразу после загрузки сессии.

void SessionManager::ensureDirectories() {
    // Список пар: описание → абсолютный путь
    const struct { const char* label; QString path; } dirs[] = {
        { "Config",  QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                     + "/naleystogramm" },
        { "Avatars", QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                     + "/avatars" },
        { "Logs",    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                     + "/logs" },
        { "Keys",    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                     + "/keys" },
    };

    for (const auto& d : dirs) {
        if (QDir().mkpath(d.path)) {
            qDebug("[Session] Directory Ready: %s", d.label);
        } else {
            qWarning("[Session] Directory Creation Failed: %s", d.label);
        }
    }
}
