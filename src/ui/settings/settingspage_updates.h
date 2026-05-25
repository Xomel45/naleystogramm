#pragma once
#include "settingspagebase.h"

class QLabel;
class ToggleSwitch;

class SettingsUpdatesPage : public SettingsPageBase {
    Q_OBJECT
public:
    explicit SettingsUpdatesPage(QWidget* parent = nullptr);
    void reload() override;

private:
    QLabel*       m_lastCheckedLabel  {nullptr};
    QLabel*       m_updateStatusLabel {nullptr};
    ToggleSwitch* m_autoCheckToggle   {nullptr};
};
