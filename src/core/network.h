#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QQueue>
#include <QSet>
#include <QUuid>
#include <QDateTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonObject>
#include "identity.h"

// Состояние подключения к пиру
enum class ConnectionState {
    Disconnected,   // Нет подключения
    Connecting,     // Попытка подключения
    Connected,      // Подключено и работает
    Reconnecting    // Переподключение после разрыва
};

// Один подключённый пир с расширенным отслеживанием состояния
struct PeerConnection {
    QUuid            uuid           {};
    QString          name           {};
    QString          ip             {};
    quint16          port           {0};
    quint16          serverPort     {0};    // Слушающий порт пира (получен из HANDSHAKE)
    QTcpSocket*      socket         {nullptr};
    QByteArray       readBuf        {};

    // Расширенные поля для надёжности подключения
    ConnectionState  state          {ConnectionState::Disconnected};
    int              reconnectAttempts {0};  // Устарело: используем m_reconnectAttempts
    QDateTime        lastActivity   {};
    QTimer*          reconnectTimer {nullptr};  // Устарело: используем m_reconnectTimers
    QTimer*          pingTimer        {nullptr};
    QTimer*          pongTimeoutTimer {nullptr}; // Таймер ожидания PONG (kPongTimeout мс)
    QElapsedTimer    pingStopwatch    {};
    bool             awaitingPong     {false};

    // Профиль пира (заполняется из HANDSHAKE)
    qint64           latencyMs        {-1};       // Последний пинг, мс (-1 = нет данных)
    QDateTime        connectedSince   {};          // Момент установки соединения
    QJsonObject      systemInfo       {};          // Системная информация пира
    QString          avatarHash       {};          // SHA-256 hex аватара пира
};

// Публичная информация о пире — безопасная копия для UI и диалогов
struct PeerPublicInfo {
    QString         name;
    QString         ip;
    quint16         serverPort     {0};
    ConnectionState state          {ConnectionState::Disconnected};
    qint64          latencyMs      {-1};
    QDateTime       connectedSince {};
    QJsonObject     systemInfo     {};
    QString         avatarHash     {};
};

class NetworkManager : public QObject {
    Q_OBJECT
public:
    explicit NetworkManager(QObject* parent = nullptr);
    ~NetworkManager();

    // Запуск сервера + обнаружение внешнего IP + попытка UPnP
    void        init();

    [[nodiscard]] QString  externalIp()  const noexcept { return m_externalIp; }
    [[nodiscard]] quint16  localPort()   const noexcept { return m_localPort; }
    [[nodiscard]] bool     upnpMapped()  const noexcept { return m_upnpMapped; }

    // Инициировать исходящее подключение
    void        connectToPeer(const PeerInfo& peer);

    // Повторная попытка пробросить порты через UPnP
    void        retryUpnp();

    // Разослать всем подключённым пирам обновлённое имя пользователя
    void        broadcastProfileUpdate(const QString& name);

    // Принять/отклонить входящее подключение
    void        acceptIncoming(const QUuid& peerUuid);
    void        rejectIncoming(const QUuid& peerUuid);

    // Отправить JSON подключённому пиру
    void        sendJson(const QUuid& peerUuid, const QJsonObject& obj);

    bool        isOnline(const QUuid& uuid) const;

    // Получить публичную информацию о пире (для диалога профиля)
    [[nodiscard]] PeerPublicInfo   getPeerInfo(const QUuid& uuid) const;

    // Получить состояние подключения к пиру
    [[nodiscard]] ConnectionState connectionState(const QUuid& uuid) const;

    // Включить/выключить подробное логирование
    void        setVerboseLogging(bool enabled);
    [[nodiscard]] bool verboseLogging() const { return m_verboseLogging; }

signals:
    void        ready(const QString& externalIp, quint16 port, bool upnpOk);
    void        externalIpDiscovered(const QString& ip);
    void        upnpMappingResult(bool ok);  // Результат UPnP (асинхронный)

    // Кто-то хочет подключиться — показать диалог подтверждения
    void        incomingRequest(QUuid peerUuid, QString peerName, QString peerIp);

    // JSON сообщение от пира
    void        messageReceived(QUuid fromUuid, QJsonObject msg);

    void        peerConnected(QUuid uuid, QString name);
    void        peerDisconnected(QUuid uuid);

    // Пир прислал PROFILE_UPDATE с новым именем
    void        contactNameUpdated(QUuid uuid, QString name);

    // Системная информация / хэш аватара пира обновлены (из HANDSHAKE)
    void        peerInfoUpdated(QUuid uuid);

    // Изменение состояния подключения (для UI)
    void        connectionStateChanged(QUuid uuid, ConnectionState state);

    // Лог-сообщение (для UI и файла)
    void        connectionLog(const QString& message);

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

    // Логирование с условной детализацией
    void        log(const QString& message, bool forceVerbose = false);

    // Переподключение с экспоненциальным откатом
    void        scheduleReconnect(const QUuid& uuid);
    void        attemptReconnect(const QUuid& uuid);
    void        resetReconnectState(const QUuid& uuid);
    int         calculateBackoffMs(int attempts) const;

    // Keepalive (PING/PONG)
    void        startKeepalive(const QUuid& uuid);
    void        stopKeepalive(const QUuid& uuid);
    void        sendPing(const QUuid& uuid);
    void        handlePing(PeerConnection& peer, const QJsonObject& obj);
    void        handlePong(PeerConnection& peer, const QJsonObject& obj);

    // Очередь сообщений: если пир временно недоступен — сообщения ждут
    void        drainMessageQueue(const QUuid& uuid);

    // Хранение данных пиров для переподключения
    struct PeerReconnectInfo {
        QString name;
        QString ip;
        quint16 port;
    };
    QMap<QUuid, PeerReconnectInfo>       m_reconnectInfo;     // Адрес для переподключения
    QMap<QUuid, int>                     m_reconnectAttempts; // Счётчики попыток
    QMap<QUuid, QTimer*>                 m_reconnectTimers;   // Таймеры (не теряются при erase)
    QMap<QUuid, QQueue<QJsonObject>>     m_messageQueues;     // Очереди сообщений по пирам

    QTcpServer*                  m_server{nullptr};
    QMap<QUuid, PeerConnection>  m_peers;       // подтверждённые подключения
    QMap<QUuid, PeerConnection>  m_pending;     // ожидание подтверждения

    QString     m_externalIp;
    quint16     m_localPort      {47821};
    // Порт, анонсируемый пирам в HANDSHAKE (= localPort в UPnP-режиме,
    // = manualPublicPort в Manual-режиме).
    quint16     m_advertisedPort {0};
    bool        m_upnpMapped{false};
    bool        m_verboseLogging{false};

    static constexpr quint16 kDefaultPort          = 47821;
    static constexpr int     kConnectionTimeout    = 10000;   // 10 секунд на подключение
    static constexpr int     kMaxReconnectDelay    = 30000;   // Макс. задержка переподключения
    static constexpr int     kPingInterval         = 30000;   // Интервал PING (30 сек)
    static constexpr int     kPongTimeout          = 10000;   // Таймаут PONG (10 сек)
    static constexpr int     kMaxReconnectAttempts = 50;      // Макс. попыток (~25 минут backoff)
    static constexpr int     kMaxQueueSize         = 100;     // Макс. сообщений в очереди пира
    // Защита от DoS: если readBuf пира превышает 16 МБ, соединение обрывается.
    // Легитимные сообщения (JSON) никогда не достигают этого размера.
    static constexpr int     kMaxBufferSize        = 16 * 1024 * 1024; // 16 МБ
};
