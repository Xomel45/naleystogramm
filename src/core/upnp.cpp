#include "upnp.h"
#include <QUdpSocket>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkInterface>
#include <QTimer>
#include <QUrl>
#include <QDebug>
#include <QRegularExpression>

UpnpMapper::UpnpMapper(QObject* parent) : QObject(parent) {
    // Определяем лучший локальный LAN IP для SSDP-пакетов.
    // Приоритет: 192.168.x.x (типичная домашняя сеть) > 172.16–31.x.x > 10.x.x.x
    // Пропускаем loopback, выключенные и VPN/tunnel интерфейсы (tun, tap, wg, ppp, …).
    static const QStringList kVpnPrefixes {
        "tun", "tap", "wg", "utun", "ppp", "vpn", "veth", "docker", "virbr", "br-"
    };

    QString best192, best172, best10, bestOther;

    for (const auto& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags() & QNetworkInterface::IsLoopBack)  continue;
        if (!(iface.flags() & QNetworkInterface::IsUp))     continue;
        if (!(iface.flags() & QNetworkInterface::IsRunning)) continue;

        // Фильтрация VPN/tunnel по имени интерфейса
        const QString ifName = iface.name().toLower();
        bool isVpn = false;
        for (const auto& pfx : kVpnPrefixes) {
            if (ifName.startsWith(pfx)) { isVpn = true; break; }
        }
        if (isVpn) {
            qDebug("[UPnP] Пропускаем интерфейс %s (VPN/tunnel)",
                   qPrintable(iface.name()));
            continue;
        }

        for (const auto& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            const QString ip = entry.ip().toString();
            qDebug("[UPnP] Интерфейс %s IP: %s",
                   qPrintable(iface.name()), qPrintable(ip));
            if      (ip.startsWith("192.168.") && best192.isEmpty())  best192 = ip;
            else if (ip.startsWith("172.")      && best172.isEmpty())  best172 = ip;
            else if (ip.startsWith("10.")       && best10.isEmpty())   best10  = ip;
            else if (bestOther.isEmpty())                              bestOther = ip;
        }
    }

    m_localIp = !best192.isEmpty()  ? best192
              : !best172.isEmpty()  ? best172
              : !best10.isEmpty()   ? best10
              :                       bestOther;

    if (m_localIp.isEmpty())
        qWarning("[UPnP] Не найден подходящий LAN IP — UPnP может не работать");
    else
        qDebug("[UPnP] Выбран локальный IP: %s", qPrintable(m_localIp));
}

void UpnpMapper::mapPort(quint16 port) {
    m_port       = port;
    m_retryCount = 0;
    discover();
}

// ── SSDP discovery ────────────────────────────────────────────────────────

