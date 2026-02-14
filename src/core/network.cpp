#include "network.h"
#include "identity.h"
#include "upnp.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QTimer>

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &NetworkManager::onNewConnection);
}

NetworkManager::~NetworkManager() {
    for (auto& peer : m_peers)
        if (peer.socket) peer.socket->disconnectFromHost();
}

// ── Init ──────────────────────────────────────────────────────────────────

void NetworkManager::init() {
    startServer();
    discoverExternalIp();
    // UPnP runs after server is up
}

void NetworkManager::startServer() {
    quint16 port = kDefaultPort;
    while (!m_server->listen(QHostAddress::Any, port)) {
        if (++port > kDefaultPort + 20) {
            emit error("Cannot bind to any port near " +
                       QString::number(kDefaultPort));
            return;
        }
    }
    m_localPort = port;
    qDebug("[Network] Listening on port %d", port);
    tryUpnp();
}

void NetworkManager::discoverExternalIp() {
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl("https://api.ipify.org?format=json"));
    req.setTransferTimeout(5000);

    auto* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();

        if (reply->error() == QNetworkReply::NoError) {
            const auto doc = QJsonDocument::fromJson(reply->readAll());
            m_externalIp = doc.object()["ip"].toString();
            qDebug("[Network] External IP: %s", qPrintable(m_externalIp));
            emit externalIpDiscovered(m_externalIp);
        } else {
            qWarning("[Network] IP discovery failed: %s",
                     qPrintable(reply->errorString()));
        }
        emit ready(m_externalIp, m_localPort, m_upnpMapped);
    });
}

void NetworkManager::tryUpnp() {
    auto* upnp = new UpnpMapper(this);
    connect(upnp, &UpnpMapper::mapped, this, [this, upnp](bool ok) {
        m_upnpMapped = ok;
        upnp->deleteLater();
        qDebug("[Network] UPnP: %s", ok ? "OK" : "failed");
    });
    upnp->mapPort(m_localPort);
}

// ── Outgoing connection ────────────────────────────────────────────────────

void NetworkManager::connectToPeer(const PeerInfo& peer) {
    auto* socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::connected, this, [this, socket, peer]() {
        qDebug("[Network] Connected to %s:%d", qPrintable(peer.ip), peer.port);

        // C++20: designated initializers
        m_peers[peer.uuid] = PeerConnection{
            .uuid   = peer.uuid,
            .name   = peer.name,
            .ip     = peer.ip,
            .port   = peer.port,
            .socket = socket,
        };

        connect(socket, &QTcpSocket::readyRead,
                this, &NetworkManager::onSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &NetworkManager::onSocketDisconnected);

        sendHandshake(socket);
    });

    connect(socket, &QTcpSocket::errorOccurred, this,
        [this, socket](QAbstractSocket::SocketError) {
            emit error("Connection failed: " + socket->errorString());
            socket->deleteLater();
        });

    socket->connectToHost(peer.ip, peer.port);
}

// ── Incoming connection ────────────────────────────────────────────────────

void NetworkManager::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        auto* socket = m_server->nextPendingConnection();

        const QUuid tempId = QUuid::createUuid();

        // C++20: designated initializers
        m_pending[tempId] = PeerConnection{
            .uuid   = tempId,
            .ip     = socket->peerAddress().toString(),
            .socket = socket,
        };

        connect(socket, &QTcpSocket::readyRead, this,
            [this, tempId]() {
                if (!m_pending.contains(tempId)) return;
                auto& conn = m_pending[tempId];
                conn.readBuf += conn.socket->readAll();
                tryParseFrames(conn, true /*isPending*/);
            });

        connect(socket, &QTcpSocket::disconnected, this, [this, tempId]() {
            if (m_pending.contains(tempId)) {
                m_pending[tempId].socket->deleteLater();
                m_pending.remove(tempId);
            }
        });
    }
}

// ── Frame parsing ──────────────────────────────────────────────────────────

