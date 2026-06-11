#pragma once
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class NetworkManager;

namespace glui {

// Поднимает NetworkManager (naleystogramm-core, Qt-free) напрямую — без App,
// чтобы не тянуть CallManager/MediaEngine (Qt-зависимость, не нужна для PoC).
// Собирает NetworkEvent в потокобезопасную очередь строк, которую главный
// (GL) поток вычитывает каждый кадр через drainLogs(). Подтверждает, что
// мост core↔glui работает без QMetaObject::invokeMethod.
class CoreBridge {
public:
    CoreBridge();
    ~CoreBridge();

    CoreBridge(const CoreBridge&)            = delete;
    CoreBridge& operator=(const CoreBridge&) = delete;

    // Возвращает и очищает накопившиеся строки лога (thread-safe).
    [[nodiscard]] std::vector<std::string> drainLogs();

private:
    void pushLog(const std::string& line);

    std::unique_ptr<NetworkManager> m_network;

    std::mutex            m_mutex;
    std::deque<std::string> m_logs;
};

} // namespace glui
