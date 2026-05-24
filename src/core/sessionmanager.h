#pragma once
#include "device_pairing.h"
#include <QObject>
#include <optional>
#include <QString>
#include <QUuid>
#include <QJsonObject>
#include <QByteArray>

class QTimer;

// Уровень конфиденциальности — кто может совершать то или иное действие
enum class PrivacyLevel : int {
    Everyone     = 0,  // Все (любой пир)
    ContactsOnly = 1,  // Только контакты (UUID есть в списке контактов)
    Nobody       = 2,  // Никто (полный запрет)
};

// Режим проброса портов — определяет, как приложение рекламирует свой адрес пирам
enum class PortForwardingMode : int {
    UpnpAuto     = 0,  // UPnP (автоматический, по умолчанию)
    Manual       = 1,  // Ручной (VPN / статический IP + внешний порт)
    Disabled     = 2,  // Отключено (только локальная сеть, без проброса)
    ClientServer = 3,  // Ретрансляция через выделенный сервер (TCP+UDP relay)
    OpenPort     = 4,  // Разблокированный порт (ручной проброс, IP авто-discovery)
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
    [[nodiscard]] QString bio()         const { return m_bio; }
    void setUuid(const QUuid& uuid);
    void setDisplayName(const QString& name);
    void setBio(const QString& b);

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
    [[nodiscard]] bool    enterSends()      const { return m_enterSends; }
    void setEnterSends(bool on);

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

    // ── Relay (Client-Server) ─────────────────────────────────────────────
    [[nodiscard]] QString relayServerIp()  const { return m_relayServerIp; }
    [[nodiscard]] quint16 relayTcpPort()   const { return m_relayTcpPort; }
    [[nodiscard]] quint16 relayUdpPort()   const { return m_relayUdpPort; }
    void setRelayServerIp(const QString& ip);
    void setRelayTcpPort(quint16 port);
    void setRelayUdpPort(quint16 port);

    // ── Security ──────────────────────────────────────────────────────────
    // Разрешить входящие запросы удалённого шелла (по умолчанию включено)
    [[nodiscard]] bool remoteShellEnabled() const { return m_remoteShellEnabled; }
    void setRemoteShellEnabled(bool on);

    // ── Privacy ───────────────────────────────────────────────────────────
    [[nodiscard]] PrivacyLevel privacyMessages() const { return m_privacyMessages; }
    [[nodiscard]] PrivacyLevel privacyFiles()    const { return m_privacyFiles;    }
    [[nodiscard]] PrivacyLevel privacyCalls()    const { return m_privacyCalls;    }
    [[nodiscard]] PrivacyLevel privacyVoice()    const { return m_privacyVoice;    }
    [[nodiscard]] PrivacyLevel privacyAvatar()   const { return m_privacyAvatar;   }
    [[nodiscard]] PrivacyLevel privacyShell()    const { return m_privacyShell;    }
    void setPrivacyMessages(PrivacyLevel v);
    void setPrivacyFiles   (PrivacyLevel v);
    void setPrivacyCalls   (PrivacyLevel v);
    void setPrivacyVoice   (PrivacyLevel v);
    void setPrivacyAvatar  (PrivacyLevel v);
    void setPrivacyShell   (PrivacyLevel v);

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
    void scheduleSave();

    QString m_filePath;
    QTimer* m_saveTimer {nullptr};

    // Identity
    QUuid   m_uuid;
    QString m_displayName {"User"};
    QString m_bio {};

    // Network
    quint16 m_port   {47821};
    QString m_bindIp {};

    // UI
    QString m_theme          {"dark"};
    QString m_language       {"ru"};
    bool    m_demoMode       {false};
    int     m_leftPanelWidth {320};
    bool    m_enterSends     {true};

    // Updates
    QString m_lastUpdateCheck {};

    // Port Forwarding
    PortForwardingMode m_portForwardingMode {PortForwardingMode::UpnpAuto};
    QString            m_manualPublicIp     {};
    quint16            m_manualPublicPort   {47821};

    // Relay (Client-Server)
    QString m_relayServerIp  {};
    quint16 m_relayTcpPort   {47822};
    quint16 m_relayUdpPort   {47823};

    // Security
    bool    m_remoteShellEnabled {false};

    // Privacy
    PrivacyLevel m_privacyMessages {PrivacyLevel::Everyone};
    PrivacyLevel m_privacyFiles    {PrivacyLevel::Everyone};
    PrivacyLevel m_privacyCalls    {PrivacyLevel::Everyone};
    PrivacyLevel m_privacyVoice    {PrivacyLevel::Everyone};
    PrivacyLevel m_privacyAvatar   {PrivacyLevel::Everyone};
    PrivacyLevel m_privacyShell    {PrivacyLevel::ContactsOnly};

    // Avatar
    QString m_avatarPath {};

    // Linked devices (multi-device)
    QList<LinkedDevice> m_linkedDevices {};

public:
    [[nodiscard]] QList<LinkedDevice> linkedDevices() const { return m_linkedDevices; }
    void addLinkedDevice(const LinkedDevice& dev);
    void removeLinkedDevice(const QUuid& uuid);
    [[nodiscard]] bool isLinkedDevice(const QUuid& uuid) const;
    [[nodiscard]] std::optional<LinkedDevice> linkedDevice(const QUuid& uuid) const;
};
