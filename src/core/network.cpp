#include "network.h"
#include "identity.h"
#include <QPointer>
#include "upnp.h"
#include "systeminfo.h"
#include "sessionmanager.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkInterface>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QTimer>
#include <QRegularExpression>
#include <QtMath>

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &NetworkManager::onNewConnection);
}

NetworkManager::~NetworkManager() {
    // Останавливаем все таймеры и отключаем сокеты
    for (auto& peer : m_peers) {
        stopKeepalive(peer.uuid);
        if (peer.reconnectTimer) {
            peer.reconnectTimer->stop();
            peer.reconnectTimer->deleteLater();
        }
        if (peer.socket) peer.socket->disconnectFromHost();
    }
}

// ── Логирование ──────────────────────────────────────────────────────────────

void NetworkManager::log(const QString& message, bool forceVerbose) {
    // Всегда логируем в qDebug
    qDebug("[Network] %s", qPrintable(message));

    // Эмитим сигнал только если включено подробное логирование или force
    if (m_verboseLogging || forceVerbose)
        emit connectionLog(message);
}

void NetworkManager::setVerboseLogging(bool enabled) {
    m_verboseLogging = enabled;
    log(QString("Verbose logging %1").arg(enabled ? "enabled" : "disabled"), true);
}

ConnectionState NetworkManager::connectionState(const QUuid& uuid) const {
    if (m_peers.contains(uuid))
        return m_peers[uuid].state;
    return ConnectionState::Disconnected;
}

PeerPublicInfo NetworkManager::getPeerInfo(const QUuid& uuid) const {
    if (!m_peers.contains(uuid)) return {};
    const auto& p = m_peers[uuid];
    return PeerPublicInfo{
        .name           = p.name,
        .ip             = p.ip,
        .serverPort     = p.serverPort,
        .state          = p.state,
        .latencyMs      = p.latencyMs,
        .connectedSince = p.connectedSince,
        .systemInfo     = p.systemInfo,
        .avatarHash     = p.avatarHash,
    };
}

// ── Вспомогательная функция: лучший локальный LAN IP ─────────────────────
// Используется в режиме Disabled для определения адреса объявляемого пирам.

static QString detectLocalLanIp() {
    static const QStringList kVpnPrefixes {
        "tun", "tap", "wg", "utun", "ppp", "vpn", "veth", "docker", "virbr", "br-"
    };
    QString best192, best10, other;
    for (const auto& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags() & QNetworkInterface::IsLoopBack)   continue;
        if (!(iface.flags() & QNetworkInterface::IsUp))      continue;
        if (!(iface.flags() & QNetworkInterface::IsRunning)) continue;
        const QString name = iface.name().toLower();
        bool isVpn = false;
        for (const auto& pfx : kVpnPrefixes)
            if (name.startsWith(pfx)) { isVpn = true; break; }
        if (isVpn) continue;
        for (const auto& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            const QString ip = entry.ip().toString();
            if      (ip.startsWith("192.168.") && best192.isEmpty()) best192 = ip;
            else if (ip.startsWith("10.")       && best10.isEmpty())  best10  = ip;
            else if (other.isEmpty())                                  other   = ip;
        }
    }
    return !best192.isEmpty() ? best192 : !best10.isEmpty() ? best10 : other;
}

// ── Init ──────────────────────────────────────────────────────────────────

void NetworkManager::init() {
    startServer();
    // m_advertisedPort: по умолчанию совпадает с локальным портом;
    // переопределяется в Manual-режиме или после успешного UPnP.
    m_advertisedPort = m_localPort;

    const auto mode = SessionManager::instance().portForwardingMode();

    if (mode == PortForwardingMode::Manual) {
        // Ручной режим: пользователь задал публичный IP и порт вручную.
        // UPnP не запускаем, IP-discovery не делаем.
        const QString manIp   = SessionManager::instance().manualPublicIp();
        const quint16 manPort = SessionManager::instance().manualPublicPort();
        m_externalIp     = manIp;
        m_advertisedPort = (manPort > 0) ? manPort : m_localPort;
        qDebug("[Network] Режим Manual: публичный адрес %s:%d",
               qPrintable(m_externalIp), m_advertisedPort);
        if (!m_externalIp.isEmpty())
            emit externalIpDiscovered(m_externalIp);
        emit ready(m_externalIp, m_advertisedPort, false);

    } else if (mode == PortForwardingMode::Disabled) {
        // Режим Disabled: используем локальный LAN IP, UPnP не запускаем.
        m_externalIp     = detectLocalLanIp();
        m_advertisedPort = m_localPort;
        qDebug("[Network] Режим Disabled: локальный адрес %s:%d",
               qPrintable(m_externalIp), m_advertisedPort);
        emit externalIpDiscovered(m_externalIp);
        emit ready(m_externalIp, m_advertisedPort, false);

    } else if (mode == PortForwardingMode::ClientServer) {
        // Режим Client-Server: все пиры подключаются через ретрансляционный сервер.
        m_externalIp     = QString();
        m_advertisedPort = m_localPort;
        qDebug("[Network] Режим Client-Server: подключаемся к ретранслятору");
        connectToRelay();
        emit ready(m_externalIp, m_advertisedPort, false);

    } else {
        // Режим UpnpAuto (по умолчанию): запускаем UPnP + discovery внешнего IP.
        tryUpnp();
        discoverExternalIp();
    }
}

