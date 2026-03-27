#include "remoteshellmanager.h"
#include "network.h"
#include "../crypto/e2e.h"
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDebug>

// ── Паттерн эскалации привилегий ──────────────────────────────────────────────
// \b — граница слова: не совпадёт с "subversion", "supervisor" и т.д.
// CaseInsensitiveOption — нечувствительность к регистру (SUDO, Sudo и т.д.).
// Охватывает: sudo, su, pkexec, doas, runas, gsudo,
//             PowerShell Start-Process -Verb RunAs.
const QRegularExpression RemoteShellManager::kPrivEscPattern(
    R"(\b(sudo|su|pkexec|doas|runas|gsudo)\b|Start-Process\s+\S*\s+-Verb\s+RunAs)",
    QRegularExpression::CaseInsensitiveOption);

// ── Конструктор / деструктор ──────────────────────────────────────────────────

RemoteShellManager::RemoteShellManager(NetworkManager* net, E2EManager* e2e,
                                        QObject* parent)
    : QObject(parent), m_net(net), m_e2e(e2e)
{}

RemoteShellManager::~RemoteShellManager() {
    // Принудительно завершаем все живые процессы при уничтожении менеджера
    for (auto& sd : m_sessions) {
        if (sd.process) {
            sd.process->kill();
            sd.process->waitForFinished(1000);
        }
    }
}

// ── Инициатор: запросить шелл ─────────────────────────────────────────────────

void RemoteShellManager::requestShell(const QUuid& peerUuid) {
    const QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    const QString peerName = m_net->getPeerInfo(peerUuid).name;

    SessionData sd;
    sd.role     = Role::Initiator;
    sd.peerUuid = peerUuid;
    sd.peerName = peerName;
    m_sessions.insert(sessionId, sd);

    m_net->sendJson(peerUuid, QJsonObject{
        {"type",      "SHELL_REQUEST"},
        {"sessionId", sessionId},
    });

    qDebug("[Shell] Запрошен шелл у %s, сессия=%s",
           qPrintable(peerUuid.toString(QUuid::WithoutBraces).left(8)),
           qPrintable(sessionId.left(8)));
}

// ── Получатель: принять запрос ────────────────────────────────────────────────

void RemoteShellManager::acceptRequest(const QString& sessionId) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) {
        qWarning("[Shell] acceptRequest: неизвестная сессия %s",
                 qPrintable(sessionId.left(8)));
        return;
    }

    m_net->sendJson(it->peerUuid, QJsonObject{
        {"type",      "SHELL_ACCEPT"},
        {"sessionId", sessionId},
    });

    spawnProcess(sessionId);

    qDebug("[Shell] Сессия %s принята, шелл-процесс запускается",
           qPrintable(sessionId.left(8)));
}

// ── Получатель: отклонить запрос ─────────────────────────────────────────────

void RemoteShellManager::rejectRequest(const QString& sessionId,
                                        const QString& reason) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return;

    m_net->sendJson(it->peerUuid, QJsonObject{
        {"type",      "SHELL_REJECT"},
        {"sessionId", sessionId},
        {"reason",    reason},
    });

    m_sessions.erase(it);

    qDebug("[Shell] Сессия %s отклонена: %s",
           qPrintable(sessionId.left(8)), qPrintable(reason));
}

// ── Инициатор: отправить ввод ─────────────────────────────────────────────────

void RemoteShellManager::sendInput(const QString& sessionId,
                                    const QByteArray& data) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end() || it->role != Role::Initiator) return;

    // ══ КРИТИЧЕСКАЯ ПРОВЕРКА БЕЗОПАСНОСТИ ════════════════════════════════════
    // Проверяем ввод ПЕРЕД отправкой: sudo, su, pkexec, doas, runas, gsudo,
    // Start-Process -Verb RunAs.
    // Обнаружение → немедленное уничтожение сессии с обоих концов.
    if (hasForbiddenPattern(data)) {
        qCritical("[Shell][SECURITY] ЭСКАЛАЦИЯ ПРИВИЛЕГИЙ! Сессия %s уничтожена. "
                  "Ввод: %.120s",
                  qPrintable(sessionId.left(8)),
                  data.constData());
        killSession(sessionId, "privilege_escalation");
        emit privilegeEscalationDetected(sessionId);
        return;
    }
    // ═════════════════════════════════════════════════════════════════════════

    sendEncrypted(it->peerUuid,
        QJsonObject{
            {"shell_type", "SHELL_INPUT"},
            {"session",    sessionId},
            {"data",       QString::fromLatin1(data.toBase64())},
        },
        "SHELL_INPUT"
    );
}

// ── Завершить сессию ──────────────────────────────────────────────────────────

