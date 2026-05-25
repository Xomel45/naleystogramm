#pragma once
#include "settingspagebase.h"
#include <QString>

class QSpinBox;
class QLineEdit;
class QComboBox;
class QLabel;
class QWidget;

class SettingsNetworkPage : public SettingsPageBase {
    Q_OBJECT
public:
    explicit SettingsNetworkPage(QWidget* parent = nullptr);
    void reload() override;
    bool save()   override;

signals:
    void networkChanged(const QString& ip, quint16 port);

private:
    QWidget*   m_portGroup      {nullptr};
    QSpinBox*  m_portSpin       {nullptr};
    QLineEdit* m_ipEdit         {nullptr};
    QLabel*    m_proxyStatus    {nullptr};
    QComboBox* m_pfModeCombo    {nullptr};
    QWidget*   m_manualFields   {nullptr};
    QLineEdit* m_manualIpEdit   {nullptr};
    QSpinBox*  m_manualPortSpin {nullptr};
    QWidget*   m_openPortFields {nullptr};
    QSpinBox*  m_openPortSpin   {nullptr};
    QWidget*   m_relayFields    {nullptr};
    QLineEdit* m_relayIpEdit    {nullptr};
    QSpinBox*  m_relayTcpPortSpin {nullptr};
    QSpinBox*  m_relayUdpPortSpin {nullptr};
    QLabel*    m_relayWarning   {nullptr};
};