void NetworkManager::tryParseFrames(PeerConnection& conn, bool /*isPending*/) {
    int newline;
    while ((newline = conn.readBuf.indexOf('\n')) != -1) {
        const QByteArray frame = conn.readBuf.left(newline);
        conn.readBuf = conn.readBuf.mid(newline + 1);

        if (const auto doc = QJsonDocument::fromJson(frame); !doc.isNull())
            handleFrame(conn, doc.object());
    }
}

// tryParseFrames is inlined in .cpp via lambda; let's do it properly:
void NetworkManager::onSocketReadyRead() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    for (auto& peer : m_peers) {
        if (peer.socket != socket) continue;
        peer.readBuf += socket->readAll();
        tryParseFrames(peer, false);
        break;
    }
}

void NetworkManager::handleFrame(PeerConnection& peer, const QJsonObject& obj) {
    const QString type = obj["type"].toString();

    if (type == "HANDSHAKE") {
        // They initiated — show confirmation dialog
        peer.uuid = QUuid(obj["uuid"].toString());
        peer.name = obj["name"].toString();
        emit incomingRequest(peer.uuid, peer.name, peer.ip);
        return;
    }

    if (type == "HANDSHAKE_ACK") {
        if (obj["accepted"].toBool()) {
            peer.uuid = QUuid(obj["uuid"].toString());
            peer.name = obj["name"].toString();
            emit peerConnected(peer.uuid, peer.name);
        } else {
            emit error("Connection rejected by " + peer.name);
            peer.socket->disconnectFromHost();
        }
        return;
    }

    // All other message types go up to application layer
    emit messageReceived(peer.uuid, obj);
}

void NetworkManager::onSocketDisconnected() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        if (it.value().socket == socket) {
            const QUuid uuid = it.key();
            socket->deleteLater();
            m_peers.erase(it);
            emit peerDisconnected(uuid);
            return;
        }
    }
}

// ── Send ──────────────────────────────────────────────────────────────────

void NetworkManager::sendHandshake(QTcpSocket* socket) {
    const auto& id = Identity::instance();
    const QJsonObject obj{
        {"type", "HANDSHAKE"},
        {"uuid", id.uuid().toString(QUuid::WithoutBraces)},
        {"name", id.displayName()},
    };
    socket->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n');
}

void NetworkManager::sendJson(const QUuid& peerUuid, const QJsonObject& obj) {
    if (!m_peers.contains(peerUuid)) {
        qWarning("[Network] sendJson: unknown peer %s",
                 qPrintable(peerUuid.toString()));
        return;
    }
    m_peers[peerUuid].socket->write(
        QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n');
}

void NetworkManager::acceptIncoming(const QUuid& peerUuid) {
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        if (it.value().uuid == peerUuid) {
            auto conn = it.value();
            m_pending.erase(it);
            m_peers[peerUuid] = conn;

            disconnect(conn.socket, &QTcpSocket::readyRead, nullptr, nullptr);
            connect(conn.socket, &QTcpSocket::readyRead,
                    this, &NetworkManager::onSocketReadyRead);
            connect(conn.socket, &QTcpSocket::disconnected,
                    this, &NetworkManager::onSocketDisconnected);

            const auto& id = Identity::instance();
            const QJsonObject ack{
                {"type",     "HANDSHAKE_ACK"},
                {"accepted", true},
                {"uuid",     id.uuid().toString(QUuid::WithoutBraces)},
                {"name",     id.displayName()},
            };
            conn.socket->write(
                QJsonDocument(ack).toJson(QJsonDocument::Compact) + '\n');
            emit peerConnected(peerUuid, conn.name);
            return;
        }
    }
}

void NetworkManager::rejectIncoming(const QUuid& peerUuid) {
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        if (it.value().uuid == peerUuid) {
            const QJsonObject ack{
                {"type",     "HANDSHAKE_ACK"},
                {"accepted", false},
            };
            it.value().socket->write(
                QJsonDocument(ack).toJson(QJsonDocument::Compact) + '\n');
            QTimer::singleShot(500, it.value().socket,
                               &QTcpSocket::disconnectFromHost);
            m_pending.erase(it);
            return;
        }
    }
}

bool NetworkManager::isOnline(const QUuid& uuid) const {
    return m_peers.contains(uuid);
}
