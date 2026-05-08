#pragma once
#ifdef _WIN32

#include <QtGlobal>
class QWidget;

class WindowsFirewall {
public:
    // Проверяет наличие правила и предлагает добавить его если отсутствует.
    // Вызывать один раз после того как главное окно стало видимым.
    static void checkAndPrompt(QWidget* parent, quint16 tcpPort);

private:
    static bool ruleExists(const QString& name);
    static bool addRule(const QString& name, quint16 tcpPort);

    static constexpr const char* kRuleName   = "Naleystogramm";
    static constexpr const char* kSettingsKey = "firewall/rule_added";
};

#endif // _WIN32
