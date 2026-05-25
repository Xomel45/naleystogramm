#pragma once
#include "settingspagebase.h"

class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;

class SettingsInterfacePage : public SettingsPageBase {
    Q_OBJECT
public:
    explicit SettingsInterfacePage(QWidget* parent = nullptr);
    void reload() override;
    bool save()   override;

signals:
    void enterSendsChanged(bool on);

private:
    void rebuildCustomThemeItems();
    void onImportTheme();
    void onRemoveTheme();

    QComboBox*   m_themeCombo        {nullptr};
    QComboBox*   m_langCombo         {nullptr};
    QLabel*      m_customRestartHint {nullptr};
    QPushButton* m_importThemeBtn    {nullptr};
    QPushButton* m_removeThemeBtn    {nullptr};
    QCheckBox*   m_enterSendsCheck   {nullptr};
};
