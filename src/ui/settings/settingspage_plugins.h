#pragma once
#include "settingspagebase.h"

class QVBoxLayout;
class QLabel;

class SettingsPluginsPage : public SettingsPageBase {
    Q_OBJECT
public:
    explicit SettingsPluginsPage(QWidget* parent = nullptr);
    void reload() override;

private:
    void rebuildList();
    void openPluginsFolder() const;

    QVBoxLayout* m_listLay {nullptr};
    QLabel*      m_emptyLbl {nullptr};
};
