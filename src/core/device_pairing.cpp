#include "device_pairing.h"
#include <QRandomGenerator>

// ── Static storage ────────────────────────────────────────────────────────────
QString   DevicePairing::s_code;
QDateTime DevicePairing::s_expiry;

// ── LinkedDevice ──────────────────────────────────────────────────────────────

QJsonObject LinkedDevice::toJson() const {
    return QJsonObject{
        {"uuid",      uuid.toString(QUuid::WithoutBraces)},
        {"name",      name},
        {"isPrimary", isPrimary},
        {"linkedAt",  linkedAt},
    };
}

LinkedDevice LinkedDevice::fromJson(const QJsonObject& obj) {
    LinkedDevice d;
    d.uuid      = QUuid(obj["uuid"].toString());
    d.name      = obj["name"].toString();
    d.isPrimary = obj["isPrimary"].toBool(false);
    d.linkedAt  = static_cast<qint64>(obj["linkedAt"].toDouble(0));
    return d;
}

// ── DevicePairing ─────────────────────────────────────────────────────────────

QString DevicePairing::generateCode() {
    const quint32 n = QRandomGenerator::global()->bounded(1'000'000u);
    s_code   = QString("%1").arg(n, kCodeLength, 10, QChar('0'));
    s_expiry = QDateTime::currentDateTime().addSecs(kCodeTtlSecs);
    return s_code;
}

QString DevicePairing::currentCode() {
    if (s_code.isEmpty()) return {};
    if (QDateTime::currentDateTime() > s_expiry) { clearCode(); return {}; }
    return s_code;
}

QDateTime DevicePairing::codeExpiry() {
    return s_expiry;
}

bool DevicePairing::validateAndConsume(const QString& code) {
    if (s_code.isEmpty()) return false;
    if (QDateTime::currentDateTime() > s_expiry) { clearCode(); return false; }
    if (code != s_code) return false;
    clearCode();
    return true;
}

void DevicePairing::clearCode() {
    s_code.clear();
    s_expiry = QDateTime();
}

// ── Frame builders ────────────────────────────────────────────────────────────

QJsonObject DevicePairing::makePairRequest(
    const QUuid& ownUuid, const QString& ownName, const QString& code)
{
    return QJsonObject{
        {"type", "DEVICE_PAIR_REQUEST"},
        {"uuid", ownUuid.toString(QUuid::WithoutBraces)},
        {"name", ownName},
        {"code", code},
    };
}

QJsonObject DevicePairing::makePairAccept(
    const QUuid& ownUuid, const QString& ownName)
{
    return QJsonObject{
        {"type", "DEVICE_PAIR_ACCEPT"},
        {"uuid", ownUuid.toString(QUuid::WithoutBraces)},
        {"name", ownName},
    };
}

QJsonObject DevicePairing::makePairReject(const QString& reason) {
    return QJsonObject{
        {"type",   "DEVICE_PAIR_REJECT"},
        {"reason", reason},
    };
}
