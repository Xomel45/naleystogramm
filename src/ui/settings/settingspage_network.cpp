#include "settingspage_network.h"
#include "settingshelpers.h"
#include "../wheelfilter.h"
#include "../../core/sessionmanager.h"
#include "../../core/types.h"
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QNetworkProxy>
#include <QMessageBox>
#include <QRegularExpression>

SettingsNetworkPage::SettingsNetworkPage(QWidget* parent) : SettingsPageBase(parent) {
    m_portGroup = new QWidget();
    {
        auto* g = new QVBoxLayout(m_portGroup);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(4);
        g->addWidget(spFieldLabel(tr("Port")));
        m_portSpin = new QSpinBox();
        m_portSpin->setObjectName("settingsInput");
        m_portSpin->setRange(1024, 65535);
        noScrollWheel(m_portSpin);
        g->addWidget(m_portSpin);
        g->addWidget(spHint(tr("Requires restart to take effect")));
    }
    m_lay->addWidget(m_portGroup);
    m_lay->addSpacing(8);

    m_lay->addWidget(spFieldLabel(tr("Bind IP")));
    m_ipEdit = new QLineEdit();
    m_ipEdit->setObjectName("settingsInput");
    m_ipEdit->setPlaceholderText(tr("0.0.0.0  (all interfaces)"));
    m_lay->addWidget(m_ipEdit);
    m_lay->addWidget(spHint(tr("Leave empty for all interfaces")));
    m_lay->addSpacing(8);

    auto* proxyBox = new QWidget();
    proxyBox->setObjectName("settingsInfoBox");
    auto* proxyLayout = new QHBoxLayout(proxyBox);
    proxyLayout->setContentsMargins(12, 8, 12, 8);
    proxyLayout->setSpacing(8);
    const auto proxy = QNetworkProxy::applicationProxy();
    const bool hasProxy = (proxy.type() != QNetworkProxy::NoProxy);
    auto* proxyIcon = new QLabel(hasProxy ? "⚠" : "✓");
    proxyIcon->setObjectName(hasProxy ? "settingsWarn" : "settingsOk");
    m_proxyStatus = new QLabel(
        hasProxy
        ? tr("Proxy %1:%2 — NOT used").arg(proxy.hostName()).arg(proxy.port())
        : tr("Direct connection"));
    m_proxyStatus->setObjectName("settingsHint");
    m_proxyStatus->setWordWrap(true);
    proxyLayout->addWidget(proxyIcon);
    proxyLayout->addWidget(m_proxyStatus, 1);
    m_lay->addWidget(proxyBox);
    m_lay->addSpacing(10);

    m_lay->addWidget(spFieldLabel(tr("Режим проброса портов")));
    m_lay->addSpacing(4);

    m_pfModeCombo = new QComboBox();
    m_pfModeCombo->setObjectName("settingsInput");
    noScrollWheel(m_pfModeCombo);
    m_pfModeCombo->addItem(tr("UPnP (автоматически)"),                  static_cast<int>(PortForwardingMode::UpnpAuto));
    m_pfModeCombo->addItem(tr("Разблокированный порт (ручной проброс)"), static_cast<int>(PortForwardingMode::OpenPort));
    m_pfModeCombo->addItem(tr("Вручную (VPN / статический IP)"),         static_cast<int>(PortForwardingMode::Manual));
    m_pfModeCombo->addItem(tr("Отключено (только локальная сеть)"),      static_cast<int>(PortForwardingMode::Disabled));
    m_pfModeCombo->addItem(tr("🖥 Client-Server (ретранслятор)"),        static_cast<int>(PortForwardingMode::ClientServer));
    m_lay->addWidget(m_pfModeCombo);

    m_manualFields = new QWidget();
    {
        auto* g = new QVBoxLayout(m_manualFields);
        g->setContentsMargins(0, 6, 0, 0);
        g->setSpacing(4);
        g->addWidget(spFieldLabel(tr("Публичный IP (IPv4)")));
        m_manualIpEdit = new QLineEdit();
        m_manualIpEdit->setObjectName("settingsInput");
        m_manualIpEdit->setPlaceholderText("203.0.113.42");
        g->addWidget(m_manualIpEdit);
        g->addSpacing(4);
        g->addWidget(spFieldLabel(tr("Внешний порт")));
        m_manualPortSpin = new QSpinBox();
        m_manualPortSpin->setObjectName("settingsInput");
        m_manualPortSpin->setRange(1024, 65535);
        m_manualPortSpin->setValue(47821);
        noScrollWheel(m_manualPortSpin);
        g->addWidget(m_manualPortSpin);
        g->addWidget(spHint(tr("Укажите порт, пробрасываемый роутером на ваше устройство.\nТребуется перезапуск для применения изменений.")));
    }
    m_manualFields->hide();
    m_lay->addWidget(m_manualFields);

    m_openPortFields = new QWidget();
    {
        auto* g = new QVBoxLayout(m_openPortFields);
        g->setContentsMargins(0, 6, 0, 0);
        g->setSpacing(4);
        g->addWidget(spFieldLabel(tr("Открытый (пробитый на роутере) порт")));
        m_openPortSpin = new QSpinBox();
        m_openPortSpin->setObjectName("settingsInput");
        m_openPortSpin->setRange(1024, 65535);
        m_openPortSpin->setValue(47821);
        noScrollWheel(m_openPortSpin);
        g->addWidget(m_openPortSpin);
        g->addWidget(spHint(tr("⚠ Убедитесь что этот порт реально открыт и пробит в настройках роутера.\nТребуется перезапуск для применения изменений.")));
    }
    m_openPortFields->hide();
    m_lay->addWidget(m_openPortFields);

    m_relayFields = new QWidget();
    {
        auto* g = new QVBoxLayout(m_relayFields);
        g->setContentsMargins(0, 6, 0, 0);
        g->setSpacing(4);
        g->addWidget(spFieldLabel(tr("IP-адрес relay-сервера")));
        m_relayIpEdit = new QLineEdit();
        m_relayIpEdit->setObjectName("settingsInput");
        m_relayIpEdit->setPlaceholderText("203.0.113.10");
        g->addWidget(m_relayIpEdit);
        g->addSpacing(4);
        g->addWidget(spFieldLabel(tr("TCP-порт (сообщения)")));
        m_relayTcpPortSpin = new QSpinBox();
        m_relayTcpPortSpin->setObjectName("settingsInput");
        m_relayTcpPortSpin->setRange(1, 65535);
        m_relayTcpPortSpin->setValue(47822);
        noScrollWheel(m_relayTcpPortSpin);
        g->addWidget(m_relayTcpPortSpin);
        g->addSpacing(4);
        g->addWidget(spFieldLabel(tr("UDP-порт (звонки)")));
        m_relayUdpPortSpin = new QSpinBox();
        m_relayUdpPortSpin->setObjectName("settingsInput");
        m_relayUdpPortSpin->setRange(1, 65535);
        m_relayUdpPortSpin->setValue(47823);
        noScrollWheel(m_relayUdpPortSpin);
        g->addWidget(m_relayUdpPortSpin);
        m_relayWarning = new QLabel(tr("⚠ Требуется перезапуск для применения изменений."));
        m_relayWarning->setObjectName("warningLabel");
        m_relayWarning->setWordWrap(true);
        g->addWidget(m_relayWarning);
    }
    m_relayFields->hide();
    m_lay->addWidget(m_relayFields);

    m_lay->addWidget(spHint(
        tr("UPnP — автоматический проброс портов через роутер.\n"
           "Разблокированный порт — вы пробросили порт вручную, IP определяется автоматически.\n"
           "Вручную — задайте IP и порт вручную (для VPN, static IP, ручного NAT).\n"
           "Отключено — только LAN, пиры подключаются напрямую по локальному IP.\n"
           "Client-Server — все соединения через ваш relay-сервер (белый IP / VPS).")));

    connect(m_pfModeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        const auto mode = static_cast<PortForwardingMode>(m_pfModeCombo->currentData().toInt());
        m_portGroup->setVisible(mode != PortForwardingMode::OpenPort);
        m_manualFields->setVisible(mode == PortForwardingMode::Manual);
        m_openPortFields->setVisible(mode == PortForwardingMode::OpenPort);
        m_relayFields->setVisible(mode == PortForwardingMode::ClientServer);
    });

    m_lay->addStretch();
}