void NetworkManager::startServer() {
    const quint16 configuredPort = SessionManager::instance().port();
    quint16 port = configuredPort;
    while (!m_server->listen(QHostAddress::Any, port)) {
        if (++port > configuredPort + 20) {
            emit error("Cannot bind to any port near " +
                       QString::number(configuredPort));
            return;
        }
    }
    m_localPort = port;
    qDebug("[Network] Listening on port %d", port);
    // UPnP запускается из init() с учётом выбранного режима
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
            const QString ip = doc.object()["ip"].toString().trimmed();

            // M-2: принимаем только корректные IPv4/IPv6 адреса.
            // Regex: IPv4 — четыре октета по 1–3 цифры через точку;
            //        IPv6 — шестнадцатеричные группы через двоеточие (включая сжатые формы).
            static const QRegularExpression kIpv4(
                QStringLiteral("^(\\d{1,3}\\.){3}\\d{1,3}$"));
            static const QRegularExpression kIpv6(
                QStringLiteral("^[0-9a-fA-F:]{2,39}$"));

            if (!kIpv4.match(ip).hasMatch() && !kIpv6.match(ip).hasMatch()) {
                qWarning("[Network] Получен невалидный внешний IP: '%s' — игнорируем",
                         qPrintable(ip));
            } else {
                m_externalIp = ip;
                qDebug("[Network] External IP: %s", qPrintable(m_externalIp));
                emit externalIpDiscovered(m_externalIp);
            }
        } else {
            qWarning("[Network] IP discovery failed: %s",
                     qPrintable(reply->errorString()));
        }
        emit ready(m_externalIp, m_advertisedPort, m_upnpMapped);
    });
}

void NetworkManager::tryUpnp() {
    auto* upnp = new UpnpMapper(this);
    connect(upnp, &UpnpMapper::mapped, this, [this, upnp](bool ok) {
        m_upnpMapped = ok;
        upnp->deleteLater();
        qDebug("[Network] UPnP: %s", ok ? "OK" : "failed");
        emit upnpMappingResult(ok);
    });
    upnp->mapPort(m_localPort);
}

void NetworkManager::retryUpnp() {
    // Повторная попытка имеет смысл только в режиме UPnP Auto
    if (SessionManager::instance().portForwardingMode() != PortForwardingMode::UpnpAuto) {
        log("retryUpnp: активен не UPnP-режим — пропускаем", true);
        return;
    }
    log("Повторная попытка UPnP маппинга...", true);
    m_upnpMapped = false;
    tryUpnp();
}

// ── Ретранслятор (Client-Server) ─────────────────────────────────────────────

void NetworkManager::connectToRelay() {
    const QString relayIp  = SessionManager::instance().relayServerIp();
    const quint16 relayTcp = SessionManager::instance().relayTcpPort();

    if (relayIp.isEmpty()) {
        qWarning("[Network] Relay: IP сервера не задан — пропускаем подключение");
        return;
    }

    if (m_relaySocket) {
        m_relaySocket->abort();
        m_relaySocket->deleteLater();
    }

    m_relaySocket     = new QTcpSocket(this);
    m_relayRegistered = false;

    connect(m_relaySocket, &QTcpSocket::connected,
            this, &NetworkManager::onRelayConnected);
    connect(m_relaySocket, &QTcpSocket::readyRead,
            this, &NetworkManager::onRelayReadyRead);
    connect(m_relaySocket, &QTcpSocket::disconnected,
            this, &NetworkManager::onRelayDisconnected);
    connect(m_relaySocket, &QTcpSocket::errorOccurred, this,
        [this](QAbstractSocket::SocketError) {
            qWarning("[Network] Relay socket error: %s",
                     qPrintable(m_relaySocket->errorString()));
        });

    qDebug("[Network] Relay: подключаемся к %s:%d", qPrintable(relayIp), relayTcp);
    m_relaySocket->connectToHost(relayIp, relayTcp);
}

void NetworkManager::onRelayConnected() {
    qDebug("[Network] Relay: подключились, отправляем RELAY_REGISTER");
    const QJsonObject reg{
        {"type", "RELAY_REGISTER"},
        {"uuid", Identity::instance().uuid().toString(QUuid::WithoutBraces)},
    };
    m_relaySocket->write(QJsonDocument(reg).toJson(QJsonDocument::Compact) + '\n');
}

void NetworkManager::onRelayReadyRead() {
    const QByteArray incoming = m_relaySocket->readAll();
    if (m_relayReadBuf.size() + incoming.size() > kMaxBufferSize) {
        qWarning("[Network] Relay: буфер переполнен (%d МБ) — сбрасываем соединение",
                 kMaxBufferSize / (1024 * 1024));
        m_relayReadBuf.clear();
        m_relaySocket->abort();
        return;
    }
    m_relayReadBuf += incoming;
    int newline;
    while ((newline = m_relayReadBuf.indexOf('\n')) != -1) {
        const QByteArray frame = m_relayReadBuf.left(newline);
        m_relayReadBuf = m_relayReadBuf.mid(newline + 1);

        const auto doc = QJsonDocument::fromJson(frame);
        if (doc.isNull()) continue;
        const auto obj  = doc.object();
        const QString t = obj["type"].toString();

        if (t == "RELAY_REGISTERED") {
            m_relayRegistered = true;
            qDebug("[Network] Relay: зарегистрированы на сервере");
            emit relayConnected();

        } else if (t == "RELAY_MSG") {
            const QUuid fromUuid = QUuid(obj["from"].toString());
            const QJsonObject inner = obj["data"].toObject();
            if (!fromUuid.isNull() && !inner.isEmpty())
                handleRelayFrame(fromUuid, inner);

        } else if (t == "RELAY_PEER_OFFLINE") {
            const QUuid peerUuid = QUuid(obj["uuid"].toString());
            qDebug("[Network] Relay: пир %s недоступен",
                   qPrintable(peerUuid.toString(QUuid::WithoutBraces)));
            if (m_peers.contains(peerUuid)) {
                stopKeepalive(peerUuid);
                m_relayPeers.remove(peerUuid);
                m_peers.remove(peerUuid);
                emit peerDisconnected(peerUuid);
                emit connectionStateChanged(peerUuid, ConnectionState::Disconnected);
            }

        } else if (t == "RELAY_ERROR") {
            qWarning("[Network] Relay error: %s", qPrintable(obj["msg"].toString()));
        }
    }
}