void RemoteShellManager::killSession(const QString& sessionId,
                                      const QString& reason) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return;

    // Уведомляем пира о завершении
    m_net->sendJson(it->peerUuid, QJsonObject{
        {"type",      "SHELL_KILL"},
        {"sessionId", sessionId},
        {"reason",    reason},
    });

    cleanupSession(sessionId);
    emit sessionEnded(sessionId, reason);
}

// ── Обработчик незашифрованного сигналинга ────────────────────────────────────

void RemoteShellManager::handleSignaling(const QUuid& from,
                                          const QJsonObject& msg) {
    const QString type      = msg["type"].toString();
    const QString sessionId = msg["sessionId"].toString();

    if (type == "SHELL_REQUEST") {
        // Сохраняем сессию как Receiver ещё до ответа пользователя
        const QString peerName = m_net->getPeerInfo(from).name;
        SessionData sd;
        sd.role     = Role::Receiver;
        sd.peerUuid = from;
        sd.peerName = peerName;
        m_sessions.insert(sessionId, sd);

        qDebug("[Shell] Входящий запрос шелла от %s, сессия=%s",
               qPrintable(from.toString(QUuid::WithoutBraces).left(8)),
               qPrintable(sessionId.left(8)));

        emit shellRequested(from, peerName, sessionId);

    } else if (type == "SHELL_ACCEPT") {
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end() || it->role != Role::Initiator) return;

        qDebug("[Shell] Запрос шелла принят пиром, сессия=%s",
               qPrintable(sessionId.left(8)));

        emit shellAccepted(sessionId, from, it->peerName);

    } else if (type == "SHELL_REJECT") {
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) return;

        const QString reason = msg["reason"].toString("declined");
        m_sessions.erase(it);

        qDebug("[Shell] Запрос шелла отклонён: %s", qPrintable(reason));

        emit shellRejected(sessionId, reason);

    } else if (type == "SHELL_KILL") {
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) return;

        const QString reason = msg["reason"].toString("terminated");

        qDebug("[Shell] Получен SHELL_KILL: сессия=%s причина=%s",
               qPrintable(sessionId.left(8)), qPrintable(reason));

        cleanupSession(sessionId);
        emit sessionEnded(sessionId, reason);
    }
}

// ── Обработчик расшифрованных данных SHELL_DATA / SHELL_INPUT ─────────────────

void RemoteShellManager::handleDecryptedData(const QUuid& from,
                                              const QByteArray& plaintext) {
    const QJsonObject inner     = QJsonDocument::fromJson(plaintext).object();
    const QString     shellType = inner["shell_type"].toString();
    const QString     sessionId = inner["session"].toString();
    const QByteArray  data      = QByteArray::fromBase64(
        inner["data"].toString().toLatin1());

    if (shellType == "SHELL_DATA") {
        // stdout/stderr от удалённого шелла → отображаем в ShellWindow инициатора
        emit dataReceived(sessionId, data);

    } else if (shellType == "SHELL_INPUT") {
        // Команда от инициатора → записываем в stdin процесса получателя
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end() || it->role != Role::Receiver || !it->process) {
            qWarning("[Shell] SHELL_INPUT: нет активного процесса для сессии %s",
                     qPrintable(sessionId.left(8)));
            return;
        }

        // ══ ВТОРИЧНАЯ ПРОВЕРКА БЕЗОПАСНОСТИ (defence in depth) ═══════════════
        // Проверяем даже входящие данные на стороне получателя.
        // Защищает от случаев когда инициатор не обновил приложение или
        // когда пакеты модифицированы в памяти до отправки.
        if (hasForbiddenPattern(data)) {
            qCritical("[Shell][SECURITY] ЭСКАЛАЦИЯ (получатель): сессия %s уничтожена",
                      qPrintable(sessionId.left(8)));

            // Уведомляем инициатора напрямую (killSession уже отправит SHELL_KILL,
            // но сессия из хэша исчезнет — делаем это вручную до cleanupSession)
            m_net->sendJson(from, QJsonObject{
                {"type",      "SHELL_KILL"},
                {"sessionId", sessionId},
                {"reason",    "privilege_escalation"},
            });

            cleanupSession(sessionId);
            emit sessionEnded(sessionId, "privilege_escalation");
            return;
        }
        // ═════════════════════════════════════════════════════════════════════

        // Сигнал для ShellMonitor: показываем команду в мониторе получателя
        emit inputMonitored(sessionId, data);

        // Добавляем перевод строки если его нет — иначе bash не выполнит команду
        QByteArray cmd = data;
        if (!cmd.endsWith('\n')) cmd.append('\n');
        it->process->write(cmd);
    }
}

// ── Проверка на эскалацию привилегий ─────────────────────────────────────────

bool RemoteShellManager::hasForbiddenPattern(const QByteArray& input) {
    // Конвертируем в строку для поиска по регулярному выражению
    const QString str = QString::fromUtf8(input);
    return kPrivEscPattern.match(str).hasMatch();
}

