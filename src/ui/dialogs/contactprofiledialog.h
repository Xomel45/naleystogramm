#pragma once
#include <QWidget>
#include <QUuid>
#include <QPixmap>

class QLabel;
class QPushButton;
class QToolButton;
class QTimer;
class QScrollArea;
class QMouseEvent;
class QPropertyAnimation;
class NetworkManager;
class StorageManager;

// ── ContactProfileDialog ──────────────────────────────────────────────────
// Оверлей-профиль контакта в TG-стиле.
// Анимация открытия/закрытия идентична SettingsPanel: размытый фон,
// карточка вылетает сверху, оверлей плавно появляется.

class ContactProfileDialog : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double overlayOpacity READ overlayOpacity WRITE setOverlayOpacity)
public:
    ContactProfileDialog(const QUuid& peerUuid,
                         NetworkManager* network,
                         StorageManager* storage,
                         QWidget* parent = nullptr);

    void openPanel();
    void closePanel();

    double overlayOpacity() const { return m_overlayOpacity; }
    void setOverlayOpacity(double v) { m_overlayOpacity = v; update(); }

public slots:
    void refreshData();
    void setSafetyNumber(const QString& safetyNum);

signals:
    void shellRequested(QUuid peerUuid);
    void callRequested(QUuid peerUuid);
    void blockRequested(QUuid peerUuid);

protected:
    void mousePressEvent(QMouseEvent* ev) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void paintEvent(QPaintEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;

private:
    void setupUi();
    void populateData();
    void applyTheme();
    void updateCardGeometry();
    QString formatUptime(const QDateTime& since) const;

    QUuid           m_uuid;
    NetworkManager* m_network  {nullptr};
    StorageManager* m_storage  {nullptr};

    // Оверлей
    QPixmap             m_blurredBg;
    QPixmap             m_trailPixmap;
    double              m_overlayOpacity {0.0};
    bool                m_closing        {false};
    QPropertyAnimation* m_anim           {nullptr};
    QPropertyAnimation* m_fadeAnim       {nullptr};

    // Шапка
    QLabel*      m_avatarLabel {nullptr};
    QLabel*      m_nameLabel   {nullptr};
    QLabel*      m_statusLabel {nullptr};

    // Кнопки действий
    QToolButton* m_callBtn     {nullptr};
    QToolButton* m_shellBtn    {nullptr};

    // Информационные строки (значения)
    QLabel* m_idRow       {nullptr};
    QLabel* m_birthdayRow {nullptr};
    QLabel* m_deviceType  {nullptr};
    QLabel* m_osRow       {nullptr};
    QLabel* m_cpuRow      {nullptr};
    QLabel* m_ramRow      {nullptr};
    QLabel* m_ipRow       {nullptr};
    QLabel* m_portRow     {nullptr};
    QLabel* m_pingRow     {nullptr};
    QLabel* m_uptimeRow   {nullptr};

    // Безопасность
    QLabel* m_safetyLabel   {nullptr};
    QLabel* m_safetyHint    {nullptr};

    // Несовместимость версий
    QLabel* m_compatWarning {nullptr};

    // Карточка + таймер
    QWidget* m_card        {nullptr};
    QTimer*  m_refreshTimer {nullptr};
};
