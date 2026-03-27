#pragma once
#include <QObject>
#include <QUuid>
#include <QHash>
#include <QJsonObject>
#include <QRegularExpression>

class NetworkManager;
class E2EManager;
class QProcess;

// ── RemoteShellManager ────────────────────────────────────────────────────────
// Управляет сессиями удалённого шелла через существующий E2E-зашифрованный канал.
//
// Протокол (незашифрованный сигналинг, аналогично CALL_*):
//   SHELL_REQUEST  → {type, sessionId}
//   SHELL_ACCEPT   ← {type, sessionId}
//   SHELL_REJECT   ← {type, sessionId, reason}
//   SHELL_KILL     ↔ {type, sessionId, reason}
//
// Данные шелла (E2E-зашифрованы через Double Ratchet, outer type = "SHELL_DATA"/"SHELL_INPUT"):
//   inner JSON: {"shell_type":"SHELL_DATA", "session":"...", "data":"<base64>"}
//   inner JSON: {"shell_type":"SHELL_INPUT","session":"...", "data":"<base64>"}
//
// КРИТИЧЕСКАЯ БЕЗОПАСНОСТЬ:
//   Перед записью в QProcess и перед отправкой ввод проверяется на паттерны
//   эскалации привилегий. Обнаружение → немедленное уничтожение сессии.

class RemoteShellManager : public QObject {
    Q_OBJECT
public:
    // Роль текущего узла в конкретной сессии
    enum class Role { Initiator, Receiver };

    explicit RemoteShellManager(NetworkManager* net, E2EManager* e2e,
                                QObject* parent = nullptr);
    ~RemoteShellManager() override;

    // Инициатор: запросить шелл-сессию у пира
    void requestShell(const QUuid& peerUuid);

    // Получатель: принять входящий запрос и запустить процесс
    void acceptRequest(const QString& sessionId);

    // Получатель: отклонить входящий запрос
    void rejectRequest(const QString& sessionId,
                       const QString& reason = "declined");

    // Инициатор: отправить данные в stdin удалённого процесса.
    // Проходит проверку на эскалацию привилегий.
    void sendInput(const QString& sessionId, const QByteArray& data);

    // Любая сторона: завершить сессию
    void killSession(const QString& sessionId,
                     const QString& reason = "terminated");

    // Обработчик незашифрованного сигналинга SHELL_REQUEST/ACCEPT/REJECT/KILL.
    // Вызывается из MainWindow::onMessageReceived.
    void handleSignaling(const QUuid& from, const QJsonObject& msg);

    // Обработчик расшифрованных данных шелла (outer type SHELL_DATA / SHELL_INPUT).
    // plaintext = JSON: {"shell_type":"...", "session":"...", "data":"<base64>"}
    void handleDecryptedData(const QUuid& from, const QByteArray& plaintext);

signals:
    // Входящий запрос шелла — MainWindow показывает диалог подтверждения
    void shellRequested(QUuid from, QString peerName, QString sessionId);

    // Инициатор: запрос принят — MainWindow создаёт ShellWindow
    void shellAccepted(QString sessionId, QUuid peerUuid, QString peerName);

    // Инициатор: запрос отклонён
    void shellRejected(QString sessionId, QString reason);

    // Данные stdout/stderr от удалённого шелла — для ShellWindow инициатора
    void dataReceived(QString sessionId, QByteArray data);

    // Команда от инициатора — для ShellMonitor получателя (отображение)
    void inputMonitored(QString sessionId, QByteArray data);

    // Сессия завершена (с любой стороны)
    void sessionEnded(QString sessionId, QString reason);

    // Обнаружена попытка эскалации привилегий — сессия уже уничтожена
    void privilegeEscalationDetected(QString sessionId);

private:
    // Проверить байты на наличие команд эскалации привилегий.
    // Возвращает true если обнаружен запрещённый паттерн.
    static bool hasForbiddenPattern(const QByteArray& input);

    // Зашифровать innerObj через Double Ratchet и отправить с внешним типом outerType.
    // decrypt() игнорирует поле type — использует только dh/n/pn/ct/nonce/tag.
    void sendEncrypted(const QUuid& peerUuid,
                       const QJsonObject& innerObj,
                       const QString& outerType);

    // Запустить шелл-процесс на стороне получателя
    void spawnProcess(const QString& sessionId);

    // Освободить ресурсы сессии (убить процесс, удалить из хэша)
    void cleanupSession(const QString& sessionId);

    struct SessionData {
        Role      role;
        QUuid     peerUuid;
        QString   peerName;
        QProcess* process {nullptr};   // только у Receiver
    };

    NetworkManager* m_net {nullptr};
    E2EManager*     m_e2e {nullptr};

    QHash<QString, SessionData> m_sessions;   // sessionId → данные сессии

    // Регулярное выражение для обнаружения команд эскалации привилегий.
    // \b — граница слова: sudo/su/pkexec/doas/runas/gsudo как самостоятельные слова.
    // Дополнительно: PowerShell Start-Process -Verb RunAs.
    static const QRegularExpression kPrivEscPattern;
};
