#pragma once
#include <QWidget>
#include "thememanager.h"

class QLineEdit;
class QSpinBox;
class QComboBox;
class QLabel;
class QPushButton;
class LogPanel;

// ── SettingsPanel ──────────────────────────────────────────────────────────
// Встроенная панель настроек — занимает левую колонку главного окна.
// Не отдельное окно, а виджет внутри QStackedWidget левой панели.

class SettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget* parent = nullptr);
    void reload();   // перечитать актуальные значения при открытии

signals:
    void nameChanged(const QString& name);
    void networkChanged(const QString& ip, quint16 port);
    void backRequested();  // нажали ← назад

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onSave();
    void onReset();
    void onAvatarClicked();

private:
    void buildHeader();
    void buildProfileSection(QWidget* container);
    void buildNetworkSection(QWidget* container);
    void buildSecuritySection(QWidget* container);
    void buildInterfaceSection(QWidget* container);

    // Отображает аватар с круглой маской в m_avatarLabel
    void applyAvatarPixmap(const QString& path);

    // Профиль
    QLabel*      m_avatarLabel     {nullptr};
    QPushButton* m_changeAvatarBtn {nullptr};
    QLineEdit*   m_nameEdit        {nullptr};
    QLineEdit* m_uuidEdit     {nullptr};  // обновляется в reload()

    // Сеть
    QSpinBox*  m_portSpin     {nullptr};
    QLineEdit* m_ipEdit       {nullptr};
    QLabel*    m_proxyStatus  {nullptr};

    // Режим проброса портов
    QComboBox* m_pfModeCombo    {nullptr};  // UPnP / Manual / Disabled
    QWidget*   m_manualFields   {nullptr};  // контейнер IP+порт (скрыт если не Manual)
    QLineEdit* m_manualIpEdit   {nullptr};
    QSpinBox*  m_manualPortSpin {nullptr};

    // Интерфейс
    QComboBox* m_themeCombo        {nullptr};
    QComboBox* m_langCombo         {nullptr};

    // Безопасность
    QPushButton* m_shellToggle {nullptr};

    // Обновления
    QLabel*    m_lastCheckedLabel  {nullptr};
    QLabel*    m_updateStatusLabel {nullptr};

    // Отладка
    LogPanel*  m_logPanel          {nullptr};
};
