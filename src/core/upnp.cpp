#include "upnp.h"
#include <QUdpSocket>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkInterface>
#include <QTimer>
#include <QUrl>
#include <QRegularExpression>

static constexpr int kUpnpTimeout = 4000; // ms

UpnpMapper::UpnpMapper(QObject* parent) : QObject(parent) {
    // Find our local LAN IP
    for (const auto& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
        if (!(iface.flags() & QNetworkInterface::IsUp)) continue;
        for (const auto& entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                m_localIp = entry.ip().toString();
                break;
            }
        }
        if (!m_localIp.isEmpty()) break;
    }
}

void UpnpMapper::mapPort(quint16 port) {
    m_port = port;
    discover();
}

// ── SSDP discovery ────────────────────────────────────────────────────────

void UpnpMapper::discover() {
    auto* udp = new QUdpSocket(this);
    udp->bind(QHostAddress::Any, 0, QUdpSocket::ShareAddress);

    const QByteArray ssdp =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 2\r\n"
        "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
        "\r\n";

    udp->writeDatagram(ssdp, QHostAddress("239.255.255.250"), 1900);

    auto* timer = new QTimer(this);
    timer->setSingleShot(true);

    connect(udp, &QUdpSocket::readyRead, this, [this, udp, timer]() {
        timer->stop();
        QByteArray data;
        data.resize(static_cast<int>(udp->pendingDatagramSize()));
        udp->readDatagram(data.data(), data.size());
        udp->deleteLater();

        // Extract LOCATION header
        static const QRegularExpression re(
            "LOCATION:\\s*(http://[^\\r\\n]+)",
            QRegularExpression::CaseInsensitiveOption);
        auto match = re.match(QString::fromLatin1(data));
        if (!match.hasMatch()) {
            emit mapped(false);
            return;
        }

        fetchControlUrl(match.captured(1).trimmed());
    });

    connect(timer, &QTimer::timeout, this, [this, udp]() {
        udp->deleteLater();
        qWarning("[UPnP] Discovery timeout");
        emit mapped(false);
    });

    timer->start(kUpnpTimeout);
}

// Fetch the IGD description XML to find WANIPConnection controlURL
void UpnpMapper::fetchControlUrl(const QString& location) {
    auto* nam = new QNetworkAccessManager(this);
    auto* reply = nam->get(QNetworkRequest(QUrl(location)));

    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, location]() {
        reply->deleteLater();
        nam->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit mapped(false);
            return;
        }

        const QString xml = QString::fromUtf8(reply->readAll());

        // Find controlURL for WANIPConnection or WANPPPConnection
        static const QRegularExpression re(
            "<serviceType>urn:schemas-upnp-org:service:WAN(?:IP|PPP)Connection:1</serviceType>"
            ".*?<controlURL>([^<]+)</controlURL>",
            QRegularExpression::DotMatchesEverythingOption);
        auto match = re.match(xml);
        if (!match.hasMatch()) {
            emit mapped(false);
            return;
        }

        // Build full control URL
        QUrl base(location);
        QString ctrl = match.captured(1).trimmed();
        if (!ctrl.startsWith("http"))
            ctrl = QString("%1://%2:%3%4")
                .arg(base.scheme()).arg(base.host())
                .arg(base.port(80)).arg(ctrl);

        addPortMapping(ctrl, m_port);
    });
}

// ── SOAP: AddPortMapping ──────────────────────────────────────────────────

void UpnpMapper::addPortMapping(const QString& controlUrl, quint16 port) {
    const QString body = QString(
        "<NewRemoteHost></NewRemoteHost>"
        "<NewExternalPort>%1</NewExternalPort>"
        "<NewProtocol>TCP</NewProtocol>"
        "<NewInternalPort>%1</NewInternalPort>"
        "<NewInternalClient>%2</NewInternalClient>"
        "<NewEnabled>1</NewEnabled>"
        "<NewPortMappingDescription>naleystogramm</NewPortMappingDescription>"
        "<NewLeaseDuration>3600</NewLeaseDuration>")
        .arg(port).arg(m_localIp);

    const QByteArray soap = soapRequest("AddPortMapping", body).toUtf8();

    // C++20: используем brace-init чтобы избежать most vexing parse
    // QNetworkRequest req(QUrl(controlUrl)) компилятор трактует как объявление функции
    QNetworkRequest req{QUrl{controlUrl}};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "text/xml; charset=\"utf-8\"");
    req.setRawHeader("SOAPAction",
        "\"urn:schemas-upnp-org:service:WANIPConnection:1#AddPortMapping\"");

    auto* nam = new QNetworkAccessManager(this);
    auto* reply = nam->post(req, soap);

    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        const bool ok = (reply->error() == QNetworkReply::NoError);
        if (ok) qDebug("[UPnP] Port mapped successfully");
        else qWarning("[UPnP] SOAP error: %s", qPrintable(reply->errorString()));
        reply->deleteLater();
        nam->deleteLater();
        emit mapped(ok);
    });
}

QString UpnpMapper::soapRequest(const QString& action, const QString& body) {
    return QString(
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body>"
        "<u:%1 xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">"
        "%2"
        "</u:%1>"
        "</s:Body>"
        "</s:Envelope>")
        .arg(action, body);
}