void NetworkManager::onRelayDisconnected() {
    m_relayRegistered = false;
    qWarning("[Network] Relay: соединение с сервером разорвано");
    emit relayDisconnected();

    // Планируем переподключение через 5 секунд
    if (!m_relayReconnectTimer) {
        m_relayReconnectTimer = new QTimer(this);
        m_relayReconnectTimer->setSingleShot(true);
        connect(m_relayReconnectTimer, &QTimer::timeout,
                this, &NetworkManager::connectToRelay);
    }
    m_relayReconnectTimer->start(5000);
}

void NetworkManager::sendViaRelay(const QUuid& targetUuid, const QJsonObject& obj) {
    if (!m_relaySocket ||
        m_relaySocket->state() != QAbstractSocket::ConnectedState ||
        !m_relayRegistered)
    {
        qWarning("[Network] sendViaRelay: ретранслятор недоступен — [%s] отброшен",
                 qPrintable(obj.value("type").toString("?")));
        return;
    }
    const QJsonObject wrapper{
        {"type", "RELAY_MSG"},
        {"to",   targetUuid.toString(QUuid::WithoutBraces)},
        {"data", obj},
    };
    m_relaySocket->write(QJsonDocument(wrapper).toJson(QJsonDocument::Compact) + '\n');
    log(QString("Relay: [%1] → %2")
        .arg(obj.value("type").toString("?"),
             targetUuid.toString(QUuid::WithoutBraces)));
}

void NetworkManager::handleRelayFrame(const QUuid& fromUuid, const QJsonObject& innerObj) {
    // Уже известный активный пир (например ждём HANDSHAKE_ACK после нашего исходящего)
    if (m_peers.contains(fromUuid)) {
        handleFrame(m_peers[fromUuid], innerObj);
        return;
    }
    // Входящий relay-пир: создаём pending-запись с его UUID как ключом
    if (!m_pending.contains(fromUuid)) {
        m_pending[fromUuid] = PeerConnection{
            .uuid   = fromUuid,
            .ip     = SessionManager::instance().relayServerIp(),
            .socket = nullptr,
        };
    }
    handleFrame(m_pending[fromUuid], innerObj);
}

void NetworkManager::broadcastProfileUpdate(const QString& name) {
    const QJsonObject msg{
        {"type", "PROFILE_UPDATE"},
        {"name", name},
    };
    for (const auto& peer : m_peers)
        sendJson(peer.uuid, msg);
    log(QString("PROFILE_UPDATE отправлен %1 пирам: \"%2\"")
        .arg(m_peers.size()).arg(name), true);
}

// ── Исходящее подключение ──────────────────────────────────────────────────