// ── Отправка зашифрованных данных ─────────────────────────────────────────────

void RemoteShellManager::sendEncrypted(const QUuid& peerUuid,
                                        const QJsonObject& innerObj,
                                        const QString& outerType) {
    if (!m_e2e->hasSession(peerUuid)) {
        qWarning("[Shell] Нет E2E-сессии для %s — данные шелла не отправлены",
                 qPrintable(peerUuid.toString(QUuid::WithoutBraces).left(8)));
        return;
    }

    // Шифруем через Double Ratchet (тот же механизм что и чат-сообщения)
    const QByteArray innerJson =
        QJsonDocument(innerObj).toJson(QJsonDocument::Compact);
    QJsonObject env = m_e2e->encrypt(peerUuid, innerJson);
    if (env.isEmpty()) return;

    // Подменяем тип конверта: decrypt() игнорирует поле type,
    // используя только dh/n/pn/ct/nonce/tag — подмена безопасна.
    env["type"] = outerType;
    m_net->sendJson(peerUuid, env);
}

// ── Запуск шелл-процесса ──────────────────────────────────────────────────────

void RemoteShellManager::spawnProcess(const QString& sessionId) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return;

    SessionData& sd = it.value();

    auto* proc = new QProcess(this);
    // Объединяем stdout и stderr в один канал: единый поток вывода
    proc->setProcessChannelMode(QProcess::MergedChannels);

    // Запускаем шелл в зависимости от платформы.
    // Процесс наследует права текущего пользователя — не root, не admin.
#ifdef Q_OS_WIN
    // PowerShell без elevation: -NoExit держит процесс живым,
    // -NoLogo убирает заголовок версии, -NonInteractive отключает интерактивные запросы.
    proc->start("powershell.exe", {"-NoLogo", "-NoExit", "-NonInteractive"});
#else
    // Предпочитаем bash → zsh → sh (обычные права пользователя)
    QString shell = "/bin/bash";
    if (!QFile::exists(shell)) {
        shell = "/bin/zsh";
        if (!QFile::exists(shell))
            shell = "/bin/sh";
    }
    // Без флага -i: не запрашиваем TTY (нет терминала, SIGTTOU/SIGTTIN не нужны)
    proc->start(shell, {});
#endif

    if (!proc->waitForStarted(5000)) {
        qWarning("[Shell] Не удалось запустить шелл: %s",
                 qPrintable(proc->errorString()));
        proc->deleteLater();
        // Уведомляем инициатора об ошибке запуска
        m_net->sendJson(sd.peerUuid, QJsonObject{
            {"type",      "SHELL_KILL"},
            {"sessionId", sessionId},
            {"reason",    "spawn_failed"},
        });
        m_sessions.erase(it);
        emit sessionEnded(sessionId, "spawn_failed");
        return;
    }

    sd.process = proc;

    const QUuid   peerUuid  = sd.peerUuid;

    // stdout/stderr → base64 → E2E-шифрование → SHELL_DATA → инициатор
    connect(proc, &QProcess::readyReadStandardOutput,
            this, [this, sessionId, peerUuid, proc]() {
        const QByteArray out = proc->readAllStandardOutput();
        if (out.isEmpty()) return;

        sendEncrypted(peerUuid,
            QJsonObject{
                {"shell_type", "SHELL_DATA"},
                {"session",    sessionId},
                {"data",       QString::fromLatin1(out.toBase64())},
            },
            "SHELL_DATA"
        );
    });

    // Процесс завершился сам (exit, Ctrl+D) → уведомляем обе стороны
    connect(proc, &QProcess::finished,
            this, [this, sessionId, peerUuid](int /*code*/, QProcess::ExitStatus) {
        auto fit = m_sessions.find(sessionId);
        if (fit == m_sessions.end()) return;   // уже очищено

        qDebug("[Shell] Шелл-процесс завершился, сессия=%s",
               qPrintable(sessionId.left(8)));

        m_net->sendJson(peerUuid, QJsonObject{
            {"type",      "SHELL_KILL"},
            {"sessionId", sessionId},
            {"reason",    "process_exited"},
        });
        cleanupSession(sessionId);
        emit sessionEnded(sessionId, "process_exited");
    });

    qDebug("[Shell] Шелл-процесс запущен (pid=%lld), сессия=%s",
           static_cast<long long>(proc->processId()),
           qPrintable(sessionId.left(8)));
}

// ── Очистка сессии ────────────────────────────────────────────────────────────

void RemoteShellManager::cleanupSession(const QString& sessionId) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return;

    if (it->process) {
        // Отключаем сигналы ДО kill() — предотвращаем повторный вызов finished/cleanup
        it->process->disconnect(this);
        it->process->kill();
        it->process->waitForFinished(2000);
        it->process->deleteLater();
        it->process = nullptr;
    }

    m_sessions.erase(it);
}
