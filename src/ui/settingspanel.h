#pragma once
#include <QWidget>
#include "thememanager.h"

class QLineEdit;
class QSpinBox;
class QComboBox;
class QLabel;

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

private slots:
    void onSave();
    void onReset();

private:
    void buildHeader();
    void buildProfileSection(QWidget* container);
    void buildNetworkSection(QWidget* container);
    void buildInterfaceSection(QWidget* container);

    // Профиль
    QLabel*    m_avatarLabel  {nullptr};
    QLineEdit* m_nameEdit     {nullptr};
    QLineEdit* m_uuidEdit     {nullptr};  // обновляется в reload()

    // Сеть
    QSpinBox*  m_portSpin     {nullptr};
    QLineEdit* m_ipEdit       {nullptr};
    QLabel*    m_proxyStatus  {nullptr};

    // Интерфейс
    QComboBox* m_langCombo         {nullptr};

    // Обновления
    QLabel*    m_lastCheckedLabel  {nullptr};
    QLabel*    m_updateStatusLabel {nullptr};
};
