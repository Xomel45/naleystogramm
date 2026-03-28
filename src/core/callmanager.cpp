#include "callmanager.h"
#include "network.h"
#include "../crypto/e2e.h"
#include "sessionmanager.h"
#include "identity.h"
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QUdpSocket>
#include <QDebug>
#include <openssl/rand.h>

// Таймаут ожидания CALL_ACCEPT от вызываемого пира (мс)
static constexpr int kCallTimeoutMs = 30000;

// ── Конструктор / деструктор ─────────────────────────────────────────────────

CallManager::CallManager(NetworkManager* net, E2EManager* e2e, QObject* parent)
    : QObject(parent), m_net(net), m_e2e(e2e)
{
    m_media = new MediaEngine(this);
    connect(m_media, &MediaEngine::mediaError, this, &CallManager::callError);
}

CallManager::~CallManager() {
    endCall();
}

// ── setState ─────────────────────────────────────────────────────────────────

void CallManager::setState(CallState s) {
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(s);
}

// ── resetState ───────────────────────────────────────────────────────────────

void CallManager::resetState() {
    if (m_callTimeout) {
        m_callTimeout->stop();
        m_callTimeout->deleteLater();
        m_callTimeout = nullptr;
    }
    m_callId.clear();
    m_peerUuid = QUuid{};
    m_peerIp   = QHostAddress{};
    m_pendingCallerUdpPort = 0;
    m_pendingMediaSalt.clear();
    setState(CallState::Idle);
}

// ── initiateCall ─────────────────────────────────────────────────────────────

void CallManager::initiateCall(const QUuid& peerUuid, const QHostAddress& peerIp) {
    if (m_state != CallState::Idle) {
        emit callError("Уже идёт звонок");
        return;
    }
    if (!m_e2e->hasSession(peerUuid)) {
        emit callError("Нет E2E сессии с пиром — невозможно выполнить звонок");
        return;
    }

    // Привязываем UDP сокет заранее, чтобы получить локальный порт
    // MediaEngine откроет сокет при startCall — нам нужен порт ДО этого.
    // Обходной путь: создаём временный QUdpSocket для резервации порта.
    // Проще: startCall открывает сокет с bind(0), порт станет известен сразу.
    // Поэтому создадим MediaEngine, запустим сокет в режиме «только bind»
    // и потом дошлём CALL_INVITE с реальным портом.
    //
    // Альтернатива (выбрана): MediaEngine::startCall() принимает peerUdpPort=0
    // при исходящем звонке — сокет открывается, порт виден через localUdpPort().
    // Реальный peerUdpPort приходит в CALL_ACCEPT.

    // Генерируем 8-байтовый salt для деривации медиа-ключа
    QByteArray salt(8, '\0');
    RAND_bytes(reinterpret_cast<unsigned char*>(salt.data()), 8);

    m_peerUuid = peerUuid;
    m_peerIp   = peerIp;
    m_callId   = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_pendingMediaSalt = salt;

    // Открываем UDP сокет (peerUdpPort=0 — настоящий порт придёт в CALL_ACCEPT)
    // Для этого вызываем специальный метод (только bind, без захвата/воспроизведения)
    // УПРОЩЕНИЕ: bindOnly — создаём QUdpSocket вручную здесь
    if (!m_media->startCall(QHostAddress::LocalHost, 0, QByteArray(32, '\0'))) {
        // startCall провалился (нет Opus/Multimedia) — callError уже был emitted
        resetState();
        return;
    }
    // Немедленно останавливаем медиапайплайн — будем ждать CALL_ACCEPT
    // НО сохраняем UDP сокет → нет смысла, пересоздадим при CALL_ACCEPT.
    m_media->endCall();

    // Отдельный подход: MediaEngine предоставит порт через создание временного сокета.
    // Для Phase 1 используем простую схему: порт = 0 в CALL_INVITE, потом при
    // CALL_ACCEPT мы стартуем настоящий звонок и сообщаем свой порт в CALL_ACCEPT_ACK.
    //
    // РЕАЛЬНАЯ РЕАЛИЗАЦИЯ PHASE 1:
    // Caller привязывает UDP сокет → берёт порт → отправляет в CALL_INVITE.
    // При CALL_ACCEPT: настраивает peerUdpPort → начинает отправку.
    //
    // Для этого добавляем вспомогательный метод bindUdp() в MediaEngine.
    // УПРОЩЕНИЕ для Phase 1: используем фиксированный порт = localTcpPort + 1
    // (не ideal, но работает в большинстве случаев).
    //
    // ФИНАЛЬНОЕ РЕШЕНИЕ (простое и надёжное):
    // Создаём MediaEngine с peerIp=local и peerPort=0, только для bind.
    // Это инициализирует m_udpSocket и мы берём localUdpPort().
    // Потом при CALL_ACCEPT вызываем startCall уже с реальным peerPort.

    // Создаём QUdpSocket вручную чтобы узнать доступный порт
    QUdpSocket* tmpSock = new QUdpSocket(this);
    if (!tmpSock->bind(QHostAddress::Any, 0)) {
        emit callError("Не удалось открыть UDP порт для звонка");
        delete tmpSock;
        resetState();
        return;
    }
    const quint16 localUdpPort = tmpSock->localPort();
    tmpSock->close();
    delete tmpSock;

    setState(CallState::Calling);

    // Таймаут: если через 30 с не получим CALL_ACCEPT — отменяем
    m_callTimeout = new QTimer(this);
    m_callTimeout->setSingleShot(true);
    m_callTimeout->setInterval(kCallTimeoutMs);
    connect(m_callTimeout, &QTimer::timeout, this, [this]() {
        qDebug("[CallManager] Таймаут ожидания CALL_ACCEPT");
        emit callRejected(m_peerUuid, "timeout");
        resetState();
    });
    m_callTimeout->start();

    // Отправляем CALL_INVITE через TCP
    QJsonObject invite;
    invite["type"]         = "CALL_INVITE";
    invite["callId"]       = m_callId;
    invite["udpPort"]      = static_cast<int>(localUdpPort);
    invite["codecs"]       = QJsonArray{"opus"};
    invite["mediaKeySalt"] = QString::fromLatin1(salt.toHex());
    m_net->sendJson(peerUuid, invite);

    qDebug("[CallManager] CALL_INVITE отправлен, callId=%s, udpPort=%d",
           qPrintable(m_callId), localUdpPort);
}

