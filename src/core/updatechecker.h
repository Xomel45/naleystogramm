#pragma once
#include <QObject>
#include <QString>
#include "types.h"

// ── UpdateChecker ──────────────────────────────────────────────────────────
// Проверяет новую версию через GitHub Releases API.
// Использование:
//   auto* uc = new UpdateChecker(this);
//   uc->checkInBackground();  // тихая проверка при старте
//   uc->checkNow();           // явная проверка из UI

// UpdateInfo перенесён в types.h

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    // Поменяй на своё GitHub repo
    static constexpr const char* kGitHubOwner   = "Xomel45";
    static constexpr const char* kGitHubRepo    = "naleystogramm";
    static constexpr const char* kCurrentVersion = "0.7.3";

    explicit UpdateChecker(QObject* parent = nullptr);

    // Тихая проверка при старте — не спамит если проверяли недавно (< 6 часов)
    void checkInBackground();

    // Явная проверка из UI — всегда делает запрос
    void checkNow();

    [[nodiscard]] QString currentVersion() const { return kCurrentVersion; }
    [[nodiscard]] QString lastChecked()    const;
    [[nodiscard]] UpdateInfo cachedResult() const { return m_cached; }

signals:
    void updateAvailable(UpdateInfo info);
    void noUpdateAvailable(const QString& currentVersion);
    void checkFailed(const QString& error);
    void checkStarted();

private:
    void doCheck();
    static bool isNewerVersion(const QString& remote, const QString& local);

    UpdateInfo m_cached;
};
