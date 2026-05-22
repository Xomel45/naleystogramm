#include "registry.h"
#include "installer.h"
#include <stdio.h>

/* Путь к uninstaller — installer.exe запускается с --uninstall */
static void build_uninstall_cmd(const wchar_t *install_path, wchar_t *out, int max) {
    _snwprintf(out, max, L"\"%s\\naleystogramm-setup.exe\" --uninstall",
               install_path);
}

int registry_add_uninstall(const wchar_t *install_path) {
    HKEY hkey;
    LONG res = RegCreateKeyExW(HKEY_LOCAL_MACHINE, REG_UNINSTALL_KEY, 0, NULL,
                                REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                                &hkey, NULL);
    if (res != ERROR_SUCCESS) return -1;

    wchar_t exe_path[MAX_PATH];
    _snwprintf(exe_path, MAX_PATH, L"%s\\%s", install_path, APP_EXE);

    wchar_t uninstall_cmd[MAX_PATH + 32];
    build_uninstall_cmd(install_path, uninstall_cmd, MAX_PATH + 32);

    wchar_t size_str[] = L"~150 MB";

    /* Стандартные поля Add/Remove Programs */
    struct { const wchar_t *name; const wchar_t *val; } str_vals[] = {
        { L"DisplayName",          APP_NAME },
        { L"DisplayVersion",       APP_VERSION },
        { L"Publisher",            APP_PUBLISHER },
        { L"URLInfoAbout",         APP_URL },
        { L"InstallLocation",      install_path },
        { L"DisplayIcon",          exe_path },
        { L"UninstallString",      uninstall_cmd },
        { L"EstimatedSize",        size_str },
        { L"NoModify",             NULL },   /* обрабатываем отдельно */
        { L"NoRepair",             NULL },
    };

    for (int i = 0; i < 8; i++) {
        RegSetValueExW(hkey, str_vals[i].name, 0, REG_SZ,
                        (const BYTE *)str_vals[i].val,
                        (DWORD)((wcslen(str_vals[i].val) + 1) * sizeof(wchar_t)));
    }

    /* DWORD-поля: NoModify=1, NoRepair=1 */
    DWORD one = 1;
    RegSetValueExW(hkey, L"NoModify", 0, REG_DWORD, (const BYTE *)&one, sizeof(DWORD));
    RegSetValueExW(hkey, L"NoRepair", 0, REG_DWORD, (const BYTE *)&one, sizeof(DWORD));

    RegCloseKey(hkey);
    return 0;
}

int registry_remove_uninstall(void) {
    LONG res = RegDeleteKeyW(HKEY_LOCAL_MACHINE, REG_UNINSTALL_KEY);
    return (res == ERROR_SUCCESS || res == ERROR_FILE_NOT_FOUND) ? 0 : -1;
}

bool registry_get_install_path(wchar_t *out_path, int max_len) {
    HKEY hkey;
    LONG res = RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_UNINSTALL_KEY, 0,
                              KEY_READ, &hkey);
    if (res != ERROR_SUCCESS) return false;

    DWORD type = REG_SZ;
    DWORD size = (DWORD)(max_len * sizeof(wchar_t));
    res = RegQueryValueExW(hkey, L"InstallLocation", NULL, &type,
                            (BYTE *)out_path, &size);
    RegCloseKey(hkey);
    return res == ERROR_SUCCESS && type == REG_SZ;
}
