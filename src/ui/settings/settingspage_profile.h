#pragma once
#include "settingspagebase.h"
#include <QString>

class QLineEdit;
class QLabel;
class QPushButton;
class QTextEdit;

class SettingsProfilePage : public SettingsPageBase {
    Q_OBJECT
public:
    explicit SettingsProfilePage(QWidget* parent = nullptr);
    void reload() override;
    bool save()   override;

    void setExternalAddress(const QString& ip, quint16 port);

signals:
    void nameChanged(const QString& name);
    void avatarChanged(const QString& path);

private:
    void onAvatarClicked();
    void applyAvatarPixmap(const QString& path);
    bool eventFilter(QObject* watched, QEvent* event) override;

    QLabel*      m_avatarLabel     {nullptr};
    QPushButton* m_changeAvatarBtn {nullptr};
    QLineEdit*   m_nameEdit        {nullptr};
    QLineEdit*   m_uuidEdit        {nullptr};
    QLineEdit*   m_birthdayEdit    {nullptr};
    QTextEdit*   m_bioEdit         {nullptr};
    QLabel*      m_profileNameLbl  {nullptr};

    QString  m_externalIp;
    quint16  m_externalPort {0};
};