void NetworkManager::connectToPeer(const PeerInfo& peer) {
    // Предотвращаем дублирующие подключения к одному UUID.
    // Это критично при взаимных одновременных попытках подключения.
    if (m_peers.contains(peer.uuid)) {
        const auto st = m_peers[peer.uuid].state;
        if (st == ConnectionState::Connected || st == ConnectionState::Connecting) {
            log(QString("Уже подключены к %1 — пропускаем повторное подключение")
                .arg(peer.name), true);
            return;
        }
    }

    // ── Режим Client-Server: подключаемся через ретранслятор ─────────────
    if (SessionManager::instance().portForwardingMode() == PortForwardingMode::ClientServer) {
        m_reconnectInfo[peer.uuid] = PeerReconnectInfo{
            .name = peer.name,
            .ip   = peer.ip,
            .port = peer.port,
        };
        m_peers[peer.uuid] = PeerConnection{
            .uuid   = peer.uuid,
            .name   = peer.name,
            .ip     = peer.ip,
            .port   = peer.port,
            .socket = nullptr,
            .state  = ConnectionState::Connecting,
            .connectedSince = QDateTime::currentDateTime(),
        };
        m_relayPeers.insert(peer.uuid);
        emit connectionStateChanged(peer.uuid, ConnectionState::Connecting);

        if (m_relayRegistered) {
            const auto& id = Identity::instance();
            const QString ownAvatarPath = SessionManager::instance().avatarPath();
            const QString ownAvatarHash = ownAvatarPath.isEmpty() ? QString()
                : QString::fromLatin1(SessionManager::computeAvatarHash(ownAvatarPath));
            const QJsonObject hs{
                {"type",       "HANDSHAKE"},
                {"uuid",       id.uuid().toString(QUuid::WithoutBraces)},
                {"name",       id.displayName()},
                {"port",       static_cast<int>(m_localPort)},
                {"systemInfo", SystemInfo::instance().toJsonForHandshake(m_externalIp)},
                {"avatarHash", ownAvatarHash},
            };
            sendViaRelay(peer.uuid, hs);
            log(QString("Relay: HANDSHAKE отправлен → %1").arg(peer.name));
        } else {
            qWarning("[Network] connectToPeer via relay: ретранслятор не подключён");
        }
        return;
    }

    log(QString("Connecting to %1 (%2:%3)...")
        .arg(peer.name, peer.ip).arg(peer.port));

    // Сохраняем информацию для возможного переподключения
    m_reconnectInfo[peer.uuid] = PeerReconnectInfo{
        .name = peer.name,
        .ip   = peer.ip,
        .port = peer.port
    };

    auto* socket = new QTcpSocket(this);

    // Создаём таймер тайм-аута подключения
    auto* timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);

    // QPointer: если таймер уже удалён (timeout сработал раньше) — errorOccurred
    // не будет вызывать deleteLater на уже мёртвый объект.
    QPointer<QTimer> timerGuard = timeoutTimer;

    connect(timeoutTimer, &QTimer::timeout, this, [this, socket, peer, timerGuard]() {
        log(QString("Connection timeout to %1").arg(peer.name));
        socket->abort();
        socket->deleteLater();
        if (timerGuard) timerGuard->deleteLater();

        // Планируем переподключение
        scheduleReconnect(peer.uuid);
    });

    connect(socket, &QTcpSocket::connected, this, [this, socket, peer, timerGuard]() {
        // Останавливаем таймер тайм-аута
        if (timerGuard) { timerGuard->stop(); timerGuard->deleteLater(); }

        // Повторная проверка: пока мы ждали TCP-connect, входящий сокет мог уже принять
        // этот пир. Закрываем дублирующий исходящий сокет.
        if (m_peers.contains(peer.uuid) &&
            m_peers[peer.uuid].state == ConnectionState::Connected)
        {
            log(QString("Дублирующий исходящий сокет к %1 — уже подключены, закрываем")
                .arg(peer.name), true);
            socket->abort();
            socket->deleteLater();
            return;
        }

        log(QString("Connected to %1 (%2:%3)")
            .arg(peer.name, peer.ip).arg(peer.port));

        // Создаём запись пира с полным состоянием
        m_peers[peer.uuid] = PeerConnection{
            .uuid              = peer.uuid,
            .name              = peer.name,
            .ip                = peer.ip,
            .port              = peer.port,
            .socket            = socket,
            .readBuf           = {},
            .state             = ConnectionState::Connected,
            .reconnectAttempts = 0,
            .lastActivity      = QDateTime::currentDateTime(),
            .connectedSince    = QDateTime::currentDateTime(),
        };

        // Сбрасываем счётчик переподключений при успешном подключении
        resetReconnectState(peer.uuid);

        connect(socket, &QTcpSocket::readyRead,
                this, &NetworkManager::onSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &NetworkManager::onSocketDisconnected);

        sendHandshake(socket);
        log(QString("Handshake sent to %1").arg(peer.name));

        // Запускаем keepalive
        startKeepalive(peer.uuid);

        emit connectionStateChanged(peer.uuid, ConnectionState::Connected);
    });

    connect(socket, &QTcpSocket::errorOccurred, this,
        [this, socket, peer, timerGuard](QAbstractSocket::SocketError err) {
            Q_UNUSED(err);
            if (timerGuard) { timerGuard->stop(); timerGuard->deleteLater(); }

            log(QString("Ошибка подключения к %1: %2")
                .arg(peer.name, socket->errorString()), true);

            socket->deleteLater();

            // Планируем переподключение с экспоненциальным откатом.
            // ConnectionRefused тоже обрабатываем — сервер может быть временно недоступен.
            scheduleReconnect(peer.uuid);
        });

    // Обновляем состояние
    emit connectionStateChanged(peer.uuid, ConnectionState::Connecting);

    // Запускаем таймер тайм-аута
    timeoutTimer->start(kConnectionTimeout);

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
                const QByteArray incoming = conn.socket->readAll();

                // M-1: проверяем лимит ДО добавления в буфер — иначе уже переполнено
                if (conn.readBuf.size() + incoming.size() > kMaxBufferSize) {
                    log(QString("DoS-атака: pending буфер от %1 превысил %2 МБ — отключаем")
                        .arg(conn.ip).arg(kMaxBufferSize / (1024 * 1024)), true);
                    conn.socket->abort();
                    return;
                }
                conn.readBuf += incoming;

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

        // Защита от аномально больших JSON-фреймов ДО парсинга:
        // avatarHash, systemInfo и прочие строки-данные не должны превышать 1 МБ.
        // Настоящий злоумышленник может попытаться прислать 100 МБ в одном поле —
        // QJsonDocument попробует разместить это в памяти до проверки типа.
        static constexpr int kMaxFrameSize = 1 * 1024 * 1024; // 1 МБ
        if (frame.size() > kMaxFrameSize) {
            log(QString("Большой фрейм %1 байт от %2 — отброшен до парсинга")
                .arg(frame.size()).arg(conn.ip), true);
            continue;
        }

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

        const QByteArray incoming = socket->readAll();

        // M-1: проверяем лимит ДО добавления в буфер — иначе уже переполнено
        if (peer.readBuf.size() + incoming.size() > kMaxBufferSize) {
            log(QString("DoS-атака: буфер пира %1 превысил %2 МБ — отключаем")
                .arg(peer.name).arg(kMaxBufferSize / (1024 * 1024)), true);
            socket->abort();
            return;
        }
        peer.readBuf += incoming;

        tryParseFrames(peer, false);
        break;
    }
}

