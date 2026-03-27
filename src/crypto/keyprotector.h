#pragma once
#include <QByteArray>
#include <QObject>

// ── KeyProtector — менеджер мастер-ключа ─────────────────────────────────────
//
// Отвечает за хранение и использование 32-байтного мастер-ключа.
// Мастер-ключ генерируется случайно при первом запуске и сохраняется
// в AppDataLocation/master.key с правами 0600 (только владелец).
//
// Используется:
//   - Для шифрования файла приватных ключей E2E (keys/*.json)
//   - Для получения ключа SQLCipher (deriveKey("db"))
//
// Формат зашифрованных данных (encrypt / decrypt):
//   [12 байт nonce][16 байт GCM-tag][N байт ciphertext]
//
// Никогда не передаётся по сети и не сохраняется в открытом виде.

class KeyProtector {
public:
    static KeyProtector& instance();

    // Загрузить или сгенерировать мастер-ключ. Вызывать до всех остальных методов.
    bool init();

    // Проверить, инициализирован ли KeyProtector
    [[nodiscard]] bool isReady() const noexcept { return !m_masterKey.isEmpty(); }

    // Вывести дочерний 32-байтный ключ через HKDF-SHA256(masterKey, label)
    [[nodiscard]] QByteArray deriveKey(const QByteArray& label, int bytes = 32) const;

    // Зашифровать произвольный блоб с AES-256-GCM
    [[nodiscard]] QByteArray encrypt(const QByteArray& plaintext) const;

    // Расшифровать блоб, созданный encrypt()
    [[nodiscard]] QByteArray decrypt(const QByteArray& blob) const;

private:
    KeyProtector()  = default;
    ~KeyProtector() = default;

    QByteArray m_masterKey; // 32 случайных байта
};
