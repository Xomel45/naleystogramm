#include "remoteshellmanager.h"
#include "network.h"
#include "../crypto/e2e.h"
#include "../crypto/qt_bridge.h"
#include <QProcess>
#include <QTimer>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDebug>

// Migration helpers — Phase 6 will migrate these call sites fully
static inline std::string quid2s(const QUuid& u) {
    return u.toString(QUuid::WithoutBraces).toStdString();
}
static inline void netSend(NetworkManager* net, const QUuid& u, const QJsonObject& o) {
    net->sendFrame(quid2s(u), bridge::fromQJsonObj(o));
}

// ── Конструктор / деструктор ──────────────────────────────────────────────────

RemoteShellManager::RemoteShellManager(NetworkManager* net, E2EManager* e2e,
                                        QObject* parent)
    : QObject(parent), m_net(net), m_e2e(e2e)
{}

RemoteShellManager::~RemoteShellManager() {
    for (auto& sd : m_sessions) {
        if (sd.inactivityTimer) { sd.inactivityTimer->stop(); }
        if (sd.process) {
            sd.process->kill();
            sd.process->waitForFinished(1000);
        }
    }
}

// ── Инициатор: запросить шелл ─────────────────────────────────────────────────

void RemoteShellManager::requestShell(const QUuid& peerUuid) {
    // Допускается не более одной одновременной сессии
    if (m_sessions.size() >= kMaxConcurrentSessions) {
        qWarning("[Shell] Отказ: уже есть активная сессия (максимум %d)",
                 kMaxConcurrentSessions);
        emit shellRejected(QString{}, "max_sessions");
        return;
    }

    const QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString peerName  = QString::fromStdString(m_net->getPeerInfo(quid2s(peerUuid)).name);

    SessionData sd;
    sd.role     = Role::Initiator;
    sd.peerUuid = peerUuid;
    sd.peerName = peerName;
    m_sessions.insert(sessionId, sd);

    netSend(m_net, peerUuid, QJsonObject{
        {"type",      "SHELL_REQUEST"},
        {"sessionId", sessionId},
    });

    qDebug("[Shell] Запрошен шелл у %s, сессия=%s",
           qPrintable(peerUuid.toString(QUuid::WithoutBraces).left(8)),
           qPrintable(sessionId.left(8)));
}

// ── Получатель: явно отклонить запрос ────────────────────────────────────────

void RemoteShellManager::rejectRequest(const QString& sessionId,
                                        const QString& reason) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return;

    netSend(m_net, it->peerUuid, QJsonObject{
        {"type",      "SHELL_REJECT"},
        {"sessionId", sessionId},
        {"reason",    reason},
    });

    cleanupSession(sessionId);

    qDebug("[Shell] Сессия %s отклонена: %s",
           qPrintable(sessionId.left(8)), qPrintable(reason));
}

// ── Инициатор: ответить на OTP-запрос ────────────────────────────────────────

void RemoteShellManager::respondToChallenge(const QString& sessionId,
                                             const QString& password) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end() || it->role != Role::Initiator) return;

    netSend(m_net, it->peerUuid, QJsonObject{
        {"type",      "SHELL_CHALLENGE_RESPONSE"},
        {"sessionId", sessionId},
        {"password",  password},
    });

    qDebug("[Shell] Отправлен ответ на OTP-запрос, сессия=%s",
           qPrintable(sessionId.left(8)));
}

// ── Инициатор: отправить ввод ─────────────────────────────────────────────────

