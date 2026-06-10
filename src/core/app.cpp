#include "app.h"
#include "storage.h"
#include "network.h"
#include "filetransfer.h"
#include "callmanager.h"
#include "remoteshellmanager.h"
#include "identity.h"
#include "../crypto/e2e.h"
#include "../crypto/keyprotector.h"
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#ifdef HAVE_CURL
#  include <curl/curl.h>
#endif

static std::filesystem::path appDataDir() {
#ifdef _WIN32
    const char* local = std::getenv("LOCALAPPDATA");
    return std::filesystem::path(local ? local : "C:\\ProgramData") / "naleystogramm";
#else
    const char* xdg  = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    std::string base = (xdg && *xdg) ? xdg
        : (std::string(home ? home : "/tmp") + "/.local/share");
    return std::filesystem::path(base) / "naleystogramm";
#endif
}

App::App(QObject* parent) : QObject(parent) {
#ifdef HAVE_CURL
    curl_global_init(CURL_GLOBAL_ALL);
#endif

    auto& id = Identity::instance();
    id.load();

    const std::filesystem::path dataDir = appDataDir();
    std::error_code ec;
    std::filesystem::create_directories(dataDir, ec);

    if (!KeyProtector::instance().init(dataDir))
        fprintf(stderr, "[App] KeyProtector не инициализирован — данные будут незащищены\n");

    m_storage = new StorageManager();
    m_storage->open();

    m_e2e = std::make_unique<E2EManager>();
    m_e2e->init(id.uuid(), dataDir);

    m_network      = new NetworkManager();
    m_fileTransfer = std::make_unique<FileTransfer>(m_network, m_e2e.get(), dataDir);
    m_callManager  = std::make_unique<CallManager>(m_network, m_e2e.get());
    m_shellManager = std::make_unique<RemoteShellManager>(m_network, m_e2e.get());
}

App::~App() {
#ifdef HAVE_CURL
    curl_global_cleanup();
#endif
}

StorageManager&     App::storage()     { return *m_storage; }
E2EManager&         App::e2e()         { return *m_e2e; }
NetworkManager&     App::network()     { return *m_network; }
FileTransfer&       App::fileTransfer(){ return *m_fileTransfer; }
CallManager&        App::callManager() { return *m_callManager; }
RemoteShellManager& App::shellManager(){ return *m_shellManager; }
