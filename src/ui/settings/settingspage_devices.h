#pragma once
#include "settingspagebase.h"
#include <QString>

class SettingsDevicesPage : public SettingsPageBase {
    Q_OBJECT
public:
    explicit SettingsDevicesPage(QWidget* parent = nullptr);

signals:
    void connectToDeviceRequested(const QString& host, quint16 port, const QString& code);
};
