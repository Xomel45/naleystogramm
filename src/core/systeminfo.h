#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>

// ── SystemInfo ────────────────────────────────────────────────────────────────
// Синглтон. Собирает статическую информацию об аппаратуре и ОС один раз
// при запуске (collect()). Результат передаётся собеседнику в HANDSHAKE
// и отображается в ContactProfileDialog.
//
// Поддерживаемые платформы:
//   Linux  — /proc/cpuinfo, /proc/meminfo, QSysInfo
//   Other  — заглушка через QSysInfo

class SystemInfo : public QObject {
    Q_OBJECT
public:
    // Единственный экземпляр (создаётся при первом обращении)
    static SystemInfo& instance();

    // Выполнить сбор данных. Вызывать один раз после запуска приложения.
    // Повторный вызов перезаписывает данные.
    void collect();

    // Геттеры собранных данных
    [[nodiscard]] QString deviceType() const { return m_deviceType; }
    [[nodiscard]] QString cpuModel()   const { return m_cpuModel;   }
    [[nodiscard]] QString ramAmount()  const { return m_ramAmount;  }
    [[nodiscard]] QString osName()     const { return m_osName;     }

    // Сериализация (для локального отображения в ContactProfileDialog)
    [[nodiscard]] QJsonObject toJson() const;

    // Сериализация для отправки в HANDSHAKE.
    // Принимает внешний IP — если сеть недоступна, активируется пасхалка.
    [[nodiscard]] QJsonObject toJsonForHandshake(const QString& externalIp) const;

private:
    explicit SystemInfo(QObject* parent = nullptr);

    // Сбор данных на Linux через /proc/
    void collectLinux();

    // Заглушка для не-Linux платформ (Windows, macOS)
    void collectFallback();

    QString m_deviceType {"PC"};
    QString m_cpuModel   {};
    QString m_ramAmount  {};
    QString m_osName     {};
};
