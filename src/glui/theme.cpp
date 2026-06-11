#include "theme.h"

namespace glui {

static ThemePalette darkPalette() {
    ThemePalette p;
    p.bg          = "#080810";
    p.bgSurface   = "#0e0e1c";
    p.bgElevated  = "#13132a";
    p.bgInput     = "#18182e";
    p.bgBubbleOut = "#2a1f5e";
    p.bgBubbleIn  = "#161628";

    p.border      = "#1e1e3a";
    p.borderFocus = "#6c5ce7";

    p.textPrimary   = "#ece9ff";
    p.textSecondary = "#b8b4d8";
    p.textMuted     = "#5a5880";
    p.textOnAccent  = "#ffffff";

    p.accent        = "#6c5ce7";
    p.accentHover   = "#8677ff";
    p.accentPressed = "#5449c4";

    p.online  = "#00cba9";
    p.offline = "#3a3a5c";
    p.danger  = "#ff4d6d";
    p.success = "#00cba9";

    p.bannerBg       = "#1e1b3a";
    p.bannerBorder   = "#6c5ce7";
    p.bannerText     = "#e2e2f0";
    p.bannerBtnHover = "#8075e5";
    return p;
}

static ThemePalette lightPalette() {
    ThemePalette p;
    p.bg          = "#f7f5ff";
    p.bgSurface   = "#ffffff";
    p.bgElevated  = "#f0eeff";
    p.bgInput     = "#ffffff";
    p.bgBubbleOut = "#ff6b35";
    p.bgBubbleIn  = "#ffffff";

    p.border      = "#e2deff";
    p.borderFocus = "#ff6b35";

    p.textPrimary   = "#1a1035";
    p.textSecondary = "#4a3f6b";
    p.textMuted     = "#9b92b8";
    p.textOnAccent  = "#ffffff";

    p.accent        = "#ff6b35";
    p.accentHover   = "#ff8555";
    p.accentPressed = "#e5521c";

    p.online  = "#00b894";
    p.offline = "#c5bfe8";
    p.danger  = "#d63031";
    p.success = "#00b894";

    p.bannerBg       = "#fff3e0";
    p.bannerBorder   = "#ff6b35";
    p.bannerText     = "#2d1a0e";
    p.bannerBtnHover = "#ff8c5a";
    return p;
}

ThemePalette paletteFor(Theme theme) {
    switch (theme) {
        case Theme::Light: return lightPalette();
        case Theme::Dark:
        default:           return darkPalette();
    }
}

} // namespace glui
