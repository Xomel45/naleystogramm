#include "core_bridge.h"

#include "core/net/network.h"
#include "core/storage/sessionmanager.h"
#include "core/identity/identity.h"

namespace glui {

namespace {
constexpr size_t kMaxLogLines = 200;
}

CoreBridge::CoreBridge() : m_network(std::make_unique<NetworkManager>()) {
    SessionManager::ensureDirectories();
    SessionManager::instance().load();
    Identity::instance().load();

    NetworkEvent ev;
    ev.onReady = [this](const std::string& ip, uint16_t port, bool upnpOk) {
        pushLog("[network] ready: " + ip + ":" + std::to_string(port) +
                (upnpOk ? " (upnp ok)" : " (upnp failed)"));
    };
    ev.onExternalIp = [this](const std::string& ip) {
        pushLog("[network] external ip: " + ip);
    };
    ev.onPeerConnected = [this](const std::string& uuid, const std::string& name) {
        pushLog("[network] peer connected: " + name + " (" + uuid + ")");
    };
    ev.onPeerDisconnected = [this](const std::string& uuid) {
        pushLog("[network] peer disconnected: " + uuid);
    };
    ev.onLog = [this](const std::string& msg) {
        pushLog("[log] " + msg);
    };
    ev.onError = [this](const std::string& msg) {
        pushLog("[error] " + msg);
    };

    m_network->addListener(ev);
    m_network->init();

    pushLog("CoreBridge: NetworkManager запущен (Qt-free)");
}

CoreBridge::~CoreBridge() = default;

void CoreBridge::pushLog(const std::string& line) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logs.push_back(line);
    while (m_logs.size() > kMaxLogLines)
        m_logs.pop_front();
}

std::vector<std::string> CoreBridge::drainLogs() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> out(m_logs.begin(), m_logs.end());
    m_logs.clear();
    return out;
}

} // namespace glui
