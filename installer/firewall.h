#pragma once
#include "installer.h"

/*
 * Добавить правило входящего TCP-соединения для Naleystogramm на порту APP_PORT.
 * exe_path — полный путь к .exe (для привязки правила к программе).
 */
int firewall_add_rule(const wchar_t *exe_path);

/* Удалить правило брандмауэра (при деинсталляции). */
int firewall_remove_rule(void);