void RemoteShellManager::sendInput(const QString& sessionId,
                                    const QByteArray& data) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end() || it->role != Role::Initiator) return;

    // ══ КРИТИЧЕСКАЯ ПРОВЕРКА БЕЗОПАСНОСТИ ════════════════════════════════════
    if (hasForbiddenPattern(data)) {
        qCritical("[Shell][SECURITY] ЭСКАЛАЦИЯ ПРИВИЛЕГИЙ! Сессия %s уничтожена.",
                  qPrintable(sessionId.left(8)));
        killSession(sessionId, "privilege_escalation");
        emit privilegeEscalationDetected(sessionId);
        return;
    }
    // ═════════════════════════════════════════════════════════════════════════

    resetInactivityTimer(sessionId);

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

    netSend(m_net, it->peerUuid, QJsonObject{
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

    // ── SHELL_REQUEST: от инициатора к получателю ─────────────────────────────
    if (type == "SHELL_REQUEST") {
        // Не допускаем более одной одновременной сессии
        if (m_sessions.size() >= kMaxConcurrentSessions) {
            qWarning("[Shell] Входящий SHELL_REQUEST отклонён: занято (сессий=%zu)",
                     static_cast<std::size_t>(m_sessions.size()));
            netSend(m_net, from, QJsonObject{
                {"type",      "SHELL_REJECT"},
                {"sessionId", sessionId},
                {"reason",    "busy"},
            });
            return;
        }

        const QString peerName = QString::fromStdString(m_net->getPeerInfo(quid2s(from)).name);
        const QString otp      = generateOtp();

        SessionData sd;
        sd.role     = Role::Receiver;
        sd.peerUuid = from;
        sd.peerName = peerName;
        sd.otp      = otp;
        m_sessions.insert(sessionId, sd);

        // Сообщаем инициатору что ждём ответа на OTP-запрос
        netSend(m_net, from, QJsonObject{
            {"type",      "SHELL_CHALLENGE"},
            {"sessionId", sessionId},
        });

        qDebug("[Shell] Входящий запрос от %s, сессия=%s — OTP сгенерирован",
               qPrintable(from.toString(QUuid::WithoutBraces).left(8)),
               qPrintable(sessionId.left(8)));

        // Показываем OTP в окне получателя — пользователь сообщает его инициатору
        emit shellChallengeGenerated(sessionId, from, peerName, otp);

    // ── SHELL_CHALLENGE: от получателя к инициатору ───────────────────────────
    } else if (type == "SHELL_CHALLENGE") {
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end() || it->role != Role::Initiator) return;

        qDebug("[Shell] Получен OTP-запрос, сессия=%s — ожидаем ввод пароля",
               qPrintable(sessionId.left(8)));

        // Просим инициатора ввести пароль, который виден у получателя
        emit shellPasswordRequired(sessionId, from, it->peerName);

    // ── SHELL_CHALLENGE_RESPONSE: от инициатора к получателю ─────────────────
    } else if (type == "SHELL_CHALLENGE_RESPONSE") {
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end() || it->role != Role::Receiver) return;

        const QString entered = msg["password"].toString();

        if (entered.isEmpty() || entered != it->otp) {
            qWarning("[Shell][SECURITY] Неверный OTP для сессии %s — шелл отклонён",
                     qPrintable(sessionId.left(8)));

            netSend(m_net, from, QJsonObject{
                {"type",      "SHELL_REJECT"},
                {"sessionId", sessionId},
                {"reason",    "wrong_password"},
            });

            cleanupSession(sessionId);
            return;
        }

        // Пароль верный — запускаем шелл и подтверждаем инициатору
        it->otp.clear(); // одноразовый: сразу обнуляем

        netSend(m_net, from, QJsonObject{
            {"type",      "SHELL_ACCEPT"},
            {"sessionId", sessionId},
        });

        spawnProcess(sessionId);

        qDebug("[Shell] OTP верный, сессия=%s — шелл запускается",
               qPrintable(sessionId.left(8)));

    // ── SHELL_ACCEPT: от получателя к инициатору ─────────────────────────────
    } else if (type == "SHELL_ACCEPT") {
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end() || it->role != Role::Initiator) return;

        resetInactivityTimer(sessionId);

        qDebug("[Shell] Шелл принят пиром, сессия=%s",
               qPrintable(sessionId.left(8)));

        emit shellAccepted(sessionId, from, it->peerName);

    // ── SHELL_REJECT: от получателя к инициатору ─────────────────────────────
    } else if (type == "SHELL_REJECT") {
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) return;

        const QString reason = msg["reason"].toString("declined");
        cleanupSession(sessionId);

        qDebug("[Shell] Запрос шелла отклонён: %s", qPrintable(reason));

        emit shellRejected(sessionId, reason);

    // ── SHELL_KILL: завершение с любой стороны ────────────────────────────────
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
        resetInactivityTimer(sessionId);
        emit dataReceived(sessionId, data);

    } else if (shellType == "SHELL_INPUT") {
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end() || it->role != Role::Receiver || !it->process) {
            qWarning("[Shell] SHELL_INPUT: нет активного процесса для сессии %s",
                     qPrintable(sessionId.left(8)));
            return;
        }

        // ══ ВТОРИЧНАЯ ПРОВЕРКА БЕЗОПАСНОСТИ (defence in depth) ═══════════════
        if (hasForbiddenPattern(data)) {
            qCritical("[Shell][SECURITY] ЭСКАЛАЦИЯ (получатель): сессия %s уничтожена",
                      qPrintable(sessionId.left(8)));

            netSend(m_net, from, QJsonObject{
                {"type",      "SHELL_KILL"},
                {"sessionId", sessionId},
                {"reason",    "privilege_escalation"},
            });

            cleanupSession(sessionId);
            emit sessionEnded(sessionId, "privilege_escalation");
            return;
        }
        // ═════════════════════════════════════════════════════════════════════

        resetInactivityTimer(sessionId);
        emit inputMonitored(sessionId, data);

        QByteArray cmd = data;
        if (!cmd.endsWith('\n')) cmd.append('\n');
        it->process->write(cmd);
    }
}

