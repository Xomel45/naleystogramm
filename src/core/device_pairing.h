#pragma once
#include <QString>
#include <QUuid>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>

// ── LinkedDevice ──────────────────────────────────────────────────────────────
// Запись о привязанном устройстве (хранится в session.json).
// На вторичном устройстве: isPrimary=true означает запись о главном.
// На главном устройстве:   isPrimary=false для каждого вторичного.

struct LinkedDevice {
    QUuid   uuid      {};
    QString name      {};
    bool    isPrimary {false};
    qint64  linkedAt  {0};     // Unix timestamp ms

    [[nodiscard]] QJsonObject toJson() const;
    static LinkedDevice fromJson(const QJsonObject& obj);
};

// ── DevicePairing ─────────────────────────────────────────────────────────────
// Логика генерации и проверки 6-значного кода привязки.
// Код действителен 60 секунд и одноразовый: после успешной проверки сбрасывается.

class DevicePairing {
public:
    static constexpr int kCodeLength  = 6;
    static constexpr int kCodeTtlSecs = 60;

    // Генерирует новый 6-значный код (заменяет предыдущий).
    // Вызывать на ГЛАВНОМ устройстве; показать пользователю.
    [[nodiscard]] static QString generateCode();

    // Возвращает активный код (пустой если не сгенерирован или истёк).
    [[nodiscard]] static QString currentCode();

    // Возвращает время истечения активного кода (invalid QDateTime если нет кода).
    [[nodiscard]] static QDateTime codeExpiry();

    // Проверяет код на ГЛАВНОМ устройстве.
    // При совпадении — сбрасывает код (одноразовый). Возвращает true при успехе.
    [[nodiscard]] static bool validateAndConsume(const QString& code);

    // Явный сброс кода (например при отмене пользователем).
    static void clearCode();

    // ── JSON helpers для сетевых фреймов ─────────────────────────────────────

    // DEVICE_PAIR_REQUEST: вторичный → главный
    [[nodiscard]] static QJsonObject makePairRequest(
        const QUuid& ownUuid, const QString& ownName, const QString& code);

    // DEVICE_PAIR_ACCEPT: главный → вторичный
    [[nodiscard]] static QJsonObject makePairAccept(
        const QUuid& ownUuid, const QString& ownName);

    // DEVICE_PAIR_REJECT: главный → вторичный
    [[nodiscard]] static QJsonObject makePairReject(const QString& reason);

private:
    static QString   s_code;
    static QDateTime s_expiry;
};