void NetworkManager::handleFrame(PeerConnection& peer, const QJsonObject& obj) {
    const QString type = obj["type"].toString();

    // Обновляем время последней активности
    peer.lastActivity = QDateTime::currentDateTime();

    if (type == "HANDSHAKE") {
        // M-4: валидируем UUID до сохранения — нулевой UUID недопустим
        const QUuid parsedUuid = QUuid(obj["uuid"].toString());
        if (parsedUuid.isNull()) {
            log(QString("HANDSHAKE: невалидный UUID от %1 — разрываем соединение").arg(peer.ip), true);
            peer.socket->abort();
            return;
        }

        // H-1: ограничиваем длину имени (256 символов), удаляем управляющие символы
        static const QRegularExpression kCtrlChars(QStringLiteral("[\\x00-\\x1F\\x7F]"));
        const QString rawName = obj["name"].toString();

        peer.uuid       = parsedUuid;
        peer.name       = rawName.left(256).remove(kCtrlChars).trimmed();
        peer.serverPort = static_cast<quint16>(obj["port"].toInt(0));
        peer.avatarHash = obj["avatarHash"].toString();

        // H-2: проверяем размер systemInfo — более 4 КБ не принимаем (защита от DoS)
        const QJsonObject rawSysInfo = obj["systemInfo"].toObject();
        if (QJsonDocument(rawSysInfo).toJson(QJsonDocument::Compact).size() <= 4096)
            peer.systemInfo = rawSysInfo;
        else
            log(QString("HANDSHAKE: systemInfo от %1 превышает 4 КБ — игнорируем").arg(peer.ip), true);

        log(QString("HANDSHAKE от %1 (серверный порт: %2, systemInfo получен: %3)")
            .arg(peer.name).arg(peer.serverPort)
            .arg(!peer.systemInfo.isEmpty() ? "да" : "нет"));

        // ── Правило «старшего UUID» ────────────────────────────────────────────
        // Если мы сами сейчас подключаемся к этому же пиру (взаимный connect),
        // тот у кого UUID лексикографически «больше» оставляет свой исходящий
        // сокет — входящий молча закрывается. Это предотвращает дублирующие сессии
        // и гарантирует единственный Double Ratchet стрим на пару.
        if (peer.socket && m_reconnectInfo.contains(parsedUuid)) {
            const QString myStr   = Identity::instance().uuid().toString();
            const QString peerStr = parsedUuid.toString();
            if (myStr > peerStr) {
                log(QString("Tie-breaking: наш UUID > %1 — закрываем входящее, сохраняем исходящее")
                    .arg(peer.name), true);
                // Откладываем abort() чтобы избежать реентерабельности внутри readyRead
                QPointer<QTcpSocket> sock = peer.socket;
                QTimer::singleShot(0, this, [sock]() { if (sock) sock->abort(); });
                return;
            }
            // Наш UUID «меньше» — входящее приоритетнее; isходящий сокет прервётся
            // сам когда пир не пришлёт HANDSHAKE_ACK (timeout) или закроет соединение.
        }

        emit incomingRequest(peer.uuid, peer.name, peer.ip);
        emit peerInfoUpdated(peer.uuid);
        return;
    }

    if (type == "HANDSHAKE_ACK") {
        if (obj["accepted"].toBool()) {
            const QUuid confirmedUuid = QUuid(obj["uuid"].toString());
            const QString confirmedName = obj["name"].toString();

            // Если к этому UUID уже есть подключённый пир (входящее сработало быстрее) —
            // просто закрываем этот дублирующий исходящий сокет без дополнительных действий.
            if (m_peers.contains(confirmedUuid) &&
                m_peers[confirmedUuid].state == ConnectionState::Connected &&
                m_peers[confirmedUuid].socket != peer.socket)
            {
                log(QString("HANDSHAKE_ACK: дублирующее соединение к %1 — закрываем лишний сокет")
                    .arg(confirmedName), true);
                peer.socket->abort();
                return;
            }

            peer.uuid = confirmedUuid;
            peer.name = confirmedName;
            // Сохраняем системную информацию и аватар пира из ACK.
            // Инициатор получает эти данные здесь — в HANDSHAKE они шли в другую сторону.
            // H-2: проверяем размер systemInfo из ACK — более 4 КБ не принимаем
            const QJsonObject rawSysInfoAck = obj["systemInfo"].toObject();
            if (QJsonDocument(rawSysInfoAck).toJson(QJsonDocument::Compact).size() <= 4096)
                peer.systemInfo = rawSysInfoAck;
            else
                log(QString("HANDSHAKE_ACK: systemInfo от %1 превышает 4 КБ — игнорируем").arg(peer.name), true);
            peer.avatarHash = obj["avatarHash"].toString();
            log(QString("HANDSHAKE_ACK принят от %1 (systemInfo: %2)")
                .arg(peer.name)
                .arg(!peer.systemInfo.isEmpty() ? "получена" : "отсутствует"));
            emit peerConnected(peer.uuid, peer.name);
            // Отправляем накопленные сообщения из очереди
            drainMessageQueue(peer.uuid);
        } else {
            log(QString("HANDSHAKE_ACK отклонён от %1").arg(peer.name));
            emit error(tr("Подключение отклонено: ") + peer.name);
            peer.socket->disconnectFromHost();
        }
        return;
    }

    // Обработка PING/PONG для keepalive
    if (type == "PING") {
        handlePing(peer, obj);
        return;
    }

    if (type == "PONG") {
        handlePong(peer, obj);
        return;
    }

    // Пир сменил отображаемое имя — обновляем локально и уведомляем UI
    if (type == "PROFILE_UPDATE") {
        // H-1: ограничиваем длину имени, удаляем управляющие символы
        static const QRegularExpression kCtrlCharsUpdate(QStringLiteral("[\\x00-\\x1F\\x7F]"));
        const QString newName = obj["name"].toString().left(256).remove(kCtrlCharsUpdate).trimmed();
        if (!newName.isEmpty() && newName != peer.name) {
            log(QString("PROFILE_UPDATE от %1: новое имя \"%2\"")
                .arg(peer.name, newName));
            peer.name = newName;
            emit contactNameUpdated(peer.uuid, newName);
        }
        return;
    }

    // Все остальные типы сообщений передаём наверх
    emit messageReceived(peer.uuid, obj);
}

