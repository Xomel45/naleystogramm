#pragma once
#include "settingspagebase.h"

class QComboBox;

class SettingsPrivacyPage : public SettingsPageBase {
    Q_OBJECT
public:
    explicit SettingsPrivacyPage(QWidget* parent = nullptr);
    void reload() override;
    bool save()   override;

private:
    QComboBox* m_messages {nullptr};
    QComboBox* m_files    {nullptr};
    QComboBox* m_calls    {nullptr};
    QComboBox* m_voice    {nullptr};
    QComboBox* m_avatar   {nullptr};
    QComboBox* m_shell    {nullptr};
};
