#pragma once
#include "installer.h"

/*
 * Добавить запись об установке в HKLM\...\Uninstall\Naleystogramm.
 * install_path — папка, куда установлено приложение.
 */
int registry_add_uninstall(const wchar_t *install_path);

/* Удалить запись из реестра. */
int registry_remove_uninstall(void);

/* Прочитать install_path из реестра (для деинсталлятора). */
bool registry_get_install_path(wchar_t *out_path, int max_len);
