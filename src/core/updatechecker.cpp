#include "updatechecker.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include "sessionmanager.h"
#include <QDateTime>
#include <QUrl>

// ── Helpers ───────────────────────────────────────────────────────────────

// Парсим semver "v1.2.3" или "1.2.3" → {1, 2, 3}
static std::tuple<int,int,int> parseSemver(const QString& v) {
    QString s = v;
    if (s.startsWith('v') || s.startsWith('V'))
        s = s.mid(1);
    const auto parts = s.split('.');
    const int major = parts.value(0).toInt();
    const int minor = parts.value(1).toInt();
    const int patch = parts.value(2).toInt();
    return {major, minor, patch};
}

// ── UpdateChecker ─────────────────────────────────────────────────────────

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent) {}

QString UpdateChecker::lastChecked() const {
    const QString iso = SessionManager::instance().lastUpdateCheck();
    if (iso.isEmpty()) return "никогда";
    const auto dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid()) return "никогда";
    return dt.toString("dd.MM.yyyy  hh:mm");
}

void UpdateChecker::checkInBackground() {
    const QString isoStr = SessionManager::instance().lastUpdateCheck();
    const auto last = isoStr.isEmpty()
        ? QDateTime()
        : QDateTime::fromString(isoStr, Qt::ISODate);
    // Не дёргаем чаще раза в 6 часов
    if (last.isValid() && last.secsTo(QDateTime::currentDateTime()) < 6 * 3600)
        return;
    doCheck();
}

void UpdateChecker::checkNow() {
    doCheck();
}

void UpdateChecker::doCheck() {
    emit checkStarted();

    const QString url = QString(
        "https://api.github.com/repos/%1/%2/releases/latest")
        .arg(kGitHubOwner, kGitHubRepo);

    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req{QUrl{url}};
    // GitHub требует User-Agent
    req.setRawHeader("User-Agent",
        QByteArray("naleystogramm/") + kCurrentVersion);
    req.setRawHeader("Accept",
        "application/vnd.github+json");
    req.setTransferTimeout(8000);

    auto* reply = nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();

        // Сохраняем время проверки в любом случае
        SessionManager::instance().setLastUpdateCheck(
            QDateTime::currentDateTime().toString(Qt::ISODate));

        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(reply->errorString());
            return;
        }

        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isNull() || !doc.isObject()) {
            emit checkFailed("Неверный ответ от GitHub API");
            return;
        }

        const QJsonObject obj  = doc.object();
        const QString tagName  = obj["tag_name"].toString();      // "v1.2.3"
        const QString htmlUrl  = obj["html_url"].toString();      // страница релиза
        const QString body     = obj["body"].toString();          // release notes

        if (tagName.isEmpty()) {
            emit checkFailed("Релизы не найдены");
            return;
        }

        // Обрезаем notes до ~280 символов
        const QString notes = body.length() > 280
            ? body.left(280) + "…"
            : body;

        m_cached = UpdateInfo{
            .version   = tagName,
            .url       = htmlUrl,
            .notes     = notes,
            .available = isNewerVersion(tagName, kCurrentVersion),
        };

        if (m_cached.available)
            emit updateAvailable(m_cached);
        else
            emit noUpdateAvailable(kCurrentVersion);
    });
}

bool UpdateChecker::isNewerVersion(const QString& remote, const QString& local) {
    const auto [rMaj, rMin, rPat] = parseSemver(remote);
    const auto [lMaj, lMin, lPat] = parseSemver(local);

    if (rMaj != lMaj) return rMaj > lMaj;
    if (rMin != lMin) return rMin > lMin;
    return rPat > lPat;
}
