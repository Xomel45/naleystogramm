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
            qWarning("[UPnP] [1/4 SSDP] Не удалось привязать UDP к %s: %s — "
                     "пробуем Any",
                     qPrintable(m_localIp), qPrintable(udp->errorString()));
            if (!udp->bind(QHostAddress::Any, 0, QUdpSocket::ShareAddress)) {
                qWarning("[UPnP] [1/4 SSDP] Привязка к Any тоже провалилась: %s — "
                         "SSDP невозможен",
                         qPrintable(udp->errorString()));
                udp->deleteLater();
                emit mapped(false);
                return;
            }
            qDebug("[UPnP] [1/4 SSDP] Привязан к Any:%d (fallback)",
                   udp->localPort());
        } else {
            qDebug("[UPnP] [1/4 SSDP] Привязан к %s:%d",
                   qPrintable(m_localIp), udp->localPort());
        }
    } else {
        if (!udp->bind(QHostAddress::Any, 0, QUdpSocket::ShareAddress)) {
            qWarning("[UPnP] [1/4 SSDP] Привязка к Any провалилась: %s",
                     qPrintable(udp->errorString()));
            udp->deleteLater();
            emit mapped(false);
            return;
        }
        qDebug("[UPnP] [1/4 SSDP] Привязан к Any:%d (localIp не определён)",
               udp->localPort());
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
    if (sent != ssdp.size()) {
        qWarning("[UPnP] [1/4 SSDP] Пакет отправлен частично (%lld из %lld байт): %s",
                 sent, static_cast<qint64>(ssdp.size()), qPrintable(udp->errorString()));
    } else {
        qDebug("[UPnP] [1/4 SSDP] M-SEARCH отправлен на 239.255.255.250:1900 "
               "(%lld байт), ожидаем ответ %d мс...",
               static_cast<qint64>(ssdp.size()), kUpnpTimeoutMs);
    }

    auto* timer = new QTimer(this);
    timer->setSingleShot(true);

    connect(udp, &QUdpSocket::readyRead, this, [this, udp, timer]() {
        timer->stop();
        timer->deleteLater();

        QHostAddress senderAddr;
        quint16 senderPort = 0;
        QByteArray data;
        data.resize(static_cast<int>(udp->pendingDatagramSize()));
        udp->readDatagram(data.data(), data.size(), &senderAddr, &senderPort);
        udp->deleteLater();

        qDebug("[UPnP] [1/4 SSDP] Ответ получен от %s:%d (%lld байт)",
               qPrintable(senderAddr.toString()), senderPort,
               static_cast<qint64>(data.size()));

        // Извлекаем LOCATION: заголовок из SSDP ответа
        static const QRegularExpression re(
            "LOCATION:\\s*(http://[^\\r\\n]+)",
            QRegularExpression::CaseInsensitiveOption);
        auto match = re.match(QString::fromLatin1(data));
        if (!match.hasMatch()) {
            qWarning("[UPnP] [1/4 SSDP] LOCATION заголовок не найден.\n"
                     "  Ответ роутера:\n%s\n"
                     "  Вероятная причина: роутер ответил, но не является IGD "
                     "(нет WANIPConnection/WANPPPConnection).",
                     data.constData());
            emit mapped(false);
            return;
        }

        const QString location = match.captured(1).trimmed();
        qDebug("[UPnP] [1/4 SSDP] IGD обнаружен: %s", qPrintable(location));
        fetchControlUrl(location);
    });

    connect(timer, &QTimer::timeout, this, [this, udp]() {
        udp->deleteLater();

        if (m_retryCount + 1 < kMaxRetries) {
            ++m_retryCount;
            qWarning("[UPnP] [1/4 SSDP] Таймаут (%d мс) — роутер не ответил на "
                     "M-SEARCH. Повтор через %d мс (попытка %d/%d).\n"
                     "  Возможные причины: UPnP отключён на роутере, мультикаст "
                     "239.255.255.250 блокируется, интерфейс %s не видит роутер.",
                     kUpnpTimeoutMs, kRetryDelayMs,
                     m_retryCount + 1, kMaxRetries,
                     qPrintable(m_localIp));
            QTimer::singleShot(kRetryDelayMs, this, &UpnpMapper::discover);
        } else {
            qWarning("[UPnP] [1/4 SSDP] IGD не найден после %d попыток.\n"
                     "  Что делать:\n"
                     "  1. Зайти в панель роутера → включить UPnP/IGD\n"
                     "  2. Или переключить режим на «Разблокированный порт» "
                     "и пробросить порт вручную\n"
                     "  3. Или использовать режим «Ретранслятор»",
                     kMaxRetries);
            emit mapped(false);
        }
    });

    timer->start(kUpnpTimeoutMs);
}

