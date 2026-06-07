#pragma once
#include "settingspagebase.h"
#include <cstdint>

class SettingsDemoPage : public SettingsPageBase {
    Q_OBJECT
public:
    explicit SettingsDemoPage(QWidget* parent = nullptr);
    ~SettingsDemoPage();
private:
    uint32_t m_demoToken{0};
};
