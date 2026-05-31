#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

// ── .plugin binary format ─────────────────────────────────────────────────
//
// Layout (all integers little-endian):
//
//   [4]  magic   = 0x4E4C5047 ('NLPG')
//   [2]  version = 1
//   [2]  flags   (ENCRYPTED=0x01)
//   [4]  meta_len
//   [meta_len]  JSON metadata — always plaintext (readable without a key)
//
//   If NOT encrypted:
//     [4]  payload_len
//     [payload_len]  archive
//
//   If encrypted:
//     [32]  PBKDF2 salt
//     [12]  AES-GCM nonce (IV)
//     [16]  AES-GCM authentication tag
//     [4]   cipher_len
//     [cipher_len]  AES-256-GCM ciphertext
//
//   Archive (plaintext or decrypted):
//     [4]  file_count
//     repeat file_count:
//       [2]  name_len
//       [name_len]  filename (UTF-8, relative path)
//       [4]  data_len
//       [data_len]  file bytes
//
// Key derivation: PBKDF2-HMAC-SHA256, 100 000 iterations, 32-byte output.
// The key argument to write()/extract() is a UTF-8 passphrase string.

// ── PluginMeta ────────────────────────────────────────────────────────────

struct PluginMeta {
    QString id;
    QString name;
    QString description;
    QString version;
    QString author;
    QString minAppVersion;
    bool    encrypted {false};

    [[nodiscard]] bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }

    [[nodiscard]] QJsonObject toJson() const;
    static PluginMeta fromJson(const QJsonObject& obj);
};

// ── PluginFile ────────────────────────────────────────────────────────────

struct PluginFile {
    QString    name;   // имя файла в архиве, e.g. "plugin.so"
    QByteArray data;
};

// ── PluginFormat ──────────────────────────────────────────────────────────

namespace PluginFormat {

static constexpr quint32 kMagic         = 0x4E4C5047u;
static constexpr quint16 kVersion       = 1;
static constexpr quint16 kFlagEncrypted = 0x0001;

// Прочитать только метаданные из .plugin (не нужен ключ, работает для зашифрованных).
// Возвращает невалидный PluginMeta при ошибке.
PluginMeta readMeta(const QString& filePath);

// Записать .plugin файл.
// key = "" → без шифрования; key ≠ "" → AES-256-GCM
bool write(const QString& filePath, const PluginMeta& meta,
           const QList<PluginFile>& files, const QString& key = {});

// Извлечь файлы из .plugin в директорию destDir.
// key требуется если encrypted (пустая строка для незашифрованных).
// Возвращает true при успехе.
bool extract(const QString& filePath, const QString& destDir, const QString& key = {});

// Проверить ключ без распаковки. Для незашифрованных всегда true.
bool verifyKey(const QString& filePath, const QString& key);

// Описание последней ошибки.
QString lastError();

} // namespace PluginFormat