// Загружаем XML-описание IGD для поиска controlURL WANIPConnection/WANPPPConnection
void UpnpMapper::fetchControlUrl(const QString& location) {
    qDebug("[UPnP] [2/4 Describe] Загружаем описание IGD: %s", qPrintable(location));

    auto* nam = new QNetworkAccessManager(this);
    auto* reply = nam->get(QNetworkRequest(QUrl(location)));

    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, location]() {
        reply->deleteLater();
        nam->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning("[UPnP] [2/4 Describe] Ошибка загрузки XML-описания IGD:\n"
                     "  URL: %s\n"
                     "  Ошибка: %s (HTTP %d)",
                     qPrintable(location),
                     qPrintable(reply->errorString()),
                     reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
            emit mapped(false);
            return;
        }

        const QString xml = QString::fromUtf8(reply->readAll());
        qDebug("[UPnP] [2/4 Describe] XML получен (%lld байт)",
               static_cast<qint64>(xml.size()));

        // Ищем controlURL для WANIPConnection или WANPPPConnection;
        // захватываем "IP" / "PPP" чтобы передать правильный SOAPAction.
        static const QRegularExpression re(
            "<serviceType>urn:schemas-upnp-org:service:WAN(IP|PPP)Connection:1</serviceType>"
            ".*?<controlURL>([^<]+)</controlURL>",
            QRegularExpression::DotMatchesEverythingOption);
        auto match = re.match(xml);
        if (!match.hasMatch()) {
            // Показываем доступные serviceType чтобы понять что поддерживает роутер
            static const QRegularExpression reServices(
                "<serviceType>([^<]+)</serviceType>",
                QRegularExpression::DotMatchesEverythingOption);
            QStringList services;
            auto it = reServices.globalMatch(xml);
            while (it.hasNext())
                services << it.next().captured(1).trimmed();
            qWarning("[UPnP] [2/4 Describe] controlURL не найден.\n"
                     "  Нужен: WANIPConnection:1 или WANPPPConnection:1\n"
                     "  Найдено в XML (%lld сервисов): %s\n"
                     "  Вероятно, роутер не поддерживает UPnP IGD v1.",
                     static_cast<qint64>(services.size()),
                     qPrintable(services.join(", ")));
            emit mapped(false);
            return;
        }

        // group 1 = "IP" или "PPP", group 2 = controlURL
        const QString serviceType = "WAN" + match.captured(1) + "Connection";

        // Строим полный URL управления (если relative path — дополняем схемой/хостом/портом)
        QUrl base(location);
        QString ctrl = match.captured(2).trimmed();
        if (!ctrl.startsWith("http"))
            ctrl = QString("%1://%2:%3%4")
                .arg(base.scheme(), base.host())
                .arg(base.port(80))
                .arg(ctrl);

        qDebug("[UPnP] [2/4 Describe] Service: %s, Control URL: %s",
               qPrintable(serviceType), qPrintable(ctrl));
        addPortMapping(ctrl, m_port, serviceType);
    });
}

// ── SOAP: AddPortMapping ──────────────────────────────────────────────────