void SettingsNetworkPage::reload() {
    auto& sm = SessionManager::instance();
    m_portSpin->setValue(sm.port());
    m_ipEdit->setText(sm.bindIp());

    const int modeVal = static_cast<int>(sm.portForwardingMode());
    for (int i = 0; i < m_pfModeCombo->count(); ++i) {
        if (m_pfModeCombo->itemData(i).toInt() == modeVal) {
            m_pfModeCombo->setCurrentIndex(i);
            break;
        }
    }
    m_manualIpEdit->setText(sm.manualPublicIp());
    m_manualPortSpin->setValue(sm.manualPublicPort() > 0 ? sm.manualPublicPort() : 47821);
    m_manualFields->setVisible(sm.portForwardingMode() == PortForwardingMode::Manual);

    m_openPortSpin->setValue(sm.manualPublicPort() > 0 ? sm.manualPublicPort() : 47821);
    m_openPortFields->setVisible(sm.portForwardingMode() == PortForwardingMode::OpenPort);
    m_portGroup->setVisible(sm.portForwardingMode() != PortForwardingMode::OpenPort);

    m_relayIpEdit->setText(sm.relayServerIp());
    m_relayTcpPortSpin->setValue(sm.relayTcpPort() > 0 ? sm.relayTcpPort() : 47822);
    m_relayUdpPortSpin->setValue(sm.relayUdpPort() > 0 ? sm.relayUdpPort() : 47823);
    m_relayFields->setVisible(sm.portForwardingMode() == PortForwardingMode::ClientServer);
}

