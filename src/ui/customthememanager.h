#pragma once
#include <QString>
#include <QList>

// ── Метаданные одной пользовательской темы ────────────────────────────────

struct CustomThemeMeta {
    QString folderName;   // имя папки внутри themes/
    QString displayName;  // поле "name" из theme.json
    QString author;       // поле "author" из theme.json (может быть пустым)
};

// ── CustomThemeManager ────────────────────────────────────────────────────
// Статический помощник: сканирует ~/.cache/naleystogramm/naleystogramm/themes/,
// импортирует архивы (.zip / .tar.gz / .7z) и удаляет пользовательские темы.

class CustomThemeManager {
public:
    // Полный путь к папке пользовательских тем
    static QString themesDir();

    // Сканирует themesDir() — возвращает все валидные темы
    static QList<CustomThemeMeta> scan();

    // Импортирует архив (.zip / .tar.gz / .7z) в themesDir()
    // Возвращает true при успехе; outError — текст ошибки для UI
    static bool importArchive(const QString& archivePath, QString& outError);

    // Удаляет тему по имени папки. Возвращает true при успехе.
    static bool removeTheme(const QString& folderName);

private:
    // Проверяет что dirPath содержит валидный theme.json со всеми 23 полями palette
    static bool validateThemeDir(const QString& dirPath, QString& outError);
};
