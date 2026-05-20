#pragma once
#include <QMainWindow>
#include <QUuid>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QSystemTrayIcon>
#include "../core/sessionmanager.h"  // нужен PrivacyLevel в сигнатуре checkPrivacy

namespace Ui { class MainWindow; }

class App;
class NetworkManager;
class StorageManager;
class E2EManager;
class FileTransfer;
class CallManager;
class RemoteShellManager;
class CallWindow;
class ContactsWidget;
class ChatWidget;
class SettingsPanel;
class UpdateBanner;
class ContactProfileDialog;
class ShellWindow;
class ShellMonitor;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(App& app, QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onAppReady(const QString& ip, quint16 port, bool upnp);
    void onIncomingRequest(QUuid uuid, QString name, QString ip);
    void onPeerConnected(QUuid uuid, QString name);
    void onPeerDisconnected(QUuid uuid);
    void onMessageReceived(QUuid from, QJsonObject msg);
    void onSessionEstablished(QUuid peerUuid);

    void onAddContactClicked();
    void onContactSelected(QUuid uuid);
    void onSendMessage(const QString& text);
    void onSendFile();
    void onShowMyId();
    void onEditName();
    void onCycleTheme();

    void openSettings();   // переключить левую панель на настройки
    void closeSettings();  // переключить обратно на чаты
    void refreshOwnDisplay();

    // Открыть диалог профиля контакта (избегаем дублирования)
    void onOpenProfile(QUuid uuid);
    // Принять и сохранить аватар от пира
    void onAvatarDataReceived(QUuid from, const QByteArray& pngData, const QString& hash);
    // Обработать смену имени пира (из PROFILE_UPDATE или HANDSHAKE)
    void onContactNameUpdated(QUuid uuid, QString name);
    // Заблокировать/разблокировать контакт (переключает состояние)
    void onBlockContact(QUuid uuid);
    // Удалить переписку с контактом (без удаления самого контакта)
    void onDeleteChat(QUuid uuid);
    // Полностью удалить контакт и всю переписку (с сохранением блокировки в blocked_list)
    void onDeleteContact(QUuid uuid);

    // Отправить голосовое сообщение активному пиру
    void onSendVoice(const QString& filePath, int durationMs);

    // ── Голосовые звонки ─────────────────────────────────────────────────────
    void onCallRequested(QUuid peerUuid);
    void onIncomingCall(QUuid from, QString callerName, QString callId);
    void onCallEnded(QUuid peer);

    // ── Удалённый шелл ───────────────────────────────────────────────────────
    void onShellRequestedFromProfile(QUuid peerUuid);
    // Receiver: входящий запрос — показываем OTP
    void onShellChallengeGenerated(QString sessionId, QUuid from,
                                   QString peerName, QString otp);
    // Receiver: шелл-процесс запущен (OTP-диалог закрывается, монитор показывается)
    void onShellSessionStarted(QString sessionId);
    // Initiator: нужно ввести OTP, который виден у получателя
    void onShellPasswordRequired(QString sessionId, QUuid peerUuid, QString peerName);
    void onShellAccepted(QString sessionId, QUuid peerUuid, QString peerName);
    void onShellRejected(QString sessionId, QString reason);
    void onShellDataReceived(QString sessionId, QByteArray data);
    void onInputMonitored(QString sessionId, QByteArray data);
    void onShellSessionEnded(QString sessionId, QString reason);
    void onPrivilegeEscalationDetected(QString sessionId);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUi();
    void applyTheme();
    void updateStatusBar(const QString& ip, quint16 port, bool upnp);
    void loadOwnAvatar();   // загружает/обновляет аватар в шапке левой панели

    // Проверяет уровень конфиденциальности — true если действие разрешено
    [[nodiscard]] bool checkPrivacy(PrivacyLevel level, const QUuid& from) const;
    [[nodiscard]] bool isKnownContact(const QUuid& uuid) const;

    Ui::MainWindow*  ui          {nullptr};
    QStackedWidget*  m_leftStack    {nullptr};
    ContactsWidget*  m_contacts     {nullptr};
    ChatWidget*      m_chat         {nullptr};
    SettingsPanel*   m_settings     {nullptr};
    UpdateBanner*    m_updateBanner {nullptr};
    QLabel*          m_nameLabel    {nullptr};
    QLabel*          m_myAvatar     {nullptr};

    StorageManager*       m_storage      {nullptr};
    E2EManager*           m_e2e          {nullptr};
    NetworkManager*       m_network      {nullptr};
    FileTransfer*         m_fileTransfer {nullptr};
    CallManager*          m_callManager  {nullptr};
    CallWindow*           m_callWindow   {nullptr};  // nullptr когда нет активного звонка
    RemoteShellManager*   m_shellManager {nullptr};

    // sessionId → окно инициатора / монитор получателя
    QHash<QString, ShellWindow*>  m_shellWindows;
    QHash<QString, ShellMonitor*> m_shellMonitors;

    QUuid            m_activePeer;
    QString          m_lastIp;
    quint16          m_lastPort {0};
    bool             m_lastUpnp {false};

    // Отслеживание входящих голосовых передач: offerId → дополнительные данные
    QMap<QString, int>   m_pendingVoiceDurations;   // offerId → durationMs (0 = не голосовое)
    QMap<QString, QUuid> m_pendingTransferSenders;  // offerId → UUID отправителя

    // UUID контактов, для которых уже показано предупреждение о несовместимости версии
    // (показываем не чаще одного раза за сессию для каждого UUID)
    QSet<QUuid> m_shownCompatWarnings;

    // Кнопка статуса UPnP в статус-баре (постоянный виджет)
    QPushButton*     m_upnpBtn     {nullptr};
    // Индикатор открытого порта (режим OpenPort)
    QPushButton*     m_openPortBtn {nullptr};

    // System tray
    QSystemTrayIcon* m_tray {nullptr};

    // Ожидание ACK: msgId → UUID пира (для markDelivered)
    QMap<QString, QUuid> m_pendingAcks;

    // Таймеры авто-скрытия typing indicator (per peer)
    QMap<QUuid, QTimer*> m_peerTypingTimers;
};
