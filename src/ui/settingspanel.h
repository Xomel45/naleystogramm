#pragma once
#include <QWidget>
#include <QPixmap>

class QPropertyAnimation;
class QStackedWidget;
class SettingsMainPage;
class SettingsProfilePage;
class SettingsNetworkPage;
class SettingsDemoPage;
class SettingsPrivacyPage;
class SettingsSecurityPage;
class SettingsInterfacePage;
class SettingsDevicesPage;
class SettingsUpdatesPage;
class SettingsDebugPage;

// ── SettingsPanel ──────────────────────────────────────────────────────────
// Overlay-панель настроек — появляется поверх всего окна с анимацией сверху.
// Внутри — стековая навигация: главная страница → страницы разделов.

class SettingsPanel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal overlayOpacity READ overlayOpacity WRITE setOverlayOpacity)
public:
    explicit SettingsPanel(QWidget* parent = nullptr);
    void reload();
    void openPanel();
    void closePanel();

    void setExternalAddress(const QString& ip, quint16 port);

    // Вызывается из SettingsMainPage и SettingsDevicesPage
    void showSection(int pageIdx, const QString& title, bool hasSave);

signals:
    void nameChanged(const QString& name);
    void networkChanged(const QString& ip, quint16 port);
    void backRequested();
    void verboseLoggingChanged(bool enabled);
    void avatarChanged(const QString& path);
    void enterSendsChanged(bool on);
    void connectToDeviceRequested(const QString& host, quint16 port, const QString& code);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onSave();

private:
    void updateCardGeometry();
    void applyCardTheme();
    void showMainPage();

    // Страницы
    SettingsMainPage*      m_mainPage      {nullptr};
    SettingsProfilePage*   m_profilePage   {nullptr};
    SettingsNetworkPage*   m_networkPage   {nullptr};
    SettingsDemoPage*      m_demoPage      {nullptr};
    SettingsPrivacyPage*   m_privacyPage   {nullptr};
    SettingsSecurityPage*  m_securityPage  {nullptr};
    SettingsInterfacePage* m_interfacePage {nullptr};
    SettingsDevicesPage*   m_devicesPage   {nullptr};
    SettingsUpdatesPage*   m_updatesPage   {nullptr};
    SettingsDebugPage*     m_debugPage     {nullptr};

    // Навигация
    QStackedWidget* m_pageStack    {nullptr};
    class QLabel*   m_cardTitleLbl {nullptr};
    class QPushButton* m_backBtn   {nullptr};
    class QPushButton* m_saveBtn   {nullptr};

    // Overlay
    QWidget*            m_card           {nullptr};
    QPropertyAnimation* m_anim           {nullptr};
    QPropertyAnimation* m_fadeAnim       {nullptr};
    QPixmap             m_blurredBg;
    QPixmap             m_trailPixmap;
    bool                m_closing        {false};
    qreal               m_overlayOpacity {0.0};

    qreal overlayOpacity() const { return m_overlayOpacity; }
    void setOverlayOpacity(qreal v) { m_overlayOpacity = v; update(); }
};