void UpnpMapper::discover() {
    qDebug("[UPnP] Попытка обнаружения IGD %d/%d (localIp=%s)",
           m_retryCount + 1, kMaxRetries, qPrintable(m_localIp));

    auto* udp = new QUdpSocket(this);

    // Привязываемся к конкретному LAN IP, а не QHostAddress::Any —
    // это гарантирует, что SSDP пакет уйдёт через нужный интерфейс,
    // а не через VPN или loopback при наличии нескольких интерфейсов.
    if (!m_localIp.isEmpty()) {
        if (!udp->bind(QHostAddress(m_localIp), 0, QUdpSocket::ShareAddress)) {
            qWarning("[UPnP] Не удалось привязать UDP к %s: %s",
                     qPrintable(m_localIp), qPrintable(udp->errorString()));
            // Запасной вариант — привязываемся к Any
            udp->bind(QHostAddress::Any, 0, QUdpSocket::ShareAddress);
        }
    } else {
        udp->bind(QHostAddress::Any, 0, QUdpSocket::ShareAddress);
    }

    const QByteArray ssdp =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 2\r\n"
        "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
        "\r\n";

    const qint64 sent = udp->writeDatagram(ssdp,
                                           QHostAddress("239.255.255.250"), 1900);
    if (sent != ssdp.size())
        qWarning("[UPnP] SSDP пакет отправлен частично (%lld из %d байт)",
                 sent, ssdp.size());
    else
        qDebug("[UPnP] SSDP M-SEARCH отправлен на 239.255.255.250:1900");

    auto* timer = new QTimer(this);
    timer->setSingleShot(true);

    connect(udp, &QUdpSocket::readyRead, this, [this, udp, timer]() {
        timer->stop();
        timer->deleteLater();
        QByteArray data;
        data.resize(static_cast<int>(udp->pendingDatagramSize()));
        udp->readDatagram(data.data(), data.size());
        udp->deleteLater();

        qDebug("[UPnP] SSDP ответ получен (%d байт)", data.size());

        // Извлекаем LOCATION: заголовок из SSDP ответа
        static const QRegularExpression re(
            "LOCATION:\\s*(http://[^\\r\\n]+)",
            QRegularExpression::CaseInsensitiveOption);
        auto match = re.match(QString::fromLatin1(data));
        if (!match.hasMatch()) {
            qWarning("[UPnP] LOCATION заголовок не найден в SSDP ответе — "
                     "IGD не поддерживает стандартный UPnP");
            emit mapped(false);
            return;
        }

        const QString location = match.captured(1).trimmed();
        qDebug("[UPnP] IGD найден: %s", qPrintable(location));
        fetchControlUrl(location);
    });

    connect(timer, &QTimer::timeout, this, [this, udp]() {
        udp->deleteLater();

        if (m_retryCount + 1 < kMaxRetries) {
            ++m_retryCount;
            qWarning("[UPnP] Таймаут обнаружения IGD — повтор через %d мс "
                     "(попытка %d/%d)",
                     kRetryDelayMs, m_retryCount + 1, kMaxRetries);
            QTimer::singleShot(kRetryDelayMs, this, &UpnpMapper::discover);
        } else {
            qWarning("[UPnP] IGD устройство не найдено после %d попыток — "
                     "UPnP недоступен (нет ответа на SSDP)", kMaxRetries);
            emit mapped(false);
        }
    });

    timer->start(kUpnpTimeoutMs);
}

// Загружаем XML-описание IGD для поиска controlURL WANIPConnection/WANPPPConnection
void UpnpMapper::fetchControlUrl(const QString& location) {
    qDebug("[UPnP] Загружаем описание IGD: %s", qPrintable(location));

    auto* nam = new QNetworkAccessManager(this);
    auto* reply = nam->get(QNetworkRequest(QUrl(location)));

    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, location]() {
        reply->deleteLater();
        nam->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning("[UPnP] Ошибка загрузки описания IGD: %s",
                     qPrintable(reply->errorString()));
            emit mapped(false);
            return;
        }

        const QString xml = QString::fromUtf8(reply->readAll());
        qDebug("[UPnP] Описание IGD получено (%d байт)", xml.size());

        // Ищем controlURL для WANIPConnection или WANPPPConnection
        static const QRegularExpression re(
            "<serviceType>urn:schemas-upnp-org:service:WAN(?:IP|PPP)Connection:1</serviceType>"
            ".*?<controlURL>([^<]+)</controlURL>",
            QRegularExpression::DotMatchesEverythingOption);
        auto match = re.match(xml);
        if (!match.hasMatch()) {
            qWarning("[UPnP] controlURL для WANIPConnection/WANPPPConnection не найден "
                     "в описании IGD — роутер может не поддерживать UPnP IGD v1");
            emit mapped(false);
            return;
        }

        // Строим полный URL управления (если relative path — дополняем схемой/хостом/портом)
        QUrl base(location);
        QString ctrl = match.captured(1).trimmed();
        if (!ctrl.startsWith("http"))
            ctrl = QString("%1://%2:%3%4")
                .arg(base.scheme(), base.host())
                .arg(base.port(80))
                .arg(ctrl);

        qDebug("[UPnP] Control URL: %s", qPrintable(ctrl));
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