// ── acceptCall ────────────────────────────────────────────────────────────────

void CallManager::acceptCall(const QString& callId) {
    if (m_state != CallState::Ringing || m_callId != callId) {
        emit callError("Нет входящего звонка с callId: " + callId);
        return;
    }

    startMedia(m_peerIp, m_pendingCallerUdpPort, m_callId, m_pendingMediaSalt);
    if (!m_media->isInCall()) return; // startMedia уже emitted callError

    // Сообщаем наш UDP порт вызывающей стороне
    QJsonObject accept;
    accept["type"]    = "CALL_ACCEPT";
    accept["callId"]  = m_callId;
    accept["udpPort"] = static_cast<int>(m_media->localUdpPort());
    m_net->sendJson(m_peerUuid, accept);

    setState(CallState::InCall);
    emit callAccepted(m_peerUuid);

    qDebug("[CallManager] Звонок принят, наш UDP порт=%d", m_media->localUdpPort());
}

// ── rejectCall ────────────────────────────────────────────────────────────────

void CallManager::rejectCall(const QString& callId, const QString& reason) {
    if (m_state != CallState::Ringing || m_callId != callId) return;

    QJsonObject reject;
    reject["type"]   = "CALL_REJECT";
    reject["callId"] = callId;
    reject["reason"] = reason;
    m_net->sendJson(m_peerUuid, reject);

    qDebug("[CallManager] Звонок отклонён: %s", qPrintable(reason));
    resetState();
}

// ── endCall ───────────────────────────────────────────────────────────────────

void CallManager::endCall() {
    if (m_state == CallState::Idle) return;

    const QUuid peer = m_peerUuid;
    const bool wasActive = (m_state == CallState::InCall ||
                            m_state == CallState::Calling ||
                            m_state == CallState::Ringing);

    if (wasActive && !m_callId.isEmpty() && !m_peerUuid.isNull()) {
        QJsonObject end;
        end["type"]   = "CALL_END";
        end["callId"] = m_callId;
        m_net->sendJson(m_peerUuid, end);
    }

    m_media->endCall();
    setState(CallState::Ended);
    if (!peer.isNull()) emit callEnded(peer);

    // Переход Ended → Idle через 1 с (чтобы UI успел показать "Звонок завершён")
    QTimer::singleShot(1000, this, [this]() { resetState(); });
}

