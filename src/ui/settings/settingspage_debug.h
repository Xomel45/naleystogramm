#pragma once
#include "settingspagebase.h"

class LogPanel;

class SettingsDebugPage : public SettingsPageBase {
    Q_OBJECT
public:
    explicit SettingsDebugPage(QWidget* parent = nullptr);

signals:
    void verboseLoggingChanged(bool enabled);

private:
    LogPanel* m_logPanel {nullptr};
};
