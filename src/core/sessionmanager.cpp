#include "sessionmanager.h"
#include "../ui/thememanager.h"  // для Theme enum если нужен
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QUuid>
#include <QDebug>

static constexpr const char* kAppDirName  = "naleystogramm";
static constexpr const char* kFileName    = "session.json";
static constexpr const char* kVersion     = "0.1.0";

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
    qDebug("[Session] Path: %s", qPrintable(m_filePath));
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
    qDebug("[Session] Loaded: name=%s uuid=%s",
           qPrintable(m_displayName),
           qPrintable(m_uuid.toString(QUuid::WithoutBraces)));
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
    qDebug("[Session] Saved to %s", qPrintable(m_filePath));
}

// ── JSON сериализация ─────────────────────────────────────────────────────

QJsonObject SessionManager::toJson() const {
    QJsonObject identity;
    identity["uuid"] = m_uuid.toString(QUuid::WithoutBraces);
    identity["name"] = m_displayName;

    QJsonObject network;
    network["port"]   = static_cast<int>(m_port);
    network["bindIp"] = m_bindIp;

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

    QJsonObject root;
    root["identity"] = identity;
    root["network"]  = network;
    root["ui"]       = ui;
    root["updates"]  = updates;
    root["meta"]     = meta;

    return root;
}

void SessionManager::fromJson(const QJsonObject& obj) {
    // Identity
    const auto id = obj["identity"].toObject();
    const QString uuidStr = id["uuid"].toString();
    m_uuid = uuidStr.isEmpty() ? QUuid() : QUuid(uuidStr);
    m_displayName = id["name"].toString("User");

    // Network
    const auto net = obj["network"].toObject();
    m_port   = static_cast<quint16>(net["port"].toInt(47821));
    m_bindIp = net["bindIp"].toString();

    // UI
    const auto ui = obj["ui"].toObject();
    m_theme          = ui["theme"].toString("dark");
    m_language       = ui["language"].toString("ru");
    m_demoMode       = ui["demoMode"].toBool(false);
    m_leftPanelWidth = ui["leftPanelWidth"].toInt(280);

    // Updates
    const auto upd = obj["updates"].toObject();
    m_lastUpdateCheck = upd["lastChecked"].toString();
}

void SessionManager::generateIdentityIfNeeded() {
    if (m_uuid.isNull()) {
        m_uuid = QUuid::createUuid();
        qDebug("[Session] Generated new UUID: %s",
               qPrintable(m_uuid.toString(QUuid::WithoutBraces)));
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