void NetworkManager::onSocketDisconnected() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        if (it.value().socket == socket) {
            const QUuid uuid = it.key();
            const QString name = it.value().name;

            log(QString("Disconnected from %1").arg(name));

            // Останавливаем keepalive
            stopKeepalive(uuid);

            socket->deleteLater();
            it.value().socket = nullptr;
            it.value().state = ConnectionState::Disconnected;

            m_peers.erase(it);
            emit peerDisconnected(uuid);
            emit connectionStateChanged(uuid, ConnectionState::Disconnected);

            // Планируем переподключение если есть информация о пире
            if (m_reconnectInfo.contains(uuid)) {
                scheduleReconnect(uuid);
            }
            return;
        }
    }
}

// ── Send ──────────────────────────────────────────────────────────────────

void NetworkManager::sendHandshake(QTcpSocket* socket) {
    const auto& id = Identity::instance();
    // Вычисляем хэш своего аватара для сравнения на стороне получателя
    const QString ownAvatarPath = SessionManager::instance().avatarPath();
    const QString ownAvatarHash = ownAvatarPath.isEmpty() ? QString()
        : QString::fromLatin1(SessionManager::computeAvatarHash(ownAvatarPath));
    const QJsonObject obj{
        {"type",       "HANDSHAKE"},
        {"uuid",       id.uuid().toString(QUuid::WithoutBraces)},
        {"name",       id.displayName()},
        // Анонсируемый порт: в Manual-режиме это внешний порт (может отличаться от m_localPort),
        // в остальных режимах совпадает с m_localPort. Нужен получателю для переподключения.
        {"port",       static_cast<int>(m_advertisedPort ? m_advertisedPort : m_localPort)},
        // Системная информация и аватар — для диалога профиля
        // (toJsonForHandshake активирует пасхалку при отсутствии внешнего IP)
        {"systemInfo", SystemInfo::instance().toJsonForHandshake(m_externalIp)},
        {"avatarHash", ownAvatarHash},
    };
    socket->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n');
}

void NetworkManager::sendJson(const QUuid& peerUuid, const QJsonObject& obj) {
    // Relay-пир: маршрутизируем через ретранслятор
    if (m_relayPeers.contains(peerUuid)) {
        sendViaRelay(peerUuid, obj);
        return;
    }

    // Проверяем: пир подключён и сокет в рабочем состоянии?
    if (!m_peers.contains(peerUuid) ||
        !m_peers[peerUuid].socket ||
        m_peers[peerUuid].socket->state() != QAbstractSocket::ConnectedState)
    {
        // Сокет недоступен — ставим сообщение в очередь для отправки после переподключения
        const QString typeHint = obj.value("type").toString("?");
        if (m_messageQueues[peerUuid].size() < kMaxQueueSize) {
            m_messageQueues[peerUuid].enqueue(obj);
            log(QString("sendJson: сокет пира недоступен, [%1] добавлен в очередь (%2/%3)")
                .arg(typeHint)
                .arg(m_messageQueues[peerUuid].size())
                .arg(kMaxQueueSize), true);
        } else {
            log(QString("sendJson: очередь пира переполнена (%1), [%2] отброшен")
                .arg(kMaxQueueSize).arg(typeHint), true);
        }
        return;
    }

    auto& peer = m_peers[peerUuid];
    const QByteArray frame = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    const qint64 written = peer.socket->write(frame);

    if (written != frame.size()) {
        // Запись частично не удалась — логируем и ставим в очередь
        log(QString("sendJson: ОШИБКА записи пиру %1 (записано %2 из %3 байт) — в очередь")
            .arg(peer.name).arg(written).arg(frame.size()), true);
        if (m_messageQueues[peerUuid].size() < kMaxQueueSize)
            m_messageQueues[peerUuid].enqueue(obj);
    } else {
        log(QString("sendJson: [%1] → %2 (%3 байт)")
            .arg(obj.value("type").toString("?"), peer.name).arg(written));
        peer.lastActivity = QDateTime::currentDateTime();
    }
}

void NetworkManager::drainMessageQueue(const QUuid& uuid) {
    if (!m_messageQueues.contains(uuid) || m_messageQueues[uuid].isEmpty()) return;

    const int count = m_messageQueues[uuid].size();
    log(QString("Отправляем %1 сообщений из очереди пиру %2")
        .arg(count).arg(uuid.toString(QUuid::WithoutBraces)), true);

    // Выгружаем очередь целиком до повторного вызова sendJson
    QQueue<QJsonObject> pending = std::move(m_messageQueues[uuid]);
    m_messageQueues.remove(uuid);

    while (!pending.isEmpty()) {
        // sendJson может снова добавить в очередь если сокет уже упал — OK
        sendJson(uuid, pending.dequeue());
    }
}

