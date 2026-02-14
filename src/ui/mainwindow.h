#pragma once
#include <QMainWindow>
#include <QUuid>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include "../core/network.h"
#include "../core/storage.h"
#include "../core/filetransfer.h"
#include "../crypto/e2e.h"
#include "thememanager.h"
#include "../core/updatechecker.h"

namespace Ui { class MainWindow; }

class ContactsWidget;
class ChatWidget;
class SettingsPanel;
class UpdateBanner;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
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

private:
    void setupUi();
    void applyTheme();
    void updateStatusBar(const QString& ip, quint16 port, bool upnp);

    Ui::MainWindow*  ui          {nullptr};
    QStackedWidget*  m_leftStack    {nullptr};
    ContactsWidget*  m_contacts     {nullptr};
    ChatWidget*      m_chat         {nullptr};
    SettingsPanel*   m_settings     {nullptr};
    UpdateBanner*    m_updateBanner {nullptr};
    QLabel*          m_nameLabel    {nullptr};

    NetworkManager*  m_network     {nullptr};
    StorageManager*  m_storage     {nullptr};
    E2EManager*      m_e2e         {nullptr};
    FileTransfer*    m_fileTransfer {nullptr};

    QUuid            m_activePeer;
    QString          m_lastIp;
    quint16          m_lastPort {0};
    bool             m_lastUpnp {false};
};
