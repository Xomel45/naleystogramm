#include "clisession.h"

#include "../core/app.h"
#include "../core/types.h"
#include "../core/identity/identity.h"
#include "../core/storage/storage.h"
#include "../core/net/network.h"
#include "../crypto/e2e.h"

#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

namespace {

int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string randomMsgId() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << dist(rng) << dist(rng);
    return oss.str();
}

std::string shortUuid(const std::string& uuid) {
    return uuid.substr(0, 8);
}

} // namespace

CliSession::CliSession(App& core) : m_core(core) {
    setupNetworkListener();
}

CliSession::~CliSession() = default;

void CliSession::pushLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(m_outMutex);
    m_pendingLines.push_back(line);
}

void CliSession::setupNetworkListener() {
    NetworkEvent ev;

    ev.onLog = [](const std::string&) {
        // Сетевой лог не дублируем в консоль — слишком многословно для CLI.
    };
    ev.onError = [this](const std::string& msg) {
        pushLine("[ошибка сети] " + msg);
    };
    ev.onReady = [this](const std::string& ip, uint16_t port, bool upnpOk) {
        pushLine("[сеть] готова: " + ip + ":" + std::to_string(port) +
                 (upnpOk ? " (UPnP ok)" : " (UPnP не удался)"));
    };
    ev.onIncomingRequest = [this](const std::string& uuid, const std::string& name, const std::string& ip) {
        pushLine("[запрос] " + name + " (" + shortUuid(uuid) + ", " + ip +
                 ") хочет добавиться — accept " + shortUuid(uuid) + " | reject " + shortUuid(uuid));
    };
    ev.onPeerConnected = [this](const std::string& uuid, const std::string& name) {
        pushLine("[+] " + name + " (" + shortUuid(uuid) + ") подключился");
        handlePeerConnected(uuid, name);
    };
    ev.onPeerDisconnected = [this](const std::string& uuid) {
        pushLine("[-] " + shortUuid(uuid) + " отключился");
    };
    ev.onMessage = [this](const std::string& uuid, const nlohmann::json& msg) {
        handleMessage(uuid, msg);
    };

    m_core.network().addListener(std::move(ev));
}

void CliSession::handlePeerConnected(const std::string& uuid, const std::string& /*name*/) {
    sendKeyBundle(uuid);
}

void CliSession::sendKeyBundle(const std::string& uuid) {
    nlohmann::json keyMsg;
    keyMsg["type"]   = "KEY_BUNDLE";
    keyMsg["bundle"] = m_core.e2e().ourBundleJson();
    m_core.network().sendFrame(uuid, keyMsg);
}

void CliSession::handleMessage(const std::string& from, const nlohmann::json& msg) {
    auto& e2e = m_core.e2e();
    auto& net = m_core.network();
    auto& storage = m_core.storage();

    const std::string type = msg.value("type", std::string{});

    if (type == "KEY_BUNDLE") {
        if (!e2e.hasSession(from)) {
            // Симметричный конфликт инициации разрешается сравнением UUID —
            // как в MainWindow::onMessageReceived.
            if (Identity::instance().uuid() < from) {
                const auto initMsg = e2e.initiateSession(from, msg.value("bundle", nlohmann::json::object()));
                if (!initMsg.empty())
                    net.sendFrame(from, initMsg);
            }
        }
        return;
    }
    if (type == "KEY_INIT") {
        const auto reply = e2e.acceptSession(from, msg);
        if (!reply.empty())
            net.sendFrame(from, reply);
        return;
    }
    if (type == "KEY_ACK" || type == "MSG_ACK") {
        return;
    }

    if (type == "CHAT") {
        if (storage.getContact(from).uuid.empty()) {
            const auto info = net.getPeerInfo(from);
            Contact tmp;
            tmp.uuid = from;
            tmp.name = info.name.empty() ? shortUuid(from) : info.name;
            tmp.ip   = info.ip;
            tmp.port = info.serverPort;
            (void)storage.addContact(tmp);
        }

        const auto plain = e2e.decrypt(from, msg);
        if (!plain.has_value()) {
            pushLine("[!] Не удалось расшифровать сообщение от " + shortUuid(from));
            return;
        }
        const std::string text(plain->begin(), plain->end());

        const std::string ackId = msg.value("msg_id", std::string{});
        if (!ackId.empty())
            net.sendFrame(from, nlohmann::json{{"type", "MSG_ACK"}, {"msg_id", ackId}});

        Message m;
        m.peerUuid  = from;
        m.outgoing  = false;
        m.text      = text;
        m.timestamp = nowMs();
        (void)storage.saveMessage(m);

        const Contact sender = storage.getContact(from);
        const std::string name = sender.name.empty() ? shortUuid(from) : sender.name;
        pushLine("[" + name + "] " + text);
        return;
    }

    // Остальные типы фреймов (файлы, звонки, шелл, группы и т.д.) в --cli
    // не обрабатываются — это минимальный текстовый режим обмена сообщениями.
}

