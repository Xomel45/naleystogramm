#pragma once
#include <string>

namespace glui {

// Qt-free порт ThemePalette (src/ui/thememanager.h) — те же 23 поля,
// но std::string вместо QString. Значения палитр Dark/Light портированы
// из src/ui/thememanager.cpp (darkPalette/lightPalette), сам файл не менялся.
struct ThemePalette {
    // Backgrounds
    std::string bg;
    std::string bgSurface;
    std::string bgElevated;
    std::string bgInput;
    std::string bgBubbleOut;
    std::string bgBubbleIn;

    // Borders
    std::string border;
    std::string borderFocus;

    // Text
    std::string textPrimary;
    std::string textSecondary;
    std::string textMuted;
    std::string textOnAccent;

    // Accent
    std::string accent;
    std::string accentHover;
    std::string accentPressed;

    // Status
    std::string online;
    std::string offline;
    std::string danger;
    std::string success;

    // Banner
    std::string bannerBg;
    std::string bannerBorder;
    std::string bannerText;
    std::string bannerBtnHover;
};

enum class Theme {
    Dark,
    Light,
};

[[nodiscard]] ThemePalette paletteFor(Theme theme);

} // namespace glui
