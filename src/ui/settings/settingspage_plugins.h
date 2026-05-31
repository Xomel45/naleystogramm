#pragma once
#include "settingspagebase.h"

class QVBoxLayout;
class QLabel;
class IPlugin;

class SettingsPluginsPage : public SettingsPageBase {
    Q_OBJECT
public:
    explicit SettingsPluginsPage(QWidget* parent = nullptr);
    void reload() override;

private:
    void rebuildList();
    void showKeyDialog(const QString& id, const QString& name);
    void showPluginSettings(IPlugin* plugin);
    void openPluginsFolder() const;

    QVBoxLayout* m_listLay  {nullptr};
    QLabel*      m_emptyLbl {nullptr};
};
