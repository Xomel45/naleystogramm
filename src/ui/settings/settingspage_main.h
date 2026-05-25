#pragma once
#include <QWidget>

class QLabel;
class SettingsPanel;

class SettingsMainPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsMainPage(SettingsPanel* panel);
    void reload();

private:
    QLabel* m_avatar {nullptr};
    QLabel* m_name   {nullptr};
    QLabel* m_uuid   {nullptr};
};
