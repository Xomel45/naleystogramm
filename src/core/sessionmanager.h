#pragma once
#include <QObject>
#include <QString>
#include <QUuid>
#include <QJsonObject>
#include <QByteArray>

// Режим проброса портов — определяет, как приложение рекламирует свой адрес пирам
enum class PortForwardingMode : int {
    UpnpAuto = 0,  // UPnP (автоматический, по умолчанию)
    Manual   = 1,  // Ручной (VPN / статический IP + внешний порт)
    Disabled = 2,  // Отключено (только локальная сеть, без проброса)
};

// ── SessionManager ─────────────────────────────────────────────────────────
// Единое хранилище всех пользовательских настроек в session.json
//
// Путь:
//   Windows : %LOCALAPPDATA%\naleystogramm\session.json
//   Linux   : ~/.cache/naleystogramm/session.json
//   macOS   : ~/Library/Caches/naleystogramm/session.json
//
// Формат файла:
// {
//   "identity": {
//     "uuid": "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx",
//     "name": "Xomel"
//   },
//   "network": {
//     "port": 47821,
//     "bindIp": ""
//   },
//   "ui": {
//     "theme": "dark",
//     "language": "ru",
//     "demoMode": false
//   },
//   "updates": {
//     "lastChecked": "2026-02-14T12:00:00"
//   },
//   "meta": {
//     "version": "0.1.2",
//     "savedAt": "2026-02-14T12:00:00"
//   }
// }

class SessionManager : public QObject {
    Q_OBJECT
public:
    static SessionManager& instance();

    // Загружает session.json, создаёт если нет
    void load();

    // Сохраняет всё в session.json
    void save();

    // Путь до файла (для отображения в UI)
    [[nodiscard]] QString filePath() const { return m_filePath; }

    // ── Identity ──────────────────────────────────────────────────────────
    [[nodiscard]] QUuid   uuid()        const { return m_uuid; }
    [[nodiscard]] QString displayName() const { return m_displayName; }
    void setUuid(const QUuid& uuid);
    void setDisplayName(const QString& name);

    // ── Network ───────────────────────────────────────────────────────────
    [[nodiscard]] quint16 port()        const { return m_port; }
    [[nodiscard]] QString bindIp()      const { return m_bindIp; }
    void setPort(quint16 port);
    void setBindIp(const QString& ip);

    // ── UI ────────────────────────────────────────────────────────────────
    [[nodiscard]] QString theme()          const { return m_theme; }
    [[nodiscard]] QString language()       const { return m_language; }
    [[nodiscard]] bool    demoMode()       const { return m_demoMode; }
    [[nodiscard]] int     leftPanelWidth() const { return m_leftPanelWidth; }
    void setTheme(const QString& theme);
    void setLanguage(const QString& lang);
    void setDemoMode(bool on);
    void setLeftPanelWidth(int w);

    // ── Updates ───────────────────────────────────────────────────────────
    [[nodiscard]] QString lastUpdateCheck() const { return m_lastUpdateCheck; }
    void setLastUpdateCheck(const QString& iso);

    // ── Port Forwarding ───────────────────────────────────────────────────
    [[nodiscard]] PortForwardingMode portForwardingMode() const { return m_portForwardingMode; }
    [[nodiscard]] QString            manualPublicIp()     const { return m_manualPublicIp; }
    [[nodiscard]] quint16            manualPublicPort()   const { return m_manualPublicPort; }
    void setPortForwardingMode(PortForwardingMode mode);
    void setManualPublicIp(const QString& ip);
    void setManualPublicPort(quint16 port);

    // ── Security ──────────────────────────────────────────────────────────
    // Разрешить входящие запросы удалённого шелла (по умолчанию включено)
    [[nodiscard]] bool remoteShellEnabled() const { return m_remoteShellEnabled; }
    void setRemoteShellEnabled(bool on);

    // ── Avatar ────────────────────────────────────────────────────────────
    [[nodiscard]] QString avatarPath() const { return m_avatarPath; }
    void setAvatarPath(const QString& path);   // автосохранение

    // SHA-256 hex файла; пустая строка если файл не читается
    [[nodiscard]] static QByteArray computeAvatarHash(const QString& filePath);

    // Создаёт все необходимые директории для хранения данных приложения.
    // Вызывается однократно при старте, до инициализации Storage и Logger.
    static void ensureDirectories();

signals:
    void loaded();
    void saved();

private:
    explicit SessionManager(QObject* parent = nullptr);

    void initFilePath();
    [[nodiscard]] QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
    void generateIdentityIfNeeded();

    QString m_filePath;

    // Identity
    QUuid   m_uuid;
    QString m_displayName {"User"};

    // Network
    quint16 m_port   {47821};
    QString m_bindIp {};

    // UI
    QString m_theme          {"dark"};
    QString m_language       {"ru"};
    bool    m_demoMode       {false};
    int     m_leftPanelWidth {280};

    // Updates
    QString m_lastUpdateCheck {};

    // Port Forwarding
    PortForwardingMode m_portForwardingMode {PortForwardingMode::UpnpAuto};
    QString            m_manualPublicIp     {};
    quint16            m_manualPublicPort   {47821};

    // Security
    bool    m_remoteShellEnabled {true};

    // Avatar
    QString m_avatarPath {};
};
