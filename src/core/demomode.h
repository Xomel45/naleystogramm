#pragma once
#include <QString>
#include <QObject>

// ── DemoMode ───────────────────────────────────────────────────────────────
// Режим презентации — UI показывает заглушки вместо реальных данных.
// Сетевой слой НЕ затрагивается: реальные UUID/имя/IP уходят собеседнику.
// Только визуальная маскировка для скриншотов и демо.

class DemoMode : public QObject {
    Q_OBJECT
public:
    static DemoMode& instance();

    [[nodiscard]] bool   enabled()     const { return m_enabled; }
    void                 setEnabled(bool on);

    // Возвращает либо реальное значение либо заглушку
    [[nodiscard]] QString displayName(const QString& real) const;
    [[nodiscard]] QString uuid(const QString& real)        const;
    [[nodiscard]] QString ip(const QString& real)          const;
    [[nodiscard]] quint16 port(quint16 real)               const;

    static constexpr const char* kDemoName = "User-0000";
    static constexpr const char* kDemoUuid = "00000000-0000-0000-0000-000000000000";
    static constexpr const char* kDemoIp   = "0.0.0.0";
    static constexpr quint16     kDemoPort = 0;

signals:
    void toggled(bool enabled);

private:
    explicit DemoMode(QObject* parent = nullptr) : QObject(parent) {}
    bool m_enabled{false};
};
