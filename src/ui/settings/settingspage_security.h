#pragma once
#include "settingspagebase.h"

class QPushButton;

class SettingsSecurityPage : public SettingsPageBase {
    Q_OBJECT
public:
    explicit SettingsSecurityPage(QWidget* parent = nullptr);
    void reload() override;

private:
    QPushButton* m_shellToggle {nullptr};
};
