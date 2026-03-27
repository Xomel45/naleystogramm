#pragma once
#include <QObject>
#include <QString>

// UPnP IGD маппер через SSDP + SOAP.
// Поддерживает: до 3 попыток обнаружения IGD, фильтрацию VPN/tunnel интерфейсов,
// привязку SSDP к корректному LAN интерфейсу, подробное логирование.
class UpnpMapper : public QObject {
    Q_OBJECT
public:
    explicit UpnpMapper(QObject* parent = nullptr);
    void mapPort(quint16 port);

    // Локальный LAN IP, выбранный для UPnP запросов
    [[nodiscard]] QString localIp() const { return m_localIp; }

signals:
    void mapped(bool success);

private:
    void    discover();
    void    fetchControlUrl(const QString& location);
    void    addPortMapping(const QString& controlUrl, quint16 port);
    QString soapRequest(const QString& action, const QString& body);

    quint16 m_port       {0};
    QString m_localIp;
    int     m_retryCount {0};  // Номер текущей попытки (0-based)

    // Параметры повторных попыток обнаружения IGD
    static constexpr int kUpnpTimeoutMs = 5000;  // Таймаут одной попытки (мс)
    static constexpr int kMaxRetries    = 3;      // Макс. число попыток обнаружения
    static constexpr int kRetryDelayMs  = 2000;   // Пауза между попытками (мс)
};
