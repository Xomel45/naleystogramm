#include "installer.h"
#include "strings.h"

int g_lang = LANG_RU;

void strings_init(void) {
    LANGID lid = GetUserDefaultUILanguage();
    WORD primary = PRIMARYLANGID(lid);
    /* Используем английский если язык системы не русский/украинский/белорусский */
    if (primary == LANG_RUSSIAN || primary == LANG_UKRAINIAN || primary == LANG_BELARUSIAN)
        g_lang = LANG_RU;
    else
        g_lang = LANG_EN;
}
