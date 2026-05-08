#ifdef _WIN32
#include "windowsfirewall.h"
#include <QProcess>
#include <QSettings>
#include <QMessageBox>
#include <QString>
#include <windows.h>
#include <shellapi.h>

bool WindowsFirewall::ruleExists(const QString& name) {
    QProcess p;
    p.start("netsh", {
        "advfirewall", "firewall", "show", "rule",
        QString("name=%1").arg(name)
    });
    p.waitForFinished(5000);
    return p.exitCode() == 0;
}

bool WindowsFirewall::addRule(const QString& name, quint16 tcpPort) {
    const QString params = QString(
        "advfirewall firewall add rule "
        "name=\"%1\" "
        "dir=in action=allow protocol=TCP "
        "localport=%2 enable=yes"
    ).arg(name).arg(tcpPort);

    SHELLEXECUTEINFOW sei{};
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_UNICODE;
    sei.lpVerb       = L"runas";
    sei.lpFile       = L"netsh";
    sei.lpParameters = reinterpret_cast<LPCWSTR>(params.utf16());
    sei.nShow        = SW_HIDE;

    if (!ShellExecuteExW(&sei)) return false;

    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 10000);
        DWORD code = 0;
        GetExitCodeProcess(sei.hProcess, &code);
        CloseHandle(sei.hProcess);
        return code == 0;
    }
    return true;
}

void WindowsFirewall::checkAndPrompt(QWidget* parent, quint16 tcpPort) {
    QSettings s;

    // Уже добавляли — не спрашиваем повторно
    if (s.value(QLatin1String(kSettingsKey), false).toBool()) return;

    // Правило уже существует (добавлено вручную или другим инсталлятором)
    if (ruleExists(QLatin1String(kRuleName))) {
        s.setValue(QLatin1String(kSettingsKey), true);
        return;
    }

    const auto btn = QMessageBox::question(
        parent,
        QObject::tr("Windows Firewall"),
        QObject::tr(
            "Naleystogramm needs to open port %1 (TCP) in Windows Firewall "
            "to accept incoming connections from other users.\n\n"
            "Add the firewall rule now?\n"
            "(Requires administrator approval)"
        ).arg(tcpPort),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
    );

    if (btn != QMessageBox::Yes) return;

    if (addRule(QLatin1String(kRuleName), tcpPort)) {
        s.setValue(QLatin1String(kSettingsKey), true);
        QMessageBox::information(
            parent,
            QObject::tr("Windows Firewall"),
            QObject::tr("Firewall rule added successfully.\n"
                        "Incoming connections on port %1 are now allowed.").arg(tcpPort)
        );
    } else {
        QMessageBox::warning(
            parent,
            QObject::tr("Windows Firewall"),
            QObject::tr("Failed to add firewall rule.\n\n"
                        "To add it manually, run as Administrator:\n"
                        "netsh advfirewall firewall add rule "
                        "name=\"Naleystogramm\" dir=in action=allow "
                        "protocol=TCP localport=%1 enable=yes").arg(tcpPort)
        );
    }
}

#endif // _WIN32
