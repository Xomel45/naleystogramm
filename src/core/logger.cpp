#include "logger.h"
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
#include <QMutexLocker>
#include <QDebug>
#include <QRegularExpression>

// Имя директории и файла приложения
static constexpr const char* kAppDirName = "naleystogramm";
static constexpr const char* kLogFileName = "debug.log";

// ── Singleton ───────────────────────────────────────────────────────────────

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::Logger(QObject* parent) : QObject(parent) {
    initFilePath();
}

Logger::~Logger() {
    if (m_file) {
        m_file->close();
        delete m_file;
    }
}

// ── Инициализация ───────────────────────────────────────────────────────────

void Logger::initFilePath() {
#ifdef Q_OS_WIN
    // Windows: %LOCALAPPDATA%\naleystogramm\debug.log
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
#else
    // Linux/macOS: ~/.cache/naleystogramm/debug.log
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation);
#endif

    // Qt добавляет org/app в путь — берём родительскую папку
    QDir dir(base);
    dir.cdUp();
    dir.mkpath(kAppDirName);
    dir.cd(kAppDirName);

    m_filePath = dir.filePath(kLogFileName);
}

void Logger::init() {
    QMutexLocker locker(&m_mutex);

    // Ротация если файл слишком большой
    rotateIfNeeded();

    // Открываем файл для дозаписи
    m_file = new QFile(m_filePath);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning("[Logger] Cannot open log file: %s", qPrintable(m_filePath));
        delete m_file;
        m_file = nullptr;
        return;
    }

    qDebug("[Logger] Log file: %s", qPrintable(m_filePath));

    // Пишем заголовок новой сессии
    QTextStream stream(m_file);
    stream << "\n";
    stream << "========================================\n";
    stream << "  Session started: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    stream << "========================================\n";
    stream.flush();
}

// ── Ротация файла ───────────────────────────────────────────────────────────

void Logger::rotateIfNeeded() {
    QFileInfo info(m_filePath);
    if (!info.exists()) return;

    if (info.size() > kMaxFileSize) {
        // Переименовываем старый файл
        const QString backupPath = m_filePath + ".old";
        QFile::remove(backupPath);
        QFile::rename(m_filePath, backupPath);
        qDebug("[Logger] Rotated log file (size: %lld bytes)", info.size());
    }
}

// ── Методы логирования ──────────────────────────────────────────────────────

void Logger::debug(LogComponent comp, const QString& message) {
    // Debug сообщения только в verbose режиме
    if (!m_verbose) return;
    log(LogLevel::Debug, comp, message);
}

void Logger::info(LogComponent comp, const QString& message) {
    log(LogLevel::Info, comp, message);
}

void Logger::warning(LogComponent comp, const QString& message) {
    log(LogLevel::Warning, comp, message);
}

void Logger::error(LogComponent comp, const QString& message) {
    log(LogLevel::Error, comp, message);
}

// Санитизирует строку сообщения: заменяет длинные hex-строки (≥64 символа) на [REDACTED].
// Применяется к сообщениям криптографического компонента чтобы предотвратить
// случайное попадание ключевого материала в лог-файл.
// 64 hex-символа = 32 байта = минимальный размер X25519/AES-256 ключа.
static QString sanitizeMessage(LogComponent comp, const QString& msg) {
    if (comp != LogComponent::Crypto) return msg;

    // Ищем hex-строки длиной ≥64 символа (ключи, хэши, производные)
    // Qt6 оптимизирует выражения автоматически — флаг не нужен
    static const QRegularExpression kHexPattern("[0-9a-fA-F]{64,}");

    QString result = msg;
    result.replace(kHexPattern, "[REDACTED]");
    return result;
}

void Logger::log(LogLevel level, LogComponent comp, const QString& message) {
    // Санитизируем криптографические сообщения перед записью
    const QString safeMessage = sanitizeMessage(comp, message);

    LogEntry entry{
        .timestamp = QDateTime::currentDateTime(),
        .level     = level,
        .component = comp,
        .message   = safeMessage
    };

    // Пишем в файл
    writeToFile(entry);

    // Сохраняем в буфер последних записей
    {
        QMutexLocker locker(&m_mutex);
        m_recentEntries.append(entry);
        while (m_recentEntries.size() > kMaxRecentEntries)
            m_recentEntries.removeFirst();
    }

    // Эмитим сигнал для UI
    emit logEntry(entry);

    // Дублируем в qDebug — используем safeMessage, чтобы ключевой материал
    // не попадал в syslog / консоль / IDE-вывод в обход файловой редакции
    qDebug("[%s][%s] %s",
           qPrintable(levelToString(level)),
           qPrintable(componentToString(comp)),
           qPrintable(safeMessage));
}

// ── Запись в файл ───────────────────────────────────────────────────────────

void Logger::writeToFile(const LogEntry& entry) {
    QMutexLocker locker(&m_mutex);

    if (!m_file || !m_file->isOpen()) return;

    QTextStream stream(m_file);
    stream << formatEntry(entry) << "\n";
    stream.flush();
}

QString Logger::formatEntry(const LogEntry& entry) const {
    // Формат: [2026-02-21 12:34:56.789] [INFO] [NETWORK] Сообщение
    return QString("[%1] [%2] [%3] %4")
        .arg(entry.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"))
        .arg(levelToString(entry.level), -7)  // Выравнивание по левому краю
        .arg(componentToString(entry.component))
        .arg(entry.message);
}

// ── Вспомогательные методы ──────────────────────────────────────────────────

QString Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
    }
    return "UNKNOWN";
}

QString Logger::componentToString(LogComponent comp) const {
    switch (comp) {
        case LogComponent::Network:      return "NETWORK";
        case LogComponent::FileTransfer: return "FILETRANSFER";
        case LogComponent::Crypto:       return "CRYPTO";
        case LogComponent::Storage:      return "STORAGE";
        case LogComponent::UI:           return "UI";
        case LogComponent::General:      return "GENERAL";
    }
    return "UNKNOWN";
}

void Logger::setVerbose(bool enabled) {
    m_verbose = enabled;
    info(LogComponent::General,
         QString("Verbose logging %1").arg(enabled ? "enabled" : "disabled"));
}

void Logger::clearLog() {
    QMutexLocker locker(&m_mutex);

    // Очищаем буфер
    m_recentEntries.clear();

    // Очищаем файл
    if (m_file && m_file->isOpen()) {
        m_file->close();
        m_file->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);

        QTextStream stream(m_file);
        stream << "========================================\n";
        stream << "  Log cleared: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        stream << "========================================\n";
        stream.flush();
    }

    emit logCleared();
}

QList<LogEntry> Logger::recentEntries(int count) const {
    QMutexLocker locker(const_cast<QMutex*>(&m_mutex));

    if (count >= m_recentEntries.size())
        return m_recentEntries;

    return m_recentEntries.mid(m_recentEntries.size() - count);
}