// ── handleSignaling ────────────────────────────────────────────────────────────

void CallManager::handleSignaling(const QUuid& from, const QJsonObject& msg) {
    const QString type   = msg["type"].toString();
    const QString callId = msg["callId"].toString();

    if (type == "CALL_INVITE") {
        if (m_state != CallState::Idle) {
            // Уже в звонке — отклоняем
            QJsonObject reject;
            reject["type"]   = "CALL_REJECT";
            reject["callId"] = callId;
            reject["reason"] = "busy";
            m_net->sendJson(from, reject);
            return;
        }

        const quint16 callerUdpPort = static_cast<quint16>(msg["udpPort"].toInt());
        const QByteArray salt = QByteArray::fromHex(
            msg["mediaKeySalt"].toString().toLatin1());

        // Сохраняем данные приглашения
        m_peerUuid             = from;
        m_callId               = callId;
        m_pendingCallerUdpPort = callerUdpPort;
        m_pendingMediaSalt     = salt;

        // Получаем IP пира из NetworkManager
        const auto peerInfo = m_net->getPeerInfo(from);
        m_peerIp = QHostAddress(peerInfo.ip);

        setState(CallState::Ringing);
        emit incomingCall(from, peerInfo.name, callId);

    } else if (type == "CALL_ACCEPT") {
        if (m_state != CallState::Calling || m_callId != callId) return;

        if (m_callTimeout) {
            m_callTimeout->stop();
            m_callTimeout->deleteLater();
            m_callTimeout = nullptr;
        }

        const quint16 peerUdpPort = static_cast<quint16>(msg["udpPort"].toInt());
        startMedia(m_peerIp, peerUdpPort, m_callId, m_pendingMediaSalt);
        if (!m_media->isInCall()) return;

        setState(CallState::InCall);
        emit callAccepted(m_peerUuid);

        qDebug("[CallManager] CALL_ACCEPT получен, peerUdpPort=%d", peerUdpPort);

    } else if (type == "CALL_REJECT") {
        if (m_state != CallState::Calling || m_callId != callId) return;

        const QString reason = msg["reason"].toString();
        qDebug("[CallManager] Звонок отклонён пиром: %s", qPrintable(reason));

        const QUuid peer = m_peerUuid;
        resetState();
        emit callRejected(peer, reason);

    } else if (type == "CALL_END") {
        if ((m_state != CallState::InCall && m_state != CallState::Ringing &&
             m_state != CallState::Calling) || m_callId != callId) return;

        qDebug("[CallManager] CALL_END получен от пира");
        m_media->endCall();

        const QUuid peer = m_peerUuid;
        setState(CallState::Ended);
        emit callEnded(peer);
        QTimer::singleShot(1000, this, [this]() { resetState(); });
    }
}

// ── startMedia ────────────────────────────────────────────────────────────────

void CallManager::startMedia(const QHostAddress& peerIp, quint16 peerUdpPort,
                               const QString& callId, const QByteArray& salt)
{
    if (!m_e2e->hasSession(m_peerUuid)) {
        emit callError("Нет E2E сессии — медиа не может быть запущено");
        resetState();
        return;
    }

    const QByteArray mediaKey = m_e2e->snapshotMediaKey(m_peerUuid, callId, salt);
    if (mediaKey.isEmpty()) {
        emit callError("Не удалось вывести медиа-ключ");
        resetState();
        return;
    }

    // В режиме Client-Server — включаем UDP-ретрансляцию до startCall()
    if (SessionManager::instance().portForwardingMode() == PortForwardingMode::ClientServer) {
        const QString relayIp  = SessionManager::instance().relayServerIp();
        const quint16 relayUdp = SessionManager::instance().relayUdpPort();
        const QUuid myUuid     = Identity::instance().uuid();
        m_media->enableUdpRelay(relayIp, relayUdp, myUuid, m_peerUuid);
    }

    if (!m_media->startCall(peerIp, peerUdpPort, mediaKey)) {
        // callError уже emitted через connect в конструкторе
        resetState();
        return;
    }
    qDebug("[CallManager] MediaEngine запущен, peerUdpPort=%d", peerUdpPort);
}