void UpnpMapper::addPortMapping(const QString& controlUrl, quint16 port,
                                const QString& serviceType) {
    const QString body = QString(
        "<NewRemoteHost></NewRemoteHost>"
        "<NewExternalPort>%1</NewExternalPort>"
        "<NewProtocol>TCP</NewProtocol>"
        "<NewInternalPort>%1</NewInternalPort>"
        "<NewInternalClient>%2</NewInternalClient>"
        "<NewEnabled>1</NewEnabled>"
        "<NewPortMappingDescription>naleystogramm</NewPortMappingDescription>"
        "<NewLeaseDuration>0</NewLeaseDuration>")
        .arg(port).arg(m_localIp);

    const QByteArray soap = soapRequest("AddPortMapping", body, serviceType).toUtf8();

    qDebug("[UPnP] [3/4 SOAP] AddPortMapping: порт %d → %s\n"
           "  Control URL: %s",
           port, qPrintable(m_localIp), qPrintable(controlUrl));

    // C++20: используем brace-init чтобы избежать most vexing parse
    // QNetworkRequest req(QUrl(controlUrl)) компилятор трактует как объявление функции
    QNetworkRequest req{QUrl{controlUrl}};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "text/xml; charset=\"utf-8\"");
    req.setRawHeader("SOAPAction",
        QString("\"urn:schemas-upnp-org:service:%1:1#AddPortMapping\"")
        .arg(serviceType).toUtf8());

    auto* nam = new QNetworkAccessManager(this);
    auto* reply = nam->post(req, soap);

    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, port]() {
        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        if (reply->error() == QNetworkReply::NoError) {
            qDebug("[UPnP] [3/4 SOAP] AddPortMapping OK (HTTP %d) — "
                   "порт %d проброшен успешно", httpStatus, port);
        } else {
            // Парсим UPnP SOAP Fault: <errorCode> и <errorDescription>
            const QString xml = QString::fromUtf8(body);
            static const QRegularExpression reCode("<errorCode>(\\d+)</errorCode>");
            static const QRegularExpression reDesc(
                "<errorDescription>([^<]+)</errorDescription>");
            const auto mCode = reCode.match(xml);
            const auto mDesc = reDesc.match(xml);
            const QString upnpCode = mCode.hasMatch() ? mCode.captured(1) : "?";
            const QString upnpDesc = mDesc.hasMatch() ? mDesc.captured(1) : "нет";

            // Расшифровка кодов ошибок IGD (UPnP Forum WANIPConnection:1 spec)
            QString hint;
            if (upnpCode == "718")
                hint = "порт уже занят другим приложением (ConflictInMappingEntry)";
            else if (upnpCode == "725")
                hint = "роутер принимает только постоянные маппинги (OnlyPermanentLeasesSupported) — "
                       "уже используем LeaseDuration=0, возможна несовместимость прошивки";
            else if (upnpCode == "501")
                hint = "действие не поддерживается роутером (ActionFailed)";
            else if (upnpCode == "606")
                hint = "доступ запрещён роутером (Unauthorized)";

            qWarning("[UPnP] [3/4 SOAP] AddPortMapping провалился:\n"
                     "  HTTP статус: %d\n"
                     "  SOAP ошибка: %s (%s)%s\n"
                     "  Тело ответа: %s",
                     httpStatus,
                     qPrintable(upnpCode), qPrintable(upnpDesc),
                     hint.isEmpty() ? "" : qPrintable("\n  Подсказка: " + hint),
                     body.constData());
        }

        reply->deleteLater();
        nam->deleteLater();
        emit mapped(reply->error() == QNetworkReply::NoError);
    });
}

QString UpnpMapper::soapRequest(const QString& action, const QString& body,
                                 const QString& serviceType) {
    return QString(
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body>"
        "<u:%1 xmlns:u=\"urn:schemas-upnp-org:service:%3:1\">"
        "%2"
        "</u:%1>"
        "</s:Body>"
        "</s:Envelope>")
        .arg(action, body, serviceType);
}
