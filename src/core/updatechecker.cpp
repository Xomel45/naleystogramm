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

// Читает /etc/os-release и возвращает {ID, ID_LIKE} в нижнем регистре.
// ID_LIKE может быть пустым или содержать несколько слов через пробел:
//   "ubuntu debian", "rhel centos fedora", "arch" и т.п.
// Благодаря ID_LIKE производные дистрибутивы покрываются автоматически —
// например, Pop!_OS имеет ID=pop, ID_LIKE="ubuntu debian".
static std::pair<QString,QString> detectOsRelease() {
    QString id, idLike;
    QFile f(QStringLiteral("/etc/os-release"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        const auto val = [&](const QString& key) -> QString {
            const QString prefix = key + '=';
            if (!line.startsWith(prefix)) return {};
            QString v = line.mid(prefix.length());
            if (v.startsWith('"')) v = v.mid(1, v.length() - 2);
            return v.toLower();
        };
        if (const auto v = val(QStringLiteral("ID"));      !v.isEmpty()) id     = v;
        if (const auto v = val(QStringLiteral("ID_LIKE")); !v.isEmpty()) idLike = v;
    }
    return {id, idLike};
}

// Проверяет принадлежность дистрибутива к семейству.
// Сначала ищет по ID, потом по каждому токену из ID_LIKE.
static bool inFamily(const QString& id, const QString& idLike, const QStringList& list) {
    if (list.contains(id)) return true;
    for (const auto& token : idLike.split(u' ', Qt::SkipEmptyParts))
        if (list.contains(token)) return true;
    return false;
}

// Выбирает лучший ассет из массива по типу дистрибутива.
// Возвращает {url, name}.
static std::pair<QString,QString> pickAsset(const QJsonArray& assets) {
    const auto [id, idLike] = detectOsRelease();

    QString appUrl, appName;
    QString debUrl, debName;
    QString rpmUrl, rpmName;
    QString pkgUrl, pkgName;

    for (const auto& a : assets) {
        const QJsonObject obj = a.toObject();
        const QString name    = obj["name"].toString();
        const QString url     = obj["browser_download_url"].toString();
        if      (name.endsWith(".AppImage"))        { appUrl = url; appName = name; }
        else if (name.endsWith(".deb"))              { debUrl = url; debName = name; }
        else if (name.endsWith(".rpm"))              { rpmUrl = url; rpmName = name; }
        else if (name.endsWith(".pkg.tar.zst"))      { pkgUrl = url; pkgName = name; }
    }

    // ── Arch и производные (pacman / .pkg.tar.zst) ───────────────────────────
    static const QStringList kArchFamily {
        // Базовый дистрибутив
        "arch",
        // Прямые производные
        "artix", "manjaro", "endeavouros", "garuda", "cachyos", "blackarch",
        "archlabs", "archcraft", "arcolinux", "arcolinuxb", "archman", "archstrike",
        "bluestar", "crystal", "ctlos", "obarun", "rebornos", "anarchy", "axyl",
        "steamos", "holo", "parabola", "hyperbola", "kaos", "alci", "blendos",
        "xerolinux", "athena", "instantos", "mabox", "biglinux", "linhes",
        "archbang", "alfheim", "librewish", "peux", "pojde", "prism", "archex",
        "archlinux32", "chakra", "archphile", "archi3", "holographos", "snal",
        "tarch", "archsway", "antergos", "apricity", "archarm", "alarm",
        "sirula", "parchlinux", "subliminal", "fenix", "m-os", "mesk",
        "easy-arch", "bridge", "archlinux-lxqt", "archlinux-arm",
        "xfce-openrc", "kde-openrc", "gnome-openrc",
        // SteamOS / gaming
        "chimeraos", "nobara-arch", "holoiso",
        // ARM / embedded
        "archlinuxarm", "alarm-rpi", "monjaro",
        // Прочие известные форки
        "archfi", "archinstall", "reclaimedlinux", "paloarch", "vanilla-arch",
        "arch-anywhere", "archdi", "archdi-portable", "architect", "zen-installer",
        "archlinux-studio",
    };

    // ── Debian и производные (apt/dpkg / .deb) ───────────────────────────────
    static const QStringList kDebianFamily {
        // Базовые
        "debian", "ubuntu",
        // Ubuntu-семейство
        "kubuntu", "lubuntu", "xubuntu", "ubuntu-mate", "ubuntu-budgie",
        "ubuntukylin", "ubuntu-studio", "edubuntu", "ubuntu-unity",
        "ubuntu-cinnamon", "ubuntu-gnome", "ubuntu-xfce", "ubuntu-openbox",
        "ubuntu-sway", "ubuntu-web", "ubuntu-touch",
        // Mint
        "linuxmint", "lmde",
        // Kali / Parrot / пентест
        "kali", "parrot", "blackbox", "backbox", "tails",
        // Elementary / Pantheon
        "elementary",
        // Zorin
        "zorin",
        // Pop!_OS
        "pop", "popos", "pop_os",
        // Deepin / UOS / Kylin
        "deepin", "uos", "kylin", "ubuntukylin", "neokylin",
        // MX Linux / antiX
        "mx", "mxlinux", "antix",
        // Devuan (systemd-free Debian)
        "devuan",
        // PureOS / Trisquel / Libre
        "pureos", "trisquel", "parabola-gnulinux",
        // Raspberry Pi OS / ARM
        "raspbian", "raspios", "raspberry-pi-os", "raspberry",
        // Armbian и встраиваемые
        "armbian", "dietpi", "dietpi-os", "mobian", "rpi-os",
        // Sparky / Siduction / Neptune
        "sparky", "siduction", "neptune", "spiral",
        // BunsenLabs / CrunchBang
        "bunsenlabs", "crunchbang", "crunchbangplusplus",
        // Bodhi / Moksha
        "bodhi",
        // Knoppix
        "knoppix",
        // SolydXK / SolydK / SolydX
        "solydxk", "solydk", "solydx",
        // Netrunner
        "netrunner",
        // Nitrux / NX Desktop
        "nitrux",
        // Regolith
        "regolith",
        // Q4OS / TDE
        "q4os",
        // Peppermint
        "peppermint",
        // Linux Lite
        "lite", "linuxlite", "linux-lite",
        // KDE Neon
        "neon", "kdeneon",
        // Endless OS
        "endless",
        // Astra Linux
        "astra", "astra-linux",
        // Whonix
        "whonix",
        // OpenMediaVault / Proxmox
        "openmediavault", "proxmox", "pve",
        // GRML
        "grml",
        // Kanotix
        "kanotix",
        // PinguyOS / Linux Lite / LXLE
        "pinguyos", "lxle",
        // Voyager / Emmabuntüs
        "voyager", "emmabuntus",
        // Drauger OS (gaming)
        "drauger",
        // LinuxFX
        "linuxfx",
        // MakuluLinux
        "makulu",
        // Bento
        "bento",
        // Subgraph OS
        "subgraph",
        // Watt OS
        "watt-os",
        // Hefftor
        "hefftor",
        // TuxedoOS
        "tuxedo",
        // Volumio / audio
        "volumio",
        // Turnkey Linux
        "turnkey",
        // Jolicloud
        "jolicloud",
        // SkoleLinux / Debian Edu
        "skolelinux", "debian-edu",
        // Vinux (accessibility)
        "vinux",
        // Ylmf OS
        "ylmf",
        // GeckoLinux (openSUSE spin — но с dpkg?)  — нет, он RPM
        // NimbleX
        "nimbleux",
        // Exe GNU/Linux
        "exe",
        // Storm Linux
        "storm",
        // Libranet
        "libranet",
        // Progeny
        "progeny",
        // Xandros
        "xandros",
        // LinEx
        "linex",
        // Corel Linux
        "corel",
        // Fluxbuntu
        "fluxbuntu",
        // Ubuntu-based gaming
        "nobara-ubuntu", "bazzite-ubuntu",
        // RisiOS
        "risiOS-debian",
        // CloudReady / ChromeOS Flex (Chromium OS, Debian base)
        "chromeos-flex", "cloudready",
        // DietPi
        "dietpi",
        // Raspberry Pi Desktop
        "rpd",
        // PiOS / PiOS Lite
        "pios", "pios-lite",
        // Kali Purple / NetHunter
        "kali-last-snapshot", "kali-rolling",
        // Ubuntu Server / Ubuntu Cloud / Ubuntu Core
        "ubuntu-server", "ubuntu-cloud", "ubuntu-core",
    };

    // ── RPM и производные (rpm/dnf/zypper / .rpm) ────────────────────────────
    static const QStringList kRpmFamily {
        // Fedora / Red Hat
        "fedora", "rhel", "centos", "centos-stream",
        // Rocky / Alma / Oracle / Scientific — RHEL-клоны
        "rocky", "almalinux", "oracle", "oraclelinux", "scientific", "sl",
        "springdale", "eurolinux", "clearos", "cloudlinux",
        // SUSE семейство
        "opensuse", "opensuse-leap", "opensuse-tumbleweed", "opensuse-microos",
        "opensuse-kubic", "microos", "suse", "sle", "sled", "sles",
        "geckolinux",
        // Mageia / Mandriva потомки
        "mageia", "openmandriva", "rosa", "pclinuxos", "turbolinux", "vine",
        // ALT Linux (российский)
        "alt", "altlinux", "alt-server", "alt-workstation", "alt-education",
        "alt-kworkstation", "simply-linux",
        // Amazon Linux
        "amzn", "amazon", "amazonlinux",
        // Nobara / игровые
        "nobara",
        // Ultramarine
        "ultramarine",
        // Universal Blue (atomic desktops)
        "bazzite", "aurora", "bluefin", "onyx", "sericea", "vauxite",
        "lazos", "ublue", "universalblue",
        // Fedora Atomic / Silverblue / Kinoite
        "silverblue", "kinoite", "fedora-silverblue", "fedora-kinoite",
        "fedora-sericea", "fedora-onyx",
        // CoreOS / FCOS / RHCOS
        "coreos", "fcos", "rhcos", "fedora-coreos",
        // Flatcar (Container Linux)
        "flatcar",
        // COS (Google Container-Optimized OS)
        "cos",
        // Asahi Linux (Apple Silicon)
        "asahi", "asahi-linux",
        // Qubes OS (базируется на Fedora)
        "qubes",
        // Circle Linux (CentOS Stream клон)
        "circle",
        // NavyNix / Berry
        "navynix", "berry",
        // RisiOS
        "risios",
        // OpenCloud / Anolis / OpenEuler / TencentOS (китайские)
        "opencloudos", "anolis", "openeuler", "alinux", "tencentos",
        "tencentlinux", "bigcloud", "openanolis", "eci",
        // Miracle Linux (Asianux)
        "miraclelinux",
        // VineLinux / Turbolinux (японские)
        "vine", "turbolinux",
        // Pidora (Raspberry Pi Fedora)
        "pidora",
        // EL clones и прочие
        "eln", "rl", "el", "cs",
        // Photon OS (VMware)
        "photon",
        // CBL-Mariner / Azure Linux (Microsoft)
        "mariner", "azurelinux",
        // Fedora IoT
        "fedora-iot", "iot",
        // RHEL for Edge / MicroShift
        "rhel-edge",
        // Navy Linux
        "navy",
        // Eurolinux
        "eurolinux",
        // OpenMandriva Lx
        "openmandriva-lx",
        // ROSA Fresh / Chrome
        "rosa-fresh", "rosa-chrome", "rosa-desktop",
    };

    if (inFamily(id, idLike, kArchFamily)  && !pkgUrl.isEmpty()) return {pkgUrl, pkgName};
    if (inFamily(id, idLike, kDebianFamily) && !debUrl.isEmpty()) return {debUrl, debName};
    if (inFamily(id, idLike, kRpmFamily)   && !rpmUrl.isEmpty()) return {rpmUrl, rpmName};
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
