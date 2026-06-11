#include "installer.h"
#include "strings.h"

int g_lang = LANG_RU;

void strings_init(void) {
    LANGID lid = GetUserDefaultUILanguage();
    WORD primary = PRIMARYLANGID(lid);
    switch (primary) {
        case LANG_RUSSIAN:    g_lang = LANG_RU; break;
        case LANG_GERMAN:     g_lang = LANG_DE; break;
        case LANG_UKRAINIAN:  g_lang = LANG_UK; break;
        case LANG_BELARUSIAN: g_lang = LANG_BE; break;
        default:              g_lang = LANG_EN; break;
    }
}
