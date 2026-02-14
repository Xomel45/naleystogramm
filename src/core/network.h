#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QUuid>
#include "identity.h"

// One connected peer
struct PeerConnection {
    QUuid       uuid    {};
    QString     name    {};
    QString     ip      {};
    quint16     port    {0};
    QTcpSocket* socket  {nullptr};
    QByteArray  readBuf {};
};

class NetworkManager : public QObject {
    Q_OBJECT
public:
    explicit NetworkManager(QObject* parent = nullptr);
    ~NetworkManager();

    // Start listening + discover external IP + try UPnP
    void        init();

    [[nodiscard]] QString  externalIp()  const noexcept { return m_externalIp; }
    [[nodiscard]] quint16  localPort()   const noexcept { return m_localPort; }
    [[nodiscard]] bool     upnpMapped()  const noexcept { return m_upnpMapped; }

    // Initiate outgoing connection
    void        connectToPeer(const PeerInfo& peer);

    // Accept/reject a pending incoming connection
    void        acceptIncoming(const QUuid& peerUuid);
    void        rejectIncoming(const QUuid& peerUuid);

    // Send raw JSON to a connected peer
    void        sendJson(const QUuid& peerUuid, const QJsonObject& obj);

    bool        isOnline(const QUuid& uuid) const;

signals:
    void        ready(const QString& externalIp, quint16 port, bool upnpOk);
    void        externalIpDiscovered(const QString& ip);

    // Someone wants to connect — show confirmation dialog
    void        incomingRequest(QUuid peerUuid, QString peerName, QString peerIp);

    // JSON message from a peer
    void        messageReceived(QUuid fromUuid, QJsonObject msg);

    void        peerConnected(QUuid uuid, QString name);
    void        peerDisconnected(QUuid uuid);

    void        error(const QString& msg);

private slots:
    void        onNewConnection();
    void        onSocketReadyRead();
    void        onSocketDisconnected();

private:
    void        startServer();
    void        discoverExternalIp();
    void        tryUpnp();

    void        handleFrame(PeerConnection& peer, const QJsonObject& obj);
    void        sendHandshake(QTcpSocket* socket);
    void        tryParseFrames(PeerConnection& conn, bool isPending);

    QTcpServer*                  m_server{nullptr};
    QMap<QUuid, PeerConnection>  m_peers;       // confirmed connections
    QMap<QUuid, PeerConnection>  m_pending;     // awaiting user confirmation

    QString     m_externalIp;
    quint16     m_localPort{47821};
    bool        m_upnpMapped{false};

    static constexpr quint16 kDefaultPort = 47821;
};
