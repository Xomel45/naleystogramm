#pragma once
#include <QDialog>
#include <QUuid>

class QLabel;
class QPushButton;
class QTimer;
class NetworkManager;
class StorageManager;

// ── ContactProfileDialog ──────────────────────────────────────────────────
// Немодальный диалог профиля контакта.
// Показывает аватар, имя, статус онлайн, системную информацию пира
// (CPU, ОЗУ, ОС) и статистику соединения (пинг, IP, порт, аптайм).
// Данные пинга и аптайма обновляются каждые 5 секунд.

class ContactProfileDialog : public QDialog {
    Q_OBJECT
public:
    ContactProfileDialog(const QUuid& peerUuid,
                         NetworkManager* network,
                         StorageManager* storage,
                         QWidget* parent = nullptr);

public slots:
    // Горячее обновление — вызывать при получении peerInfoUpdated(uuid)
    void refreshData();

    // Устанавливает номер безопасности (Safety Number) в диалоге.
    // Вызывать после создания диалога, когда сессия E2E уже установлена.
    void setSafetyNumber(const QString& safetyNum);

signals:
    // Пользователь нажал кнопку ">_" — запросить удалённый шелл
    void shellRequested(QUuid peerUuid);

private:
    void setupUi();
    void populateData();
    QString formatUptime(const QDateTime& since) const;

    QUuid            m_uuid;
    NetworkManager*  m_network  {nullptr};
    StorageManager*  m_storage  {nullptr};

    // Аватар
    QLabel* m_avatarLabel   {nullptr};

    // Шапка
    QLabel* m_nameLabel     {nullptr};
    QLabel* m_statusLabel   {nullptr};

    // Устройство
    QLabel* m_deviceType    {nullptr};
    QLabel* m_cpuRow        {nullptr};
    QLabel* m_ramRow        {nullptr};
    QLabel* m_osRow         {nullptr};

    // Соединение
    QLabel* m_pingRow       {nullptr};
    QLabel* m_ipRow         {nullptr};
    QLabel* m_portRow       {nullptr};
    QLabel* m_uptimeRow     {nullptr};

    // Совместимость версий
    QLabel*      m_compatWarning {nullptr}; // предупреждение о несовместимости (скрыто по умолчанию)

    // Безопасность
    QLabel*      m_safetyLabel   {nullptr}; // номер безопасности (Safety Number)
    QLabel*      m_safetyHint    {nullptr}; // подсказка для верификации

    // Кнопка удалённого шелла — активна только когда пир онлайн
    QPushButton* m_shellBtn      {nullptr};

    QTimer* m_refreshTimer  {nullptr};
};
