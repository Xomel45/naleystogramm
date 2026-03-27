#pragma once
#include <QObject>
#include <QString>

enum class Theme {
    Dark,      // Тёмная — глубокие тёмные тона, акценты насыщенные
    Light,     // Светлая — яркие, воздушные, сочные акценты
    BW,        // Ч/Б — только чёрный, белый и серые
    Forest,    // Лес — тёмно-зелёные тона, природные акценты
    Cyberpunk, // Киберпанк — неоновый фиолетовый, тёмный фон
    Nordic,    // Нордик — холодный синий/серый, скандинавский минимализм
    Sunset     // Закат — тёплый розово-оранжевый, уютная атмосфера
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

    Theme        m_theme{Theme::Dark};
    ThemePalette m_palette;
};
