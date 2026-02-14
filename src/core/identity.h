#pragma once
#include <QString>
#include <QUuid>
#include <optional>

// C++20: поддержка designated initializers:
//   PeerInfo{ .name = "Bob", .uuid = ..., .ip = "1.2.3.4", .port = 47821 }
struct PeerInfo {
    QString name;
    QUuid   uuid;
    QString ip;
    quint16 port{0};

    [[nodiscard]] bool operator==(const PeerInfo& o) const noexcept {
        return uuid == o.uuid;
    }
};

class Identity {
public:
    static Identity& instance();

    // Load from disk or generate on first launch
    void        load();
    void        save() const;

    QUuid       uuid()        const { return m_uuid; }
    QString     displayName() const { return m_name; }
    void        setDisplayName(const QString& name);

    // "Name@UUID@IP:Port" — share this with contacts
    QString     connectionString(const QString& externalIp, quint16 port) const;

    // Parse a connection string received from a contact
    // Returns nullopt if format is invalid
    static std::optional<PeerInfo> parseConnectionString(const QString& str);

private:
    Identity() = default;
    QString m_filePath;
    QUuid   m_uuid;
    QString m_name;
};
