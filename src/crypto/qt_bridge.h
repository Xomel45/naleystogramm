#pragma once
// Временный мост Qt ↔ Qt-free crypto API.
// Используется только в Qt-файлах (UI, core) во время постепенной миграции.
// Фаза 7: после миграции UI bridge будет удалён.
#include "bytes.h"
#include <QByteArray>
#include <QUuid>
#include <QJsonObject>
#include <QJsonDocument>
#include <nlohmann/json.hpp>

namespace bridge {

inline Bytes fromQBA(const QByteArray& ba) {
    return Bytes(reinterpret_cast<const uint8_t*>(ba.constData()),
                 reinterpret_cast<const uint8_t*>(ba.constData()) + ba.size());
}

inline QByteArray toQBA(const Bytes& b) {
    return QByteArray(reinterpret_cast<const char*>(b.data()),
                      static_cast<qsizetype>(b.size()));
}

inline std::string fromQUuid(const QUuid& uuid) {
    return uuid.toString(QUuid::WithoutBraces).toStdString();
}

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
