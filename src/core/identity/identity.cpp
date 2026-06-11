#include "identity.h"
#include "../storage/sessionmanager.h"
#include <chrono>
#include <cctype>
#include <optional>

Identity& Identity::instance() {
    static Identity inst;
    return inst;
}

void Identity::load() {
    auto& sm = SessionManager::instance();
    m_uuid = sm.uuid();
    m_name = sm.displayName();

    if (m_name == "User") {
        const auto epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        m_name = "User-" + std::to_string(epochMs % 10000);
        sm.setDisplayName(m_name);
    }
}

void Identity::save() const {
    auto& sm = SessionManager::instance();
    sm.setUuid(m_uuid);
    sm.setDisplayName(m_name);
}

void Identity::setDisplayName(const std::string& name) {
    const auto start = name.find_first_not_of(" \t\r\n");
    const auto end   = name.find_last_not_of (" \t\r\n");
    m_name = (start == std::string::npos) ? std::string{} : name.substr(start, end - start + 1);
    SessionManager::instance().setDisplayName(m_name);
}

std::string Identity::connectionString(const std::string& ip, uint16_t port) const {
    return m_uuid + "@" + ip + ":" + std::to_string(port);
}

bool Identity::isValidUuid(const std::string& uuid) {
    // "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" — 36 chars, hyphens at 8,13,18,23
    if (uuid.size() != 36) return false;
    static constexpr int kHyphenPos[] = {8, 13, 18, 23};
    for (const int pos : kHyphenPos)
        if (uuid[pos] != '-') return false;
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) continue;
        if (!std::isxdigit(static_cast<unsigned char>(uuid[i]))) return false;
    }
    return true;
}

std::optional<PeerInfo> Identity::parseConnectionString(const std::string& str) {
    // format: "UUID@IP:Port"
    const auto atPos = str.find('@');
    if (atPos == std::string::npos) return std::nullopt;

    const std::string uuid   = str.substr(0, atPos);
    const std::string ipPort = str.substr(atPos + 1);

    if (!isValidUuid(uuid)) return std::nullopt;

    const auto colonPos = ipPort.rfind(':');
    if (colonPos == std::string::npos) return std::nullopt;

    const std::string ip      = ipPort.substr(0, colonPos);
    const std::string portStr = ipPort.substr(colonPos + 1);

    if (ip.empty() || portStr.empty()) return std::nullopt;

    uint16_t port = 0;
    try {
        const int p = std::stoi(portStr);
        if (p <= 0 || p > 65535) return std::nullopt;
        port = static_cast<uint16_t>(p);
    } catch (...) {
        return std::nullopt;
    }

    // Name is intentionally empty — filled automatically via HANDSHAKE after connect
    return PeerInfo{ .name = {}, .uuid = uuid, .ip = ip, .port = port };
}
