#pragma once
#include <QObject>

class StorageManager;
class E2EManager;
class NetworkManager;
class FileTransfer;
class CallManager;
class RemoteShellManager;

// Слой владения core-сервисами.
// Создаётся в main() до MainWindow; передаётся по ссылке в конструктор MainWindow.
// Управляет порядком инициализации: KeyProtector → Storage → E2E → Network → остальные.
// NetworkManager::init() (запуск сервера) вызывается из MainWindow после подключения сигналов.
class App : public QObject {
    Q_OBJECT
public:
    explicit App(QObject* parent = nullptr);
    ~App() override;

    [[nodiscard]] StorageManager&     storage();
    [[nodiscard]] E2EManager&         e2e();
    [[nodiscard]] NetworkManager&     network();
    [[nodiscard]] FileTransfer&       fileTransfer();
    [[nodiscard]] CallManager&        callManager();
    [[nodiscard]] RemoteShellManager& shellManager();

private:
    StorageManager*     m_storage     {nullptr};
    E2EManager*         m_e2e         {nullptr};
    NetworkManager*     m_network     {nullptr};
    FileTransfer*       m_fileTransfer{nullptr};
    CallManager*        m_callManager {nullptr};
    RemoteShellManager* m_shellManager{nullptr};
};
