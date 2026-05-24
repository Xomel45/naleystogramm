#pragma once
#include <QWidget>
#include <QPixmap>
#include "thememanager.h"

class QLineEdit;
class QSpinBox;
class QComboBox;
class QLabel;
class QPushButton;
class LogPanel;
class QPropertyAnimation;
class QStackedWidget;
class QScrollArea;

// ── SettingsPanel ──────────────────────────────────────────────────────────
// Overlay-панель настроек — появляется поверх всего окна с анимацией сверху.
// Внутри — стековая навигация: главная страница → страницы разделов.

class SettingsPanel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal overlayOpacity READ overlayOpacity WRITE setOverlayOpacity)
public:
    explicit SettingsPanel(QWidget* parent = nullptr);
    void reload();      // перечитать актуальные значения
    void openPanel();   // показать с анимацией
    void closePanel();  // скрыть с анимацией

signals:
    void nameChanged(const QString& name);
    void networkChanged(const QString& ip, quint16 port);
    void backRequested();  // устарел, оставлен для совместимости
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
    void onReset();
    void onAvatarClicked();
    void onImportTheme();
    void onRemoveTheme();

private:
    void updateCardGeometry();
    void applyCardTheme();
    void rebuildCustomThemeItems();
    void applyAvatarPixmap(const QString& path);
    void showSection(int pageIdx, const QString& title, bool hasSave);
    void showMainPage();

    QWidget*     buildMainPage();
    QScrollArea* buildProfilePage();
    QScrollArea* buildNetworkPage();
    QScrollArea* buildDemoPage();
    QScrollArea* buildPrivacyPage();
    QScrollArea* buildSecurityPage();
    QScrollArea* buildInterfacePage();
    QScrollArea* buildDevicesPage();
    QScrollArea* buildUpdatesPage();
    QScrollArea* buildDebugPage();

    // Профиль
    QLabel*      m_avatarLabel     {nullptr};
    QPushButton* m_changeAvatarBtn {nullptr};
    QLineEdit*   m_nameEdit        {nullptr};
    QLineEdit*   m_uuidEdit        {nullptr};
    class QTextEdit* m_bioEdit     {nullptr};
    QLabel*      m_profileNameLbl  {nullptr};

    // Сеть
    QWidget*   m_portGroup    {nullptr};
    QSpinBox*  m_portSpin     {nullptr};
    QLineEdit* m_ipEdit       {nullptr};
    QLabel*    m_proxyStatus  {nullptr};

    // Режим проброса портов
    QComboBox* m_pfModeCombo    {nullptr};
    QWidget*   m_manualFields   {nullptr};
    QLineEdit* m_manualIpEdit   {nullptr};
    QSpinBox*  m_manualPortSpin {nullptr};
    QWidget*   m_openPortFields {nullptr};
    QSpinBox*  m_openPortSpin   {nullptr};

    // Relay (Client-Server)
    QWidget*   m_relayFields      {nullptr};
    QLineEdit* m_relayIpEdit      {nullptr};
    QSpinBox*  m_relayTcpPortSpin {nullptr};
    QSpinBox*  m_relayUdpPortSpin {nullptr};
    QLabel*    m_relayWarning     {nullptr};

    // Интерфейс
    QComboBox*       m_themeCombo        {nullptr};
    QComboBox*       m_langCombo         {nullptr};
    QLabel*          m_customRestartHint {nullptr};
    QPushButton*     m_importThemeBtn    {nullptr};
    QPushButton*     m_removeThemeBtn    {nullptr};
    class QCheckBox* m_enterSendsCheck   {nullptr};

    // Конфиденциальность
    QComboBox* m_privacyMessages {nullptr};
    QComboBox* m_privacyFiles    {nullptr};
    QComboBox* m_privacyCalls    {nullptr};
    QComboBox* m_privacyVoice    {nullptr};
    QComboBox* m_privacyAvatar   {nullptr};
    QComboBox* m_privacyShell    {nullptr};

    // Безопасность
    QPushButton* m_shellToggle {nullptr};

    // Обновления
    QLabel* m_lastCheckedLabel  {nullptr};
    QLabel* m_updateStatusLabel {nullptr};

    // Отладка
    LogPanel* m_logPanel {nullptr};

    // Главная страница — только отображение (не редактирование)
    QLabel* m_mainPageAvatar {nullptr};
    QLabel* m_mainPageName   {nullptr};
    QLabel* m_mainPageUuid   {nullptr};

    // Навигация
    QStackedWidget* m_pageStack    {nullptr};
    QLabel*         m_cardTitleLbl {nullptr};
    QPushButton*    m_backBtn      {nullptr};
    QPushButton*    m_saveBtn      {nullptr};

    // Overlay
    QWidget*            m_card         {nullptr};
    QPropertyAnimation* m_anim         {nullptr};
    QPropertyAnimation* m_fadeAnim     {nullptr};
    QPixmap             m_blurredBg;
    bool                m_closing      {false};
    qreal               m_overlayOpacity {0.0};

    qreal overlayOpacity() const { return m_overlayOpacity; }
    void setOverlayOpacity(qreal v) { m_overlayOpacity = v; update(); }
};
