#include "customthememanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>

// Все 23 обязательных поля палитры
static const QStringList kRequiredPaletteFields = {
    "bg", "bgSurface", "bgElevated", "bgInput", "bgBubbleOut", "bgBubbleIn",
    "border", "borderFocus",
    "textPrimary", "textSecondary", "textMuted", "textOnAccent",
    "accent", "accentHover", "accentPressed",
    "online", "offline", "danger", "success",
    "bannerBg", "bannerBorder", "bannerText", "bannerBtnHover"
};

QString CustomThemeManager::themesDir() {
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/themes";
}

QList<CustomThemeMeta> CustomThemeManager::scan() {
    QList<CustomThemeMeta> result;
    const QDir dir(themesDir());
    if (!dir.exists()) return result;

    for (const QString& entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString themeDir = dir.filePath(entry);
        QFile f(themeDir + "/theme.json");
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        if (obj.isEmpty()) continue;
        const QString name = obj.value("name").toString();
        if (name.isEmpty()) continue;
        result.append({ entry, name, obj.value("author").toString() });
    }
    return result;
}

bool CustomThemeManager::validateThemeDir(const QString& dirPath, QString& outError) {
    QFile f(dirPath + "/theme.json");
    if (!f.open(QIODevice::ReadOnly)) {
        outError = "theme.json не найден";
        return false;
    }
    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    if (obj.value("name").toString().isEmpty()) {
        outError = "theme.json: поле \"name\" обязательно";
        return false;
    }
    const QJsonObject pal = obj.value("palette").toObject();
    for (const QString& field : kRequiredPaletteFields) {
        if (!pal.contains(field)) {
            outError = QString("theme.json: palette.%1 отсутствует").arg(field);
            return false;
        }
    }
    return true;
}

bool CustomThemeManager::importArchive(const QString& archivePath, QString& outError) {
    // Создаём папку тем если нет
    QDir destBase(themesDir());
    if (!destBase.exists()) destBase.mkpath(".");

    // Временная папка для распаковки
    const QString tmpPath = themesDir() + "/_import_tmp";
    QDir tmp(tmpPath);
    if (tmp.exists()) tmp.removeRecursively();
    tmp.mkpath(".");

    // Выбираем программу по расширению архива
    const QString lower = archivePath.toLower();
    QString program;
    QStringList args;

    if (lower.endsWith(".zip")) {
#ifdef Q_OS_WIN
        program = "powershell";
        args = { "-NoProfile", "-Command",
                 QString("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                     .arg(archivePath, tmpPath) };
#else
        program = "unzip";
        args = { "-o", archivePath, "-d", tmpPath };
#endif
    } else if (lower.endsWith(".tar.gz") || lower.endsWith(".tgz")) {
        program = "tar";
        args = { "xzf", archivePath, "-C", tmpPath };
    } else if (lower.endsWith(".7z")) {
        program = "7z";
        args = { "x", archivePath, "-o" + tmpPath, "-y" };
    } else {
        outError = "Неподдерживаемый формат. Поддерживаются: .zip, .tar.gz, .7z";
        tmp.removeRecursively();
        return false;
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(program, args);
    if (!proc.waitForFinished(30000)) {
        proc.kill();
        outError = QString("%1 не ответил в течение 30 секунд").arg(program);
        tmp.removeRecursively();
        return false;
    }
    if (proc.exitCode() != 0) {
        const QString errOut = QString::fromLocal8Bit(proc.readAll()).trimmed();
        outError = QString("Ошибка распаковки (%1): %2")
            .arg(program,
                 errOut.isEmpty() ? "код " + QString::number(proc.exitCode()) : errOut);
        tmp.removeRecursively();
        return false;
    }

    // Ищем папку с theme.json: сначала корень tmpPath, потом подпапки 1 уровня
    QString foundDir;
    if (QFile::exists(tmpPath + "/theme.json")) {
        foundDir = tmpPath;
    } else {
        for (const QString& sub : QDir(tmpPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString candidate = tmpPath + "/" + sub;
            if (QFile::exists(candidate + "/theme.json")) {
                foundDir = candidate;
                break;
            }
        }
    }

    if (foundDir.isEmpty()) {
        outError = "В архиве не найден theme.json";
        tmp.removeRecursively();
        return false;
    }

    QString validateError;
    if (!validateThemeDir(foundDir, validateError)) {
        outError = validateError;
        tmp.removeRecursively();
        return false;
    }

    // Имя папки назначения: имя подпапки из архива, или sanitized "name" если архив без подпапки
    QString folderName = QFileInfo(foundDir).fileName();
    if (folderName == "_import_tmp") {
        QFile f(foundDir + "/theme.json");
        if (!f.open(QIODevice::ReadOnly)) {
            outError = "Не удалось прочитать theme.json";
            tmp.removeRecursively();
            return false;
        }
        const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        folderName = obj.value("name").toString()
                         .toLower()
                         .replace(' ', '-')
                         .replace(QRegularExpression("[^a-z0-9_\\-]"), "");
        if (folderName.isEmpty()) folderName = "custom-theme";
    }

    const QString destPath = themesDir() + "/" + folderName;
    QDir destTheme(destPath);
    if (destTheme.exists()) destTheme.removeRecursively();

    // Перемещаем (rename на одном ФС работает для директорий)
    if (!QDir().rename(foundDir, destPath)) {
        outError = "Не удалось переместить тему в папку тем";
        tmp.removeRecursively();
        return false;
    }

    tmp.removeRecursively();
    return true;
}

bool CustomThemeManager::removeTheme(const QString& folderName) {
    return QDir(themesDir() + "/" + folderName).removeRecursively();
}
