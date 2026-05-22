#include "shortcuts.h"
#include "installer.h"
#include <shlobj.h>
#include <objbase.h>
#include <stdio.h>

/* Получить путь к специальной папке (CSIDL) */
static bool get_folder(int csidl, wchar_t *out, int max) {
    (void)max;
    return SUCCEEDED(SHGetFolderPathW(NULL, csidl, NULL, SHGFP_TYPE_CURRENT, out));
}

int shortcut_create(const wchar_t *target, const wchar_t *lnk_path,
                     const wchar_t *description) {
    IShellLinkW *psl = NULL;
    IPersistFile *ppf = NULL;
    int ret = -1;

    HRESULT hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_IShellLinkW, (void **)&psl);
    if (FAILED(hr)) return -1;

    psl->lpVtbl->SetPath(psl, target);
    if (description && description[0])
        psl->lpVtbl->SetDescription(psl, description);

    /* Рабочая папка — директория .exe */
    wchar_t work_dir[MAX_PATH];
    wcsncpy(work_dir, target, MAX_PATH - 1);
    work_dir[MAX_PATH - 1] = L'\0';
    wchar_t *sep = wcsrchr(work_dir, L'\\');
    if (sep) *sep = L'\0';
    psl->lpVtbl->SetWorkingDirectory(psl, work_dir);

    hr = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void **)&ppf);
    if (FAILED(hr)) goto done;

    hr = ppf->lpVtbl->Save(ppf, lnk_path, TRUE);
    if (SUCCEEDED(hr)) ret = 0;

done:
    if (ppf) ppf->lpVtbl->Release(ppf);
    if (psl) psl->lpVtbl->Release(psl);
    return ret;
}

int shortcuts_install(const wchar_t *install_path, bool desktop, bool startmenu) {
    wchar_t exe_path[MAX_PATH];
    _snwprintf(exe_path, MAX_PATH, L"%s\\%s", install_path, APP_EXE);

    if (desktop) {
        wchar_t desk[MAX_PATH];
        if (get_folder(CSIDL_COMMON_DESKTOPDIRECTORY, desk, MAX_PATH)) {
            wchar_t lnk[MAX_PATH];
            _snwprintf(lnk, MAX_PATH, L"%s\\" APP_NAME L".lnk", desk);
            shortcut_create(exe_path, lnk, APP_NAME L" — P2P мессенджер");
        }
    }

    if (startmenu) {
        wchar_t smenu[MAX_PATH];
        if (get_folder(CSIDL_COMMON_PROGRAMS, smenu, MAX_PATH)) {
            /* Создаём подпапку Naleystogramm в меню Пуск */
            wchar_t sm_dir[MAX_PATH];
            _snwprintf(sm_dir, MAX_PATH, L"%s\\" APP_NAME, smenu);
            CreateDirectoryW(sm_dir, NULL);

            wchar_t lnk[MAX_PATH];
            _snwprintf(lnk, MAX_PATH, L"%s\\" APP_NAME L".lnk", sm_dir);
            shortcut_create(exe_path, lnk, APP_NAME L" — P2P мессенджер");
        }
    }

    return 0;
}

int shortcuts_remove(void) {
    /* Рабочий стол */
    wchar_t desk[MAX_PATH];
    if (get_folder(CSIDL_COMMON_DESKTOPDIRECTORY, desk, MAX_PATH)) {
        wchar_t lnk[MAX_PATH];
        _snwprintf(lnk, MAX_PATH, L"%s\\" APP_NAME L".lnk", desk);
        DeleteFileW(lnk);
    }

    /* Меню Пуск */
    wchar_t smenu[MAX_PATH];
    if (get_folder(CSIDL_COMMON_PROGRAMS, smenu, MAX_PATH)) {
        wchar_t lnk[MAX_PATH];
        _snwprintf(lnk, MAX_PATH, L"%s\\" APP_NAME L"\\" APP_NAME L".lnk", smenu);
        DeleteFileW(lnk);
        wchar_t sm_dir[MAX_PATH];
        _snwprintf(sm_dir, MAX_PATH, L"%s\\" APP_NAME, smenu);
        RemoveDirectoryW(sm_dir);
    }

    return 0;
}
