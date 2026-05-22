#pragma once
#include "installer.h"

/*
 * Точка входа деинсталлятора.
 * Показывает диалог подтверждения, затем:
 *  1. Удаляет файлы из install_path (читается из реестра)
 *  2. Удаляет ярлыки
 *  3. Удаляет правило брандмауэра
 *  4. Удаляет запись из реестра
 *  5. Удаляет папку установки если пустая
 */
int run_uninstall(HINSTANCE hInst);

/* Запускается из %TEMP% — удаляет папку install_path после выхода родителя */
void run_uninstall_cleanup(const wchar_t *install_path);
