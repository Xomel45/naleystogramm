#pragma once
#include <QObject>

// Minimal UPnP IGD port mapper via SSDP + SOAP.
// Works on most home routers including Tenda and TP-Link.
class UpnpMapper : public QObject {
    Q_OBJECT
public:
    explicit UpnpMapper(QObject* parent = nullptr);
    void mapPort(quint16 port);

signals:
    void mapped(bool success);

private:
    void        discover();
    void        fetchControlUrl(const QString& location);  // FIX: не был объявлен
    void        addPortMapping(const QString& controlUrl, quint16 port);
    QString     soapRequest(const QString& action, const QString& body);

    quint16     m_port{0};
    QString     m_localIp;
};
