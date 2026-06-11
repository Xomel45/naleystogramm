#include "uninstall.h"
#include "installer.h"
#include "registry.h"
#include "shortcuts.h"
#include "firewall.h"
#include "strings.h"
#include <shlobj.h>
#include <stdio.h>

/* ── Рекурсивное удаление директории ──────────────────────────────────────── */
static void rmdir_recursive(const wchar_t *dir) {
    wchar_t pattern[MAX_PATH];
    _snwprintf(pattern, MAX_PATH, L"%s\\*", dir);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) goto remove_self;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        wchar_t path[MAX_PATH];
        _snwprintf(path, MAX_PATH, L"%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            rmdir_recursive(path);
        else
            DeleteFileW(path);
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

remove_self:
    RemoveDirectoryW(dir);
}

/* ── Диалог подтверждения ─────────────────────────────────────────────────── */

int run_uninstall(HINSTANCE hInst) {
    (void)hInst;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    /* Читаем путь установки из реестра */
    wchar_t install_path[MAX_PATH] = {0};
    if (!registry_get_install_path(install_path, MAX_PATH)) {
        MessageBoxW(NULL,
            STR_UNINSTALL_NOTFOUND,
            STR_UNINSTALL_TITLE, MB_OK | MB_ICONWARNING);
        CoUninitialize();
        return 1;
    }

    /* Подтверждение */
    int ans = MessageBoxW(NULL,
        STR_UNINSTALL_CONFIRM,
        STR_UNINSTALL_TITLE,
        MB_YESNO | MB_ICONQUESTION);

    if (ans != IDYES) { CoUninitialize(); return 0; }

    bool had_error = false;

    /* 1. Ярлыки */
    shortcuts_remove();

    /* 2. Брандмауэр */
    firewall_remove_rule();

    /* 3. Автозапуск */
    {
        HKEY hkey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                0, KEY_WRITE, &hkey) == ERROR_SUCCESS) {
            RegDeleteValueW(hkey, APP_NAME);
            RegCloseKey(hkey);
        }
    }

    /* 4. Реестр (Add/Remove Programs) */
    if (registry_remove_uninstall() != 0)
        had_error = true;

    /*
     * 5. Удаление файлов.
     * Сам setup.exe сейчас запущен из install_path — его нельзя удалить
     * сразу. Используем трюк: копируем себя во %TEMP%, запускаем оттуда
     * с командой удалить install_path, и выходим.
     */
    wchar_t self[MAX_PATH], tmp_self[MAX_PATH];
    GetModuleFileNameW(NULL, self, MAX_PATH);
    GetTempPathW(MAX_PATH, tmp_self);
    wcscat(tmp_self, L"nal_uninstall_tmp.exe");

    CopyFileW(self, tmp_self, FALSE);

    /* Командная строка: tmp_self --uninstall-cleanup "path" */
    wchar_t cmd[MAX_PATH * 2 + 64];
    _snwprintf(cmd, sizeof(cmd) / sizeof(wchar_t),
               L"\"%s\" --uninstall-cleanup \"%s\"",
               tmp_self, install_path);

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        /* Если не удалось запустить — удаляем как можем сейчас */
        rmdir_recursive(install_path);
    } else {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    MessageBoxW(NULL,
        had_error ? STR_UNINSTALL_ERR : STR_UNINSTALL_DONE,
        STR_UNINSTALL_TITLE,
        MB_OK | (had_error ? MB_ICONWARNING : MB_ICONINFORMATION));

    CoUninitialize();
    return 0;
}

/*
 * --uninstall-cleanup "path"
 * Запускается из %TEMP% после закрытия основного окна деинсталлятора.
 * Ждёт 500 мс (оригинальный процесс завершится), удаляет папку, удаляет себя.
 */
void run_uninstall_cleanup(const wchar_t *install_path) {
    Sleep(500);
    rmdir_recursive(install_path);

    /* Удаляем себя (tmp файл из %TEMP%) */
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(NULL, self, MAX_PATH);

    /* MoveFileExW с MOVEFILE_DELAY_UNTIL_REBOOT не нужен — нас уже нет в PATH */
    wchar_t cmd[MAX_PATH + 32];
    _snwprintf(cmd, sizeof(cmd) / sizeof(wchar_t), L"cmd /c del /f \"%s\"", self);
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                   CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread)  CloseHandle(pi.hThread);
}
