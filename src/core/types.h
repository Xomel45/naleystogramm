#pragma once
// Чистые типы данных — передаются между core и UI.
// Не содержит сервисных классов (QObject, сигналов, методов).
// UI-заголовки включают этот файл вместо полных service-заголовков.
#include <QUuid>
#include <QString>
#include <QByteArray>
#include <QDateTime>

// ── Контакт ──────────────────────────────────────────────────────────────────
struct Contact {
    QUuid      uuid;
    QString    name;
    QString    ip;
    quint16    port{0};
    QByteArray identityKey;
    QString    avatarHash {};
    QString    avatarPath {};
    bool       isBlocked {false};
    bool       isMuted   {false};
    QDateTime  lastSeen  {};
    QString    systemInfoJson {};
    QString    versionCreated {"0.1.0"};
};

// ── Сообщение ─────────────────────────────────────────────────────────────────
struct Message {
    qint64     id{0};
    QUuid      peerUuid;
    bool       outgoing{false};
    QString    text;
    QString    fileName;
    qint64     fileSize{0};
    QByteArray ciphertext;
    QDateTime  timestamp;
    bool       delivered{false};
    bool       isVoice{false};
    int        voiceDurationMs{0};
    QString    versionCreated {"0.1.0"};
};

// ── Логирование ───────────────────────────────────────────────────────────────
enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

enum class LogComponent {
    Network,
    FileTransfer,
    Crypto,
    Storage,
    UI,
    General
};

struct LogEntry {
    QDateTime    timestamp;
    LogLevel     level;
    LogComponent component;
    QString      message;
};

// ── Прогресс передачи файла ──────────────────────────────────────────────────
struct TransferProgress {
    QString  id;
    QString  fileName;
    qint64   bytesTransferred;
    qint64   totalBytes;
    double   speedBytesPerSec;
    int      etaSeconds;
    int      percent;
    bool     outgoing;
};

// ── Информация об обновлении ─────────────────────────────────────────────────
struct UpdateInfo {
    QString version;
    QString url;
    QString notes;
    bool    available{false};
};
