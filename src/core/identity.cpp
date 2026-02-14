#include "identity.h"
#include "sessionmanager.h"
#include <QDateTime>
#include <optional>

Identity& Identity::instance() {
    static Identity inst;
    return inst;
}

void Identity::load() {
    // SessionManager уже загружен в main() до создания окна.
    // Просто берём данные оттуда.
    auto& sm = SessionManager::instance();
    m_uuid = sm.uuid();
    m_name = sm.displayName();

    // На случай первого запуска (SessionManager уже сгенерировал uuid)
    if (m_name == "User") {
        m_name = QString("User-%1")
            .arg(QString::number(QDateTime::currentMSecsSinceEpoch()).right(4));
        sm.setDisplayName(m_name);
    }
}

void Identity::save() const {
    auto& sm = SessionManager::instance();
    sm.setUuid(m_uuid);
    sm.setDisplayName(m_name);
}

void Identity::setDisplayName(const QString& name) {
    m_name = name.trimmed();
    SessionManager::instance().setDisplayName(m_name);
}

QString Identity::connectionString(const QString& ip, quint16 port) const {
    return QString("%1@%2@%3:%4")
        .arg(m_name)
        .arg(m_uuid.toString(QUuid::WithoutBraces))
        .arg(ip)
        .arg(port);
}

std::optional<PeerInfo> Identity::parseConnectionString(const QString& str) {
    const QStringList parts = str.trimmed().split('@');
    if (parts.size() != 3) return std::nullopt;

    const QString name    = parts[0].trimmed();
    const QUuid   uuid    = QUuid(parts[1].trimmed());
    const QString ipPort  = parts[2].trimmed();

    if (name.isEmpty() || uuid.isNull()) return std::nullopt;

    const int colonIdx = ipPort.lastIndexOf(':');
    if (colonIdx < 0) return std::nullopt;

    const QString ip   = ipPort.left(colonIdx);
    const quint16 port = static_cast<quint16>(ipPort.mid(colonIdx + 1).toUInt());

    if (ip.isEmpty() || port == 0) return std::nullopt;

    return PeerInfo{
        .name = name,
        .uuid = uuid,
        .ip   = ip,
        .port = port,
    };
}