void CliSession::printHelp() const {
    std::cout <<
        "Команды:\n"
        "  contacts                 — список контактов\n"
        "  send <N> <текст>         — отправить сообщение контакту №N\n"
        "  history <N> [лимит]      — последние сообщения с контактом №N\n"
        "  accept <uuid-префикс>    — принять входящий запрос\n"
        "  reject <uuid-префикс>    — отклонить входящий запрос\n"
        "  help                     — эта справка\n"
        "  quit / exit              — выход\n";
}

void CliSession::printContacts() const {
    const auto contacts = m_core.storage().allContacts();
    if (contacts.empty()) {
        std::cout << "Контактов нет.\n";
        return;
    }
    for (size_t i = 0; i < contacts.size(); ++i) {
        const auto& c = contacts[i];
        const bool online = m_core.network().isOnline(c.uuid);
        std::cout << "  [" << i << "] " << c.name
                  << " (" << shortUuid(c.uuid) << ") "
                  << (online ? "online" : "offline") << "\n";
    }
}

void CliSession::printHistory(int index, int limit) const {
    const auto contacts = m_core.storage().allContacts();
    if (index < 0 || static_cast<size_t>(index) >= contacts.size()) {
        std::cout << "Нет контакта с номером " << index << ".\n";
        return;
    }
    const auto& c = contacts[static_cast<size_t>(index)];
    auto messages = m_core.storage().getMessages(c.uuid, limit);
    for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
        std::cout << (it->outgoing ? "  > " : "  < ") << it->text << "\n";
    }
}

void CliSession::sendTextMessage(int index, const std::string& text) {
    const auto contacts = m_core.storage().allContacts();
    if (index < 0 || static_cast<size_t>(index) >= contacts.size()) {
        std::cout << "Нет контакта с номером " << index << ".\n";
        return;
    }
    const auto& c = contacts[static_cast<size_t>(index)];

    auto& e2e = m_core.e2e();
    if (!e2e.hasSession(c.uuid)) {
        std::cout << "Сессия шифрования с " << c.name
                  << " ещё не установлена. Подождите подключения и повторите.\n";
        return;
    }

    const Bytes plaintext(text.begin(), text.end());
    nlohmann::json env = e2e.encrypt(c.uuid, plaintext);
    if (env.empty()) {
        std::cout << "Не удалось зашифровать сообщение.\n";
        return;
    }
    const std::string msgId = randomMsgId();
    env["msg_id"] = msgId;
    m_core.network().sendFrame(c.uuid, env);

    Message m;
    m.peerUuid  = c.uuid;
    m.outgoing  = true;
    m.text      = text;
    m.timestamp = nowMs();
    (void)m_core.storage().saveMessage(m);

    std::cout << "  > " << text << "\n";
}

void CliSession::handleCommand(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd.empty()) return;

    if (cmd == "quit" || cmd == "exit") {
        m_running = false;
        return;
    }
    if (cmd == "help") {
        printHelp();
        return;
    }
    if (cmd == "contacts" || cmd == "list") {
        printContacts();
        return;
    }
    if (cmd == "send") {
        int index = -1;
        iss >> index;
        std::string text;
        std::getline(iss, text);
        // Убираем один ведущий пробел-разделитель после номера.
        if (!text.empty() && text.front() == ' ')
            text.erase(0, 1);
        if (text.empty()) {
            std::cout << "Использование: send <N> <текст>\n";
            return;
        }
        sendTextMessage(index, text);
        return;
    }
    if (cmd == "history") {
        int index = -1;
        int limit = 20;
        iss >> index;
        if (!(iss >> limit))
            limit = 20;
        printHistory(index, limit);
        return;
    }
    if (cmd == "accept" || cmd == "reject") {
        std::string prefix;
        iss >> prefix;
        for (const auto& c : m_core.storage().allContacts()) {
            if (c.uuid.rfind(prefix, 0) == 0) {
                if (cmd == "accept")
                    m_core.network().acceptIncoming(c.uuid);
                else
                    m_core.network().rejectIncoming(c.uuid);
                return;
            }
        }
        std::cout << "uuid не найден среди известных контактов: " << prefix << "\n";
        return;
    }

    std::cout << "Неизвестная команда: " << cmd << " (help — список команд)\n";
}

int CliSession::run() {
    std::cout << "naleystogramm --cli — текстовый режим. 'help' для справки, 'quit' для выхода.\n";

    // Читаем stdin в отдельном потоке, чтобы не блокировать вывод входящих
    // событий сети (NetworkManager работает в своём io_context-потоке).
    std::thread inputThread([this]() {
        std::string line;
        while (m_running && std::getline(std::cin, line))
            handleCommand(line);
        m_running = false;
    });

    while (m_running) {
        std::deque<std::string> lines;
        {
            std::lock_guard<std::mutex> lock(m_outMutex);
            lines.swap(m_pendingLines);
        }
        for (const auto& line : lines)
            std::cout << line << "\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (inputThread.joinable())
        inputThread.join();

    return 0;
}