// ── Проверка на эскалацию привилегий ─────────────────────────────────────────

bool RemoteShellManager::hasForbiddenPattern(const QByteArray& input) {
    static const QList<QRegularExpression> kPatterns = {
        // sudo / su / pkexec / doas / runas / gsudo как самостоятельные слова
        QRegularExpression(R"(\b(sudo|su|pkexec|doas|runas|gsudo)\b)",
                           QRegularExpression::CaseInsensitiveOption),
        // Полные пути к sudo/su (обход $PATH)
        QRegularExpression(R"(/usr(/local)?/bin/(sudo|su|pkexec|doas))"),
        // env-bypass: env sudo, env -i sudo, env VAR=x sudo и т.д.
        QRegularExpression(R"(\benv\b[^|&;]*\b(sudo|su|pkexec)\b)"),
        // chmod +s / chmod 4xxx / chmod 6xxx (setuid / setgid бит)
        QRegularExpression(R"(\bchmod\b[^|&;]*[+\-][sS]|\bchmod\s+[46][0-7]{3}\b)"),
        // setcap, newuidmap, newgidmap, nsenter (namespace / capabilities)
        QRegularExpression(R"(\b(setcap|newuidmap|newgidmap|nsenter)\b)"),
        // chown root — смена владельца на root
        QRegularExpression(R"(\bchown\s+(root[: ]|0[: ]))",
                           QRegularExpression::CaseInsensitiveOption),
        // Запись в /etc/passwd, /etc/shadow, /etc/sudoers, /etc/crontab и др.
        QRegularExpression(R"(>{1,2}\s*/etc/(passwd|shadow|sudoers|crontab|cron\.\S*|hosts|ld\.so\.(conf|preload)))",
                           QRegularExpression::CaseInsensitiveOption),
        // tee в /etc/
        QRegularExpression(R"(\btee\b[^|&;]*/etc/)"),
        // Python/Perl/Ruby setuid one-liners
        QRegularExpression(R"(\b(python[23]?|perl|ruby)\b[^|&;]*\b(setuid|setgid)\s*\()"),
        // LD_PRELOAD / LD_LIBRARY_PATH инъекция
        QRegularExpression(R"(\bLD_(PRELOAD|LIBRARY_PATH)\s*=)"),
        // Чтение физической памяти ядра
        QRegularExpression(R"(/dev/(mem|kmem|port)\b)"),
        // PowerShell RunAs (Windows)
        QRegularExpression(R"(Start-Process\s+\S*\s+-Verb\s+RunAs)",
                           QRegularExpression::CaseInsensitiveOption),
        // crontab -e / at now (инъекция cron)
        QRegularExpression(R"(\bcrontab\s+-e|\bat\s+now\b)"),
    };

    const QString str = QString::fromUtf8(input);
    for (const auto& pat : kPatterns) {
        if (pat.match(str).hasMatch())
            return true;
    }
    return false;
}

// ── Генерация одноразового пароля ─────────────────────────────────────────────

QString RemoteShellManager::generateOtp() {
    // Исключаем визуально похожие символы: 0/O, 1/I, B/8 и т.д.
    static const QString kChars = QStringLiteral("ACDEFGHJKLMNPQRTUVWXYZ2346789");
    QString otp;
    otp.reserve(kOtpLength);
    for (int i = 0; i < kOtpLength; ++i)
        otp += kChars[static_cast<int>(
            QRandomGenerator::global()->bounded(static_cast<quint32>(kChars.size())))];
    return otp;
}

// ── Отправка зашифрованных данных ─────────────────────────────────────────────

