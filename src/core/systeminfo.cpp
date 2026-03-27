#include "systeminfo.h"
#include <QFile>
#include <QTextStream>
#include <QSysInfo>
#include <QJsonObject>

// ── Синглтон ─────────────────────────────────────────────────────────────────

SystemInfo& SystemInfo::instance() {
    static SystemInfo inst;
    return inst;
}

SystemInfo::SystemInfo(QObject* parent) : QObject(parent) {}

// ── Основной метод сбора ─────────────────────────────────────────────────────

void SystemInfo::collect() {
#ifdef Q_OS_LINUX
    collectLinux();
#else
    collectFallback();
#endif
    // ОС — через Qt (работает на всех платформах)
    m_osName = QSysInfo::prettyProductName();

    qDebug("[SystemInfo] Собрана информация: тип=%s cpu=%s ram=%s os=%s",
           qPrintable(m_deviceType),
           qPrintable(m_cpuModel),
           qPrintable(m_ramAmount),
           qPrintable(m_osName));
}

// ── Linux: читаем /proc/ ─────────────────────────────────────────────────────

void SystemInfo::collectLinux() {
    m_deviceType = "PC";

    // ── CPU: /proc/cpuinfo ────────────────────────────────────────────────
    // Ищем строку вида: "model name	: Intel(R) Core(TM) i7-12700K CPU @ 3.60GHz"
    QFile cpuFile("/proc/cpuinfo");
    if (cpuFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&cpuFile);
        while (!stream.atEnd()) {
            const QString line = stream.readLine();
            if (line.startsWith("model name")) {
                // Берём всё после первого двоеточия
                m_cpuModel = line.section(':', 1).trimmed();
                // Убираем маркетинговый мусор для компактности:
                //   "(R)" → "", "(TM)" → "", " CPU" → "", лишние пробелы → simplified()
                m_cpuModel.remove("(R)");
                m_cpuModel.remove("(TM)");
                m_cpuModel.remove(" CPU");
                m_cpuModel = m_cpuModel.simplified();
                break;
            }
        }
        cpuFile.close();
    }

    if (m_cpuModel.isEmpty()) {
        // Запасной вариант: архитектура из QSysInfo (например "x86_64")
        m_cpuModel = QSysInfo::currentCpuArchitecture();
    }

    // ── RAM: /proc/meminfo ────────────────────────────────────────────────
    // Ищем строку вида: "MemTotal:       33411232 kB"
    QFile memFile("/proc/meminfo");
    if (memFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&memFile);
        while (!stream.atEnd()) {
            const QString line = stream.readLine();
            if (line.startsWith("MemTotal:")) {
                // Разбиваем на токены: ["MemTotal:", "N", "kB"]
                const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    const qint64 kb = parts.at(1).toLongLong();
                    const double gb = static_cast<double>(kb) / (1024.0 * 1024.0);
                    // Округляем до целых GB если кратно (8, 16, 32...)
                    // Иначе показываем одну десятичную (16.5 GB)
                    const int rounded = qRound(gb);
                    if (qAbs(gb - rounded) < 0.1)
                        m_ramAmount = QString("%1 GB").arg(rounded);
                    else
                        m_ramAmount = QString("%1 GB").arg(gb, 0, 'f', 1);
                }
                break;
            }
        }
        memFile.close();
    }

    if (m_ramAmount.isEmpty()) {
        m_ramAmount = tr("неизвестно");
    }
}

// ── Заглушка для Windows/macOS ───────────────────────────────────────────────

void SystemInfo::collectFallback() {
    m_deviceType = "PC";
    // На не-Linux используем архитектуру как минимальную информацию о CPU
    m_cpuModel   = QSysInfo::currentCpuArchitecture();
    m_ramAmount  = tr("неизвестно");
    // m_osName будет установлен в collect() после вызова этой функции
}

// ── Сериализация ─────────────────────────────────────────────────────────────

QJsonObject SystemInfo::toJson() const {
    return QJsonObject{
        {"deviceType", m_deviceType},
        {"cpuModel",   m_cpuModel},
        {"ramAmount",  m_ramAmount},
        {"osName",     m_osName},
    };
}

QJsonObject SystemInfo::toJsonForHandshake(const QString& externalIp) const {
    QJsonObject obj = toJson();

    // Пасхалка: если сети нет, мы в бункере с Ким Чен-Танком
    if (externalIp.isEmpty() || externalIp == "0.0.0.0") {
        obj["cpuModel"]   = QString("Ким Чен-Танк");
        obj["deviceType"] = QString("Secret Bunker");
        obj["ramAmount"]  = QString("Unlimited Nuclear Power");
    }

    return obj;
}