void NetworkManager::acceptIncoming(const QUuid& peerUuid) {
    // Если к этому UUID уже есть активное соединение (outgoing победило первым) —
    // отклоняем входящее дублирующее подключение, чтобы не иметь двух сокетов.
    if (m_peers.contains(peerUuid) &&
        m_peers[peerUuid].state == ConnectionState::Connected)
    {
        log(QString("Дублирующее входящее от %1 — уже подключены, отклоняем")
            .arg(peerUuid.toString()), true);
        rejectIncoming(peerUuid);
        return;
    }

    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        if (it.value().uuid == peerUuid) {
            auto conn = it.value();
            m_pending.erase(it);

            // Обновляем состояние подключения
            conn.state          = ConnectionState::Connected;
            conn.lastActivity   = QDateTime::currentDateTime();
            conn.connectedSince = QDateTime::currentDateTime();
            conn.reconnectAttempts = 0;

            m_peers[peerUuid] = conn;

            // Сохраняем информацию для переподключения.
            // Используем serverPort из HANDSHAKE — это настоящий слушающий порт пира.
            // conn.port было бы 0 (эфемерный порт не пригоден для переподключения).
            m_reconnectInfo[peerUuid] = PeerReconnectInfo{
                .name = conn.name,
                .ip   = conn.ip,
                .port = conn.serverPort
            };

            // Для relay-пиров (socket == nullptr) сигналы TCP не нужны
            if (conn.socket) {
                disconnect(conn.socket, &QTcpSocket::readyRead, nullptr, nullptr);
                connect(conn.socket, &QTcpSocket::readyRead,
                        this, &NetworkManager::onSocketReadyRead);
                connect(conn.socket, &QTcpSocket::disconnected,
                        this, &NetworkManager::onSocketDisconnected);
            }

            const auto& id = Identity::instance();
            // Включаем свою системную информацию и аватар в ACK —
            // инициатор получает данные о нас именно здесь (в HANDSHAKE их нет).
            const QString ownAvatarPath = SessionManager::instance().avatarPath();
            const QString ownAvatarHash = ownAvatarPath.isEmpty() ? QString()
                : QString::fromLatin1(SessionManager::computeAvatarHash(ownAvatarPath));
            const QJsonObject ack{
                {"type",       "HANDSHAKE_ACK"},
                {"accepted",   true},
                {"uuid",       id.uuid().toString(QUuid::WithoutBraces)},
                {"name",       id.displayName()},
                {"systemInfo", SystemInfo::instance().toJsonForHandshake(m_externalIp)},
                {"avatarHash", ownAvatarHash},
            };
            // Отправляем HANDSHAKE_ACK (напрямую или через ретранслятор)
            if (conn.socket) {
                conn.socket->write(
                    QJsonDocument(ack).toJson(QJsonDocument::Compact) + '\n');
            } else {
                // Relay-пир: регистрируем и шлём через ретранслятор
                m_relayPeers.insert(peerUuid);
                sendViaRelay(peerUuid, ack);
            }

            log(QString("Принято подключение от %1 (серверный порт: %2)")
                .arg(conn.name).arg(conn.serverPort));

            // Keepalive только для прямых TCP-подключений
            if (conn.socket)
                startKeepalive(peerUuid);

            emit peerConnected(peerUuid, conn.name);
            emit connectionStateChanged(peerUuid, ConnectionState::Connected);

            // Отправляем накопленные в очереди сообщения
            drainMessageQueue(peerUuid);
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
            auto* socket = it.value().socket;
            if (socket) {
                socket->write(QJsonDocument(ack).toJson(QJsonDocument::Compact) + '\n');
                // После разрыва — освобождаем сокет. Он удалён из m_pending,
                // поэтому lambda в onNewConnection не вызовет deleteLater повторно.
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                QTimer::singleShot(500, socket, &QTcpSocket::disconnectFromHost);
            } else {
                // Relay-пир: шлём отклонение через ретранслятор
                sendViaRelay(peerUuid, ack);
            }
            m_pending.erase(it);
            return;
        }
    }
}

bool NetworkManager::isOnline(const QUuid& uuid) const {
    return m_peers.contains(uuid);
}

// ── Переподключение с экспоненциальным откатом ─────────────────────────────

int NetworkManager::calculateBackoffMs(int attempts) const {
    // Экспоненциальный откат: 1s, 2s, 4s, 8s, 16s, 30s (max)
    const int baseDelay = 1000;
    const int delay = baseDelay * static_cast<int>(qPow(2, qMin(attempts, 5)));
    return qMin(delay, kMaxReconnectDelay);
}

void NetworkManager::scheduleReconnect(const QUuid& uuid) {
    if (!m_reconnectInfo.contains(uuid)) {
        log(QString("Нет информации для переподключения к %1")
            .arg(uuid.toString(QUuid::WithoutBraces)));
        return;
    }

    // Читаем счётчик из отдельного словаря — он не зависит от m_peers
    const int attempts = m_reconnectAttempts.value(uuid, 0);

    if (attempts >= kMaxReconnectAttempts) {
        log(QString("Превышен лимит попыток (%1) для %2")
            .arg(attempts).arg(m_reconnectInfo[uuid].name), true);
        emit error(tr("Не удалось переподключиться после %1 попыток").arg(attempts));
        m_reconnectInfo.remove(uuid);
        m_reconnectAttempts.remove(uuid);
        m_messageQueues.remove(uuid);
        return;
    }

    const int delayMs = calculateBackoffMs(attempts);
    log(QString("Переподключение к %1 через %2мс (попытка %3/%4)")
        .arg(m_reconnectInfo[uuid].name).arg(delayMs)
        .arg(attempts + 1).arg(kMaxReconnectAttempts), true);

    emit connectionStateChanged(uuid, ConnectionState::Reconnecting);

    // Останавливаем предыдущий таймер чтобы не накапливать
    if (m_reconnectTimers.contains(uuid)) {
        m_reconnectTimers[uuid]->stop();
        m_reconnectTimers[uuid]->deleteLater();
        m_reconnectTimers.remove(uuid);
    }

    auto* timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, uuid]() {
        m_reconnectTimers.remove(uuid);
        attemptReconnect(uuid);
    });
    timer->start(delayMs);
    m_reconnectTimers[uuid] = timer;
}

