#pragma once
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <nlohmann/json.hpp>

class App;

// Текстовый интерактивный режим (--cli): без MainWindow/SplashScreen.
// Поднимает NetworkManager (через App), обрабатывает E2E-рукопожатие и
// зашифрованные сообщения ("CHAT"), читает команды из stdin в отдельном
// потоке. Команды: contacts, send <N> <text>, history <N> [limit], help, quit.
class CliSession {
public:
    explicit CliSession(App& core);
    ~CliSession();

    // Блокирующий цикл. Возвращает код выхода процесса.
    int run();

private:
    void setupNetworkListener();
    void handleMessage(const std::string& fromUuid, const nlohmann::json& msg);
    void handlePeerConnected(const std::string& uuid, const std::string& name);
    void sendKeyBundle(const std::string& uuid);

    void printHelp() const;
    void printContacts() const;
    void printHistory(int index, int limit) const;
    void sendTextMessage(int index, const std::string& text);

    void handleCommand(const std::string& line);
    void pushLine(const std::string& line);

    App& m_core;

    std::mutex              m_outMutex;
    std::deque<std::string> m_pendingLines;

    std::atomic<bool> m_running{true};
};
