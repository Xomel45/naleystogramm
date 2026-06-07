#pragma once
#include <string>
#include <optional>
#include <cstdint>

struct PeerInfo {
    std::string name;
    std::string uuid;
    std::string ip;
    uint16_t    port{0};

    [[nodiscard]] bool operator==(const PeerInfo& o) const noexcept {
        return uuid == o.uuid;
    }
};

class Identity {
public:
    static Identity& instance();

    // Load from disk or generate on first launch
    void load();
    void save() const;

    [[nodiscard]] std::string uuid()        const { return m_uuid; }
    [[nodiscard]] std::string displayName() const { return m_name; }
    void setDisplayName(const std::string& name);

    // "UUID@IP:Port" — share this with contacts
    [[nodiscard]] std::string connectionString(const std::string& externalIp, uint16_t port) const;

    // Parse a connection string received from a contact
    static std::optional<PeerInfo> parseConnectionString(const std::string& str);

private:
    Identity() = default;
    std::string m_filePath;
    std::string m_uuid;
    std::string m_name;
};
