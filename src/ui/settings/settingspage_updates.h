#pragma once
#include "settingspagebase.h"

class QLabel;
class ToggleSwitch;
class UpdateChecker;

class SettingsUpdatesPage : public SettingsPageBase {
    Q_OBJECT
public:
    explicit SettingsUpdatesPage(QWidget* parent = nullptr);
    ~SettingsUpdatesPage() override;
    void reload() override;

private:
    QLabel*        m_lastCheckedLabel  {nullptr};
    QLabel*        m_updateStatusLabel {nullptr};
    ToggleSwitch*  m_autoCheckToggle   {nullptr};
    UpdateChecker* m_checker           {nullptr};
};
