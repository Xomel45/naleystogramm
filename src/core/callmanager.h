#pragma once
#include <QObject>
#include <QUuid>
#include <QHostAddress>
#include "../media/mediaengine.h"

class NetworkManager;
class E2EManager;
class QTimer;

// ── CallManager ────────────────────────────────────────────────────────────────
// Машина состояний голосового звонка + обработка сигналинга (CALL_INVITE/ACCEPT/REJECT/END).
//
// Диаграмма состояний:
//   Idle → Calling  (мы отправили CALL_INVITE)
//   Idle → Ringing  (получили CALL_INVITE от пира)
//   Calling → InCall  (получили CALL_ACCEPT)
//   Calling → Idle    (получили CALL_REJECT или таймаут 30 с)
//   Ringing → InCall  (мы отправили CALL_ACCEPT)
//   Ringing → Idle    (мы отклонили или пир отменил)
//   InCall  → Ended   (получили/отправили CALL_END или пир отключился)
//   Ended   → Idle    (автоматически через 1 с)

class CallManager : public QObject {
    Q_OBJECT
public:
    enum class CallState { Idle, Calling, Ringing, InCall, Ended };
    Q_ENUM(CallState)

    explicit CallManager(NetworkManager* net, E2EManager* e2e,
                         QObject* parent = nullptr);
    ~CallManager() override;

    // ── Исходящий звонок ──────────────────────────────────────────────────────
    // Создаёт callId, отправляет CALL_INVITE через TCP, запускает таймаут 30 с.
    void initiateCall(const QUuid& peerUuid, const QHostAddress& peerIp);

    // ── Принять входящий звонок ───────────────────────────────────────────────
    // Отправляет CALL_ACCEPT, запускает MediaEngine.
    void acceptCall(const QString& callId);

    // ── Отклонить входящий звонок ─────────────────────────────────────────────
    void rejectCall(const QString& callId, const QString& reason = "declined");

    // ── Завершить текущий звонок ──────────────────────────────────────────────
    void endCall();

    // ── Обработчик сигналинговых сообщений ───────────────────────────────────
    // Вызывается из MainWindow::onMessageReceived для CALL_* типов.
    void handleSignaling(const QUuid& from, const QJsonObject& msg);

    [[nodiscard]] CallState     state()         const { return m_state; }
    [[nodiscard]] bool          isCallActive()  const { return m_state == CallState::InCall; }
    [[nodiscard]] MediaEngine*  mediaEngine()         { return m_media; }
    [[nodiscard]] QUuid         activePeer()    const { return m_peerUuid; }

signals:
    // Входящий звонок — показать CallWindow в режиме Ringing
    void incomingCall(QUuid from, QString callerName, QString callId);
    // Наш звонок принят — показать CallWindow в режиме InCall
    void callAccepted(QUuid peer);
    // Наш звонок отклонён
    void callRejected(QUuid peer, QString reason);
    // Звонок завершён (с любой стороны)
    void callEnded(QUuid peer);
    // Ошибка (нет сессии E2E, нет Opus и т.п.)
    void callError(const QString& msg);
    // Смена состояния (для обновления UI)
    void stateChanged(CallState state);

private:
    void setState(CallState s);
    // Запустить MediaEngine после согласования параметров
    void startMedia(const QHostAddress& peerIp, quint16 peerUdpPort,
                    const QString& callId, const QByteArray& salt);
    // Очистить состояние и вернуться в Idle
    void resetState();

    NetworkManager* m_net   {nullptr};
    E2EManager*     m_e2e   {nullptr};
    MediaEngine*    m_media {nullptr};

    CallState    m_state   {CallState::Idle};
    QString      m_callId;
    QUuid        m_peerUuid;
    QHostAddress m_peerIp;

    QTimer*      m_callTimeout {nullptr};  // 30 с без CALL_ACCEPT → Idle

    // Данные входящего приглашения (ждут acceptCall/rejectCall)
    quint16      m_pendingCallerUdpPort {0};
    QByteArray   m_pendingMediaSalt;
};