void RemoteShellManager::sendEncrypted(const QUuid& peerUuid,
                                        const QJsonObject& innerObj,
                                        const QString& outerType) {
    if (!m_e2e->hasSession(bridge::fromQUuid(peerUuid))) {
        qWarning("[Shell] Нет E2E-сессии для %s — данные шелла не отправлены",
                 qPrintable(peerUuid.toString(QUuid::WithoutBraces).left(8)));
        return;
    }

    const QByteArray innerJson =
        QJsonDocument(innerObj).toJson(QJsonDocument::Compact);
    QJsonObject env = bridge::toQJsonObj(
        m_e2e->encrypt(bridge::fromQUuid(peerUuid), bridge::fromQBA(innerJson)));
    if (env.isEmpty()) return;

    // decrypt() не использует поле type — подмена безопасна
    env["type"] = outerType;
    netSend(m_net, peerUuid, env);
}

// ── Запуск шелл-процесса ──────────────────────────────────────────────────────

void RemoteShellManager::spawnProcess(const QString& sessionId) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return;

    SessionData& sd = it.value();

    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);

#ifdef Q_OS_WIN
    proc->start("powershell.exe", {"-NoLogo", "-NoExit", "-NonInteractive"});
#else
    QString shell = "/bin/bash";
    if (!QFile::exists(shell)) {
        shell = "/bin/zsh";
        if (!QFile::exists(shell))
            shell = "/bin/sh";
    }
    proc->start(shell, {});
#endif

    if (!proc->waitForStarted(5000)) {
        qWarning("[Shell] Не удалось запустить шелл: %s",
                 qPrintable(proc->errorString()));
        proc->deleteLater();
        netSend(m_net, sd.peerUuid, QJsonObject{
            {"type",      "SHELL_KILL"},
            {"sessionId", sessionId},
            {"reason",    "spawn_failed"},
        });
        m_sessions.erase(it);
        emit sessionEnded(sessionId, "spawn_failed");
        return;
    }

    sd.process = proc;

    const QUuid peerUuid = sd.peerUuid;

    connect(proc, &QProcess::readyReadStandardOutput,
            this, [this, sessionId, peerUuid, proc]() {
        resetInactivityTimer(sessionId);
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

    connect(proc, &QProcess::finished,
            this, [this, sessionId, peerUuid](int, QProcess::ExitStatus) {
        if (!m_sessions.contains(sessionId)) return;

        qDebug("[Shell] Шелл-процесс завершился, сессия=%s",
               qPrintable(sessionId.left(8)));

        netSend(m_net, peerUuid, QJsonObject{
            {"type",      "SHELL_KILL"},
            {"sessionId", sessionId},
            {"reason",    "process_exited"},
        });
        cleanupSession(sessionId);
        emit sessionEnded(sessionId, "process_exited");
    });

    // Таймер бездействия: убиваем сессию через kSessionTimeoutMs
    auto* timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(kSessionTimeoutMs);
    connect(timer, &QTimer::timeout, this, [this, sessionId]() {
        qWarning("[Shell] Таймаут бездействия: сессия %s завершена",
                 qPrintable(sessionId.left(8)));
        killSession(sessionId, "timeout");
        emit sessionEnded(sessionId, "timeout");
    });
    sd.inactivityTimer = timer;
    timer->start();

    qDebug("[Shell] Шелл-процесс запущен (pid=%lld), сессия=%s, таймаут=%d мин",
           static_cast<long long>(proc->processId()),
           qPrintable(sessionId.left(8)),
           kSessionTimeoutMs / 60000);

    emit shellSessionStarted(sessionId);
}

// ── Сброс таймера бездействия ─────────────────────────────────────────────────

void RemoteShellManager::resetInactivityTimer(const QString& sessionId) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return;
    if (it->inactivityTimer)
        it->inactivityTimer->start(); // restart() — сбрасывает отсчёт
}

// ── Очистка сессии ────────────────────────────────────────────────────────────

void RemoteShellManager::cleanupSession(const QString& sessionId) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return;

    if (it->inactivityTimer) {
        it->inactivityTimer->stop();
        it->inactivityTimer->deleteLater();
        it->inactivityTimer = nullptr;
    }

    if (it->process) {
        it->process->disconnect(this);
        it->process->kill();
        it->process->waitForFinished(2000);
        it->process->deleteLater();
        it->process = nullptr;
    }

    m_sessions.erase(it);
}
