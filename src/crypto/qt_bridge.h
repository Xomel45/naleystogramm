#pragma once
// ══════════════════════════════════════════════════════════════════════════════
//  qt_bridge.h — ЕДИНСТВЕННОЕ место где core/crypto имеет право касаться Qt.
//
//  ПРАВИЛО:
//    • src/core/ и src/crypto/ НЕ включают Qt-заголовки напрямую.
//    • Любое преобразование Qt ↔ std должно происходить здесь.
//    • Этот файл включается ТОЛЬКО из UI-слоя (src/ui/) или из временных
//      заглушек в core во время миграции (Phase 6).
//    • После завершения Phase 6 этот файл включается исключительно из UI.
//
//  Проверка чистоты:
//    cmake --build . --target core-purity        # grep-отчёт о нарушениях
//    cmake -DSTRICT_NO_QT_IN_CORE=ON ...          # hard compile-time ошибка
// ══════════════════════════════════════════════════════════════════════════════

#include "bytes.h"
#include <QByteArray>
#include <QUuid>
#include <QJsonObject>
#include <QJsonDocument>
#include <QString>
#include <QHostAddress>
#include <nlohmann/json.hpp>

namespace bridge {

// ── Bytes ↔ QByteArray ────────────────────────────────────────────────────────
inline Bytes fromQBA(const QByteArray& ba) {
    return Bytes(reinterpret_cast<const uint8_t*>(ba.constData()),
                 reinterpret_cast<const uint8_t*>(ba.constData()) + ba.size());
}

inline QByteArray toQBA(const Bytes& b) {
    return QByteArray(reinterpret_cast<const char*>(b.data()),
                      static_cast<qsizetype>(b.size()));
}

// ── std::string ↔ QString ────────────────────────────────────────────────────
inline std::string fromQStr(const QString& s) {
    return s.toStdString();
}

inline QString toQStr(const std::string& s) {
    return QString::fromStdString(s);
}

// ── std::string ↔ QUuid ──────────────────────────────────────────────────────
inline std::string fromQUuid(const QUuid& uuid) {
    return uuid.toString(QUuid::WithoutBraces).toStdString();
}

inline QUuid toQUuid(const std::string& s) {
    return QUuid::fromString(QString::fromStdString(s));
}

// ── std::string ↔ QHostAddress ───────────────────────────────────────────────
inline QHostAddress toQHostAddress(const std::string& s) {
    return QHostAddress(QString::fromStdString(s));
}

inline std::string fromQHostAddress(const QHostAddress& a) {
    return a.toString().toStdString();
}

// ── nlohmann::json ↔ QJsonObject ─────────────────────────────────────────────
inline nlohmann::json fromQJsonObj(const QJsonObject& obj) {
    const QByteArray ba = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    return nlohmann::json::parse(ba.constBegin(), ba.constEnd());
}

inline QJsonObject toQJsonObj(const nlohmann::json& j) {
    if (j.is_null()) return {};
    const std::string s = j.dump();
    return QJsonDocument::fromJson(QByteArray(s.c_str(), static_cast<qsizetype>(s.size()))).object();
}

} // namespace bridge