bool SettingsNetworkPage::save() {
    auto& sm = SessionManager::instance();
    const quint16 port = static_cast<quint16>(m_portSpin->value());
    const QString ip   = m_ipEdit->text().trimmed();
    sm.setPort(port);
    sm.setBindIp(ip);

    const auto pfMode = static_cast<PortForwardingMode>(m_pfModeCombo->currentData().toInt());
    static const QRegularExpression kIpv4Re(R"(^(\d{1,3}\.){3}\d{1,3}$)");

    if (pfMode == PortForwardingMode::Manual) {
        const QString manIp = m_manualIpEdit->text().trimmed();
        if (!kIpv4Re.match(manIp).hasMatch()) {
            QMessageBox::warning(this, tr("Ошибка ввода"),
                tr("Некорректный формат IPv4-адреса.\nПример: 203.0.113.42"));
            return false;
        }
        sm.setManualPublicIp(manIp);
        sm.setManualPublicPort(static_cast<quint16>(m_manualPortSpin->value()));
    }

    if (pfMode == PortForwardingMode::ClientServer) {
        const QString relayIp = m_relayIpEdit->text().trimmed();
        if (relayIp.isEmpty() || !kIpv4Re.match(relayIp).hasMatch()) {
            QMessageBox::warning(this, tr("Ошибка ввода"),
                tr("Укажите корректный IPv4-адрес relay-сервера.\nПример: 203.0.113.10"));
            return false;
        }
        sm.setRelayServerIp(relayIp);
        sm.setRelayTcpPort(static_cast<quint16>(m_relayTcpPortSpin->value()));
        sm.setRelayUdpPort(static_cast<quint16>(m_relayUdpPortSpin->value()));
    }

    sm.setPortForwardingMode(pfMode);
    if (pfMode == PortForwardingMode::OpenPort) {
        const quint16 openPort = static_cast<quint16>(m_openPortSpin->value());
        sm.setManualPublicPort(openPort);
        sm.setPort(openPort);
    }

    emit networkChanged(ip, port);
    return true;
}
