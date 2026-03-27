#pragma once
#include <QObject>
#include <QString>
#include <QFile>
#include <QMutex>
#include <QDateTime>

// Уровни логирования
enum class LogLevel {
    Debug,    // Отладочная информация
    Info,     // Информационные сообщения
    Warning,  // Предупреждения
    Error     // Ошибки
};

// Компоненты приложения для фильтрации логов
enum class LogComponent {
    Network,      // Сетевой слой
    FileTransfer, // Передача файлов
    Crypto,       // Шифрование
    Storage,      // Хранение данных
    UI,           // Интерфейс
    General       // Общее
};

// Структура одной записи лога
struct LogEntry {
    QDateTime   timestamp;
    LogLevel    level;
    LogComponent component;
    QString     message;
};

// ── Logger ──────────────────────────────────────────────────────────────────
// Централизованная система логирования с записью в файл и сигналами для UI.
//
// Путь к файлу:
//   Windows: %LOCALAPPDATA%\naleystogramm\debug.log
//   Linux:   ~/.cache/naleystogramm/debug.log
//
// Формат записи:
//   [2026-02-21 12:34:56.789] [INFO] [NETWORK] Сообщение
//
class Logger : public QObject {
    Q_OBJECT
public:
    static Logger& instance();

    // Инициализация — открывает файл лога
    void init();

    // Основные методы логирования
    void debug(LogComponent comp, const QString& message);
    void info(LogComponent comp, const QString& message);
    void warning(LogComponent comp, const QString& message);
    void error(LogComponent comp, const QString& message);

    // Универсальный метод
    void log(LogLevel level, LogComponent comp, const QString& message);

    // Включить/выключить подробный режим (debug сообщения)
    void setVerbose(bool enabled);
    [[nodiscard]] bool isVerbose() const { return m_verbose; }

    // Получить путь к файлу лога
    [[nodiscard]] QString logFilePath() const { return m_filePath; }

    // Очистить файл лога
    void clearLog();

    // Получить последние N записей
    [[nodiscard]] QList<LogEntry> recentEntries(int count = 100) const;

signals:
    // Новая запись в логе (для UI)
    void logEntry(LogEntry entry);

    // Файл лога очищен
    void logCleared();

private:
    explicit Logger(QObject* parent = nullptr);
    ~Logger();

    void initFilePath();
    void rotateIfNeeded();
    void writeToFile(const LogEntry& entry);
    QString formatEntry(const LogEntry& entry) const;
    QString levelToString(LogLevel level) const;
    QString componentToString(LogComponent comp) const;

    QString m_filePath;
    QFile*  m_file{nullptr};
    QMutex  m_mutex;
    bool    m_verbose{false};

    QList<LogEntry> m_recentEntries;
    static constexpr int kMaxRecentEntries = 1000;
    static constexpr qint64 kMaxFileSize = 5 * 1024 * 1024;  // 5 МБ
};

// Удобные макросы для логирования
#define LOG_DEBUG(comp, msg)   Logger::instance().debug(LogComponent::comp, msg)
#define LOG_INFO(comp, msg)    Logger::instance().info(LogComponent::comp, msg)
#define LOG_WARNING(comp, msg) Logger::instance().warning(LogComponent::comp, msg)
#define LOG_ERROR(comp, msg)   Logger::instance().error(LogComponent::comp, msg)
