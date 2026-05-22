#pragma once
#include "installer.h"

/*
 * Создать ярлык .lnk через IShellLink COM.
 * target      — путь к .exe
 * lnk_path    — полный путь к создаваемому файлу .lnk
 * description — подсказка (tooltip)
 */
int shortcut_create(const wchar_t *target, const wchar_t *lnk_path,
                     const wchar_t *description);

/* Создать Desktop и/или Start Menu ярлыки */
int shortcuts_install(const wchar_t *install_path,
                       bool desktop, bool startmenu);

/* Удалить созданные ярлыки (при деинсталляции) */
int shortcuts_remove(void);
