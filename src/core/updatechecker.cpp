#include "updatechecker.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "sessionmanager.h"
#include <QDateTime>
#include <QUrl>
#include <QFile>

// ── Helpers ───────────────────────────────────────────────────────────────

// Возвращает ID= из /etc/os-release (например, "arch", "ubuntu", "fedora").
static QString detectDistroId() {
    QFile f("/etc/os-release");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    while (!f.atEnd()) {
        QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.startsWith("ID=")) {
            QString val = line.mid(3);
            if (val.startsWith('"')) val = val.mid(1, val.length() - 2);
            return val.toLower();
        }
    }
    return {};
}

// Выбирает лучший ассет из массива по типу дистрибутива.
// Возвращает {url, name}.
static std::pair<QString,QString> pickAsset(const QJsonArray& assets) {
    const QString distro = detectDistroId();

    QString appUrl, appName;
    QString debUrl, debName;
    QString rpmUrl, rpmName;
    QString pkgUrl, pkgName;

    for (const auto& a : assets) {
        const QJsonObject obj  = a.toObject();
        const QString name     = obj["name"].toString();
        const QString url      = obj["browser_download_url"].toString();
        if (name.endsWith(".AppImage"))       { appUrl = url; appName = name; }
        else if (name.endsWith(".deb"))        { debUrl = url; debName = name; }
        else if (name.endsWith(".rpm"))        { rpmUrl = url; rpmName = name; }
        else if (name.endsWith(".pkg.tar.zst")){ pkgUrl = url; pkgName = name; }
    }

    static const QStringList archBased    {"arch","manjaro","endeavouros","garuda","artix","cachyos"};
    static const QStringList debianBased  {"ubuntu","debian","linuxmint","pop","zorin","kali","elementary"};
    static const QStringList rpmBased     {"fedora","rhel","centos","rocky","almalinux",
                                           "opensuse","opensuse-leap","opensuse-tumbleweed"};

    if (archBased.contains(distro)  && !pkgUrl.isEmpty()) return {pkgUrl, pkgName};
    if (debianBased.contains(distro) && !debUrl.isEmpty()) return {debUrl, debName};
    if (rpmBased.contains(distro)   && !rpmUrl.isEmpty()) return {rpmUrl, rpmName};
    return {appUrl, appName};
}

// Парсим semver "v1.2.3", "1.2.3-beta", "1.2.3-rc1" → {1, 2, 3}.
// Суффиксы (-beta, -rc1 и т.п.) отбрасываются перед разбором чисел.
static std::tuple<int,int,int> parseSemver(const QString& v) {
    QString s = v;
    // Убираем ведущий 'v' или 'V'
    if (s.startsWith('v') || s.startsWith('V'))
        s = s.mid(1);
    // Отбрасываем всё после первого дефиса (-beta, -rc1, -stable и т.п.)
    const int dashPos = s.indexOf('-');
    if (dashPos != -1)
        s = s.left(dashPos);
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
    if (!SessionManager::instance().autoCheckUpdates())
        return;
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

        const auto [dlUrl, dlName] = pickAsset(obj["assets"].toArray());

        m_cached = UpdateInfo{
            .version     = tagName,
            .url         = htmlUrl,
            .notes       = notes,
            .downloadUrl = dlUrl,
            .assetName   = dlName,
            .available   = isNewerVersion(tagName, kCurrentVersion),
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
