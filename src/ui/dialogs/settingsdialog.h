#pragma once
#include <QDialog>
#include "../ui/thememanager.h"

class QStackedWidget;
class QListWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QLabel;

// ── SettingsDialog ─────────────────────────────────────────────────────────
// Три секции:
//   • Профиль  — имя пользователя
//   • Сеть     — IP/порт, прямое подключение по умолчанию
//   • Интерфейс — тема, язык

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

signals:
    void nameChanged(const QString& newName);
    void networkChanged(const QString& ip, quint16 port);
    void languageChanged(const QString& langCode);

private slots:
    void onSave();
    void onReset();

private:
    void buildProfilePage(QWidget* page);
    void buildNetworkPage(QWidget* page);
    void buildInterfacePage(QWidget* page);
    void loadCurrentValues();

    QStackedWidget* m_stack   {nullptr};
    QListWidget*    m_nav     {nullptr};

    // Профиль
    QLineEdit*  m_nameEdit    {nullptr};

    // Сеть
    QLineEdit*  m_ipEdit      {nullptr};
    QSpinBox*   m_portSpin    {nullptr};
    QLabel*     m_proxyStatus {nullptr};

    // Интерфейс
    QComboBox*  m_langCombo   {nullptr};
};
