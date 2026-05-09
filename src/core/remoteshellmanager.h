#pragma once
#include <QObject>
#include <QUuid>
#include <QHash>
#include <QList>
#include <QJsonObject>
#include <QRegularExpression>

class NetworkManager;
class E2EManager;
class QProcess;
class QTimer;

// ── RemoteShellManager ────────────────────────────────────────────────────────
// Управляет сессиями удалённого шелла через существующий E2E-зашифрованный канал.
//
// Протокол (незашифрованный сигналинг):
//   Initiator → Receiver:  SHELL_REQUEST           {type, sessionId}
//   Receiver  → Initiator: SHELL_CHALLENGE          {type, sessionId}
//   Initiator → Receiver:  SHELL_CHALLENGE_RESPONSE {type, sessionId, password}
//   Receiver  → Initiator: SHELL_ACCEPT             {type, sessionId}
//   Receiver  → Initiator: SHELL_REJECT             {type, sessionId, reason}
//   Either    → Either:    SHELL_KILL               {type, sessionId, reason}
//
// При получении SHELL_REQUEST получатель автоматически генерирует одноразовый
// пароль (OTP) и показывает его в своём окне. Инициатор должен ввести пароль,
// который ему сообщает получатель вне полосы (голосом, чатом). Только при
// совпадении — запускается шелл.
//
// Данные шелла (E2E-зашифрованы через Double Ratchet):
//   SHELL_DATA  ← {shell_type, session, data:<base64>}   stdout/stderr получателя
//   SHELL_INPUT → {shell_type, session, data:<base64>}   stdin инициатора

class RemoteShellManager : public QObject {
    Q_OBJECT
public:
    enum class Role { Initiator, Receiver };

    explicit RemoteShellManager(NetworkManager* net, E2EManager* e2e,
                                QObject* parent = nullptr);
    ~RemoteShellManager() override;

    // Инициатор: запросить шелл-сессию у пира.
    // Отказывает немедленно если уже есть активная сессия.
    void requestShell(const QUuid& peerUuid);

    // Получатель: явно отклонить запрос (нажата кнопка «Отклонить» в UI).
    void rejectRequest(const QString& sessionId,
                       const QString& reason = "declined");

    // Инициатор: ответить на OTP-запрос.
    // Вызывается после того как пользователь ввёл пароль в диалоге.
    void respondToChallenge(const QString& sessionId, const QString& password);

    // Инициатор: отправить данные в stdin удалённого процесса.
    void sendInput(const QString& sessionId, const QByteArray& data);

    // Любая сторона: завершить сессию
    void killSession(const QString& sessionId,
                     const QString& reason = "terminated");

    // Обработчик незашифрованного сигналинга (все SHELL_* типы).
    // Вызывается из MainWindow::onMessageReceived.
    void handleSignaling(const QUuid& from, const QJsonObject& msg);

    // Обработчик расшифрованных данных шелла (outer type SHELL_DATA / SHELL_INPUT).
    void handleDecryptedData(const QUuid& from, const QByteArray& plaintext);

signals:
    // Receiver: входящий запрос — показать OTP в окне получателя
    void shellChallengeGenerated(QString sessionId, QUuid peerUuid,
                                 QString peerName, QString otp);

    // Initiator: нужно ввести пароль (который виден у получателя)
    void shellPasswordRequired(QString sessionId, QUuid peerUuid, QString peerName);

    // Initiator: пароль принят, шелл запущен
    void shellAccepted(QString sessionId, QUuid peerUuid, QString peerName);

    // Receiver: шелл-процесс успешно запущен (OTP-диалог можно закрыть, монитор — показать)
    void shellSessionStarted(QString sessionId);

    // Initiator: запрос отклонён (неверный пароль или явный отказ)
    void shellRejected(QString sessionId, QString reason);

    // Данные stdout/stderr от удалённого шелла → ShellWindow инициатора
    void dataReceived(QString sessionId, QByteArray data);

    // Команда от инициатора → ShellMonitor получателя (для отображения)
    void inputMonitored(QString sessionId, QByteArray data);

    // Сессия завершена (с любой стороны)
    void sessionEnded(QString sessionId, QString reason);

    // Обнаружена попытка эскалации привилегий — сессия уже уничтожена
    void privilegeEscalationDetected(QString sessionId);

private:
    static bool    hasForbiddenPattern(const QByteArray& input);
    static QString generateOtp();

    void sendEncrypted(const QUuid& peerUuid,
                       const QJsonObject& innerObj,
                       const QString& outerType);

    void spawnProcess(const QString& sessionId);
    void cleanupSession(const QString& sessionId);
    void resetInactivityTimer(const QString& sessionId);

    struct SessionData {
        Role      role;
        QUuid     peerUuid;
        QString   peerName;
        QString   otp;                     // OTP (только у Receiver, до верификации)
        QProcess* process       {nullptr}; // только у Receiver после принятия
        QTimer*   inactivityTimer {nullptr};
    };

    NetworkManager* m_net {nullptr};
    E2EManager*     m_e2e {nullptr};

    QHash<QString, SessionData> m_sessions;

    static constexpr int kMaxConcurrentSessions = 1;
    static constexpr int kSessionTimeoutMs      = 30 * 60 * 1000; // 30 минут
    static constexpr int kOtpLength             = 6;
};
