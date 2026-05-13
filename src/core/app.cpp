#include "app.h"
#include "storage.h"
#include "network.h"
#include "filetransfer.h"
#include "callmanager.h"
#include "remoteshellmanager.h"
#include "identity.h"
#include "../crypto/e2e.h"
#include "../crypto/keyprotector.h"
#include <QDebug>

App::App(QObject* parent) : QObject(parent) {
    auto& id = Identity::instance();
    id.load();

    if (!KeyProtector::instance().init())
        qCritical("[App] KeyProtector не инициализирован — данные будут незащищены");

    m_storage = new StorageManager(this);
    m_storage->open();

    m_e2e = new E2EManager(this);
    m_e2e->init(id.uuid());

    m_network      = new NetworkManager(this);
    m_fileTransfer = new FileTransfer(m_network, m_e2e, this);
    m_callManager  = new CallManager(m_network, m_e2e, this);
    m_shellManager = new RemoteShellManager(m_network, m_e2e, this);
}

App::~App() = default;

StorageManager&     App::storage()     { return *m_storage; }
E2EManager&         App::e2e()         { return *m_e2e; }
NetworkManager&     App::network()     { return *m_network; }
FileTransfer&       App::fileTransfer(){ return *m_fileTransfer; }
CallManager&        App::callManager() { return *m_callManager; }
RemoteShellManager& App::shellManager(){ return *m_shellManager; }