void NetworkManager::attemptReconnect(const QUuid& uuid) {
    if (!m_reconnectInfo.contains(uuid)) return;

    const auto& info = m_reconnectInfo[uuid];

    // Увеличиваем счётчик в отдельном словаре (m_peers может быть пуст)
    m_reconnectAttempts[uuid] = m_reconnectAttempts.value(uuid, 0) + 1;

    log(QString("Попытка переподключения к %1 (%2:%3), попытка #%4")
        .arg(info.name, info.ip).arg(info.port)
        .arg(m_reconnectAttempts[uuid]), true);

    PeerInfo peer;
    peer.uuid = uuid;
    peer.name = info.name;
    peer.ip   = info.ip;
    peer.port = info.port;

    connectToPeer(peer);
}

void NetworkManager::resetReconnectState(const QUuid& uuid) {
    // Сбрасываем счётчик и останавливаем таймер из отдельных словарей
    m_reconnectAttempts.remove(uuid);

    if (m_reconnectTimers.contains(uuid)) {
        m_reconnectTimers[uuid]->stop();
        m_reconnectTimers[uuid]->deleteLater();
        m_reconnectTimers.remove(uuid);
    }

    // Сбрасываем устаревшие поля в PeerConnection если пир есть
    if (m_peers.contains(uuid)) {
        auto& peer = m_peers[uuid];
        peer.reconnectAttempts = 0;
        if (peer.reconnectTimer) {
            peer.reconnectTimer->stop();
            peer.reconnectTimer->deleteLater();
            peer.reconnectTimer = nullptr;
        }
    }
}

// ── Keepalive (PING/PONG) ───────────────────────────────────────────────────

void NetworkManager::startKeepalive(const QUuid& uuid) {
    if (!m_peers.contains(uuid)) return;

    auto& peer = m_peers[uuid];

    // Создаём таймер PING если его нет
    if (!peer.pingTimer) {
        peer.pingTimer = new QTimer(this);
        connect(peer.pingTimer, &QTimer::timeout, this, [this, uuid]() {
            sendPing(uuid);
        });
    }

    peer.pingTimer->start(kPingInterval);
    log(QString("Keepalive started for %1").arg(peer.name));

    // Первый PING сразу после установки keepalive — задержка появится в профиле
    // через 1-2 секунды, а не через kPingInterval (30 секунд).
    QTimer::singleShot(0, this, [this, uuid]() { sendPing(uuid); });
}

void NetworkManager::stopKeepalive(const QUuid& uuid) {
    if (!m_peers.contains(uuid)) return;

    auto& peer = m_peers[uuid];
    if (peer.pingTimer) {
        peer.pingTimer->stop();
        peer.pingTimer->deleteLater();
        peer.pingTimer = nullptr;
    }
    // Останавливаем таймер ожидания PONG если он запущен
    if (peer.pongTimeoutTimer) {
        peer.pongTimeoutTimer->stop();
        peer.pongTimeoutTimer->deleteLater();
        peer.pongTimeoutTimer = nullptr;
    }
    peer.awaitingPong = false;
}

void NetworkManager::sendPing(const QUuid& uuid) {
    if (!m_peers.contains(uuid)) return;

    auto& peer = m_peers[uuid];

    // Если уже ждём PONG — не отправляем дублирующий PING.
    // Таймаут обрабатывается отдельным pongTimeoutTimer.
    if (peer.awaitingPong) {
        log(QString("PING к %1 пропущен — ожидаем PONG").arg(peer.name));
        return;
    }

    if (!peer.socket || peer.socket->state() != QAbstractSocket::ConnectedState) {
        log(QString("Нет активного сокета для PING к %1").arg(peer.name));
        return;
    }

    const QJsonObject ping{
        {"type", "PING"},
        {"ts", QDateTime::currentMSecsSinceEpoch()}
    };

    peer.socket->write(QJsonDocument(ping).toJson(QJsonDocument::Compact) + '\n');
    peer.awaitingPong = true;
    peer.pingStopwatch.start();
    log(QString("PING отправлен %1").arg(peer.name));

    // Запускаем отдельный таймер ожидания PONG.
    // Если PONG не придёт за kPongTimeout мс — соединение считается мёртвым.
    if (!peer.pongTimeoutTimer) {
        peer.pongTimeoutTimer = new QTimer(this);
        peer.pongTimeoutTimer->setSingleShot(true);
        connect(peer.pongTimeoutTimer, &QTimer::timeout, this, [this, uuid]() {
            if (!m_peers.contains(uuid)) return;
            auto& p = m_peers[uuid];
            if (p.awaitingPong) {
                log(QString("PONG таймаут от %1 — соединение мёртво, разрываем")
                    .arg(p.name), true);
                if (p.socket) p.socket->abort();
            }
        });
    }
    peer.pongTimeoutTimer->start(kPongTimeout);
}

void NetworkManager::handlePing(PeerConnection& peer, const QJsonObject& obj) {
    Q_UNUSED(obj);

    // Отвечаем PONG
    const QJsonObject pong{
        {"type", "PONG"},
        {"ts", QDateTime::currentMSecsSinceEpoch()}
    };

    if (peer.socket && peer.socket->state() == QAbstractSocket::ConnectedState) {
        peer.socket->write(QJsonDocument(pong).toJson(QJsonDocument::Compact) + '\n');
        log(QString("PONG sent to %1").arg(peer.name));
    }

    peer.lastActivity = QDateTime::currentDateTime();
}

void NetworkManager::handlePong(PeerConnection& peer, const QJsonObject& obj) {
    Q_UNUSED(obj);

    peer.awaitingPong = false;
    peer.lastActivity = QDateTime::currentDateTime();

    // Отменяем таймер ожидания PONG — соединение живо
    if (peer.pongTimeoutTimer) {
        peer.pongTimeoutTimer->stop();
    }

    peer.latencyMs = peer.pingStopwatch.elapsed();
    log(QString("PONG от %1 (задержка: %2мс)").arg(peer.name).arg(peer.latencyMs));
}
