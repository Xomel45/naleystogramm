#pragma once
#include <QObject>
#include <QString>
#include <QIcon>
#include <QSize>

class QAbstractButton;

enum class Theme {
    Dark,      // Тёмная — глубокие тёмные тона, акценты насыщенные
    Light,     // Светлая — яркие, воздушные, сочные акценты
    BW,        // Ч/Б — только чёрный, белый и серые
    Forest,    // Лес — тёмно-зелёные тона, природные акценты
    Cyberpunk, // Киберпанк — неоновый фиолетовый, тёмный фон
    Nordic,    // Нордик — холодный синий/серый, скандинавский минимализм
    Sunset,    // Закат — тёплый розово-оранжевый, уютная атмосфера
    Legacy,    // Легаси — тёмная палитра эпохи 0.1.0–0.5.0
    Custom     // Пользовательская тема из ~/.cache/.../themes/<folder>/
};

// ── Палитра одной темы ────────────────────────────────────────────────────

struct ThemePalette {
    // Фоны
    QString bg;           // основной фон
    QString bgSurface;    // карточки, панели
    QString bgElevated;   // элементы поверх surface (хедер, инпут)
    QString bgInput;      // поле ввода
    QString bgBubbleOut;  // исходящий пузырь
    QString bgBubbleIn;   // входящий пузырь

    // Границы
    QString border;
    QString borderFocus;

    // Текст
    QString textPrimary;
    QString textSecondary;
    QString textMuted;
    QString textOnAccent;

    // Акцент
    QString accent;
    QString accentHover;
    QString accentPressed;

    // Статусы
    QString online;       // зелёный / аналог
    QString offline;      // серый
    QString danger;       // красный / аналог
    QString success;      // зелёный подтверждение

    // Баннер обновления
    QString bannerBg;     // фон баннера
    QString bannerBorder; // граница / кнопка
    QString bannerText;   // текст
    QString bannerBtnHover; // ховер кнопки
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager& instance();

    void        setTheme(Theme t);
    [[nodiscard]] Theme       currentTheme()     const noexcept { return m_theme; }
    [[nodiscard]] QString     currentThemeName() const;
    [[nodiscard]] const ThemePalette& palette()  const noexcept { return m_palette; }
    [[nodiscard]] QString     stylesheet()       const;

    // Загружает пользовательскую тему из папки по имени (внутри themesDir).
    // Возвращает true при успехе; при ошибке палитра не меняется.
    bool loadCustomTheme(const QString& folderName);

    // Возвращает иконку, перекрашенную в цвет textPrimary текущей темы.
    // Ожидает white-on-transparent PNG (наш стандарт). На тёмных темах
    // иконка остаётся белой, на светлых — становится тёмной автоматически.
    static QIcon tintedIcon(const QString& path);

    // Перегрузка с явным цветом — для иконок на произвольном фоне.
    static QIcon tintedIcon(const QString& path, const QColor& color);

    // Устанавливает иконку на кнопку и подключает авто-обновление при смене темы.
    // Иконка красится в textPrimary — для прозрачных/нейтральных кнопок (iconBtn).
    static void applyIcon(QAbstractButton* btn, const QString& path, const QSize& sz);

    // То же, но иконка красится в textOnAccent — для кнопок с акцентным фоном
    // (sendBtn, addContactBtn и т.п.). Гарантирует читаемость на любой теме.
    static void applyIconOnAccent(QAbstractButton* btn, const QString& path, const QSize& sz);

    [[nodiscard]] QString customThemeFolderName()  const noexcept { return m_customFolderName; }
    [[nodiscard]] QString customThemeDisplayName() const noexcept { return m_customDisplayName; }
    [[nodiscard]] QString customThemeBgMain()      const noexcept { return m_customBgMain; }

signals:
    void themeChanged(Theme newTheme);

private:
    ThemeManager();
    void applyPalette();
    static ThemePalette darkPalette();
    static ThemePalette lightPalette();
    static ThemePalette bwPalette();
    static ThemePalette forestPalette();
    static ThemePalette cyberpunkPalette();
    static ThemePalette nordicPalette();
    static ThemePalette sunsetPalette();
    static ThemePalette legacyPalette();

    Theme        m_theme{Theme::Dark};
    ThemePalette m_palette;

    // Данные активной пользовательской темы (пусто если не Custom)
    QString m_customFolderName;
    QString m_customDisplayName;
    QString m_customCss;      // содержимое css/main.css (пусто → генерируем из палитры)
    QString m_customBgMain;   // абсолютный путь к backgrounds/bg_main.png (пусто → нет фона)
};
