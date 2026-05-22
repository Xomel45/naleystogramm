#include "install.h"
#include "installer.h"
#include "extract.h"
#include "download.h"
#include "registry.h"
#include "shortcuts.h"
#include "firewall.h"
#include "strings.h"
#include <objbase.h>
#include <stdio.h>

InstallerState g_state;

/* ── Вспомогательные функции ─────────────────────────────────────────────── */

void log_msg(const wchar_t *fmt, ...) {
    wchar_t buf[1024];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf(buf, 1024, fmt, args);
    va_end(args);

    /* В лог-бокс через PostMessage (безопасно из любого потока) */
    if (g_state.hwnd_main) {
        wchar_t *copy = (wchar_t *)malloc((wcslen(buf) + 1) * sizeof(wchar_t));
        if (copy) {
            wcscpy(copy, buf);
            PostMessageW(g_state.hwnd_main, WM_INSTALL_LOG, 0, (LPARAM)copy);
        }
    }

    fwprintf(stderr, L"[INSTALL] %s\n", buf);
}

static void set_progress(HWND hwnd, int pct) {
    PostMessageW(hwnd, WM_INSTALL_PROGRESS, (WPARAM)pct, 0);
}

/* ── Коллбек прогресса распаковки ───────────────────────────────────────── */

typedef struct { HWND hwnd; int base_pct; int range_pct; int total; } ExtractCtx;
static ExtractCtx s_ext_ctx;

static void extract_progress_cb(int current, int total, const wchar_t *filename) {
    (void)filename;
    if (total <= 0) return;
    int pct = s_ext_ctx.base_pct
              + (int)((long long)current * s_ext_ctx.range_pct / total);
    set_progress(s_ext_ctx.hwnd, pct);
}

/* ── Коллбек прогресса загрузки ──────────────────────────────────────────── */
static HWND s_dl_hwnd;
static void download_progress_cb(uint64_t done, uint64_t total) {
    if (total == 0) return;
    int pct = (int)(done * 50 / total);   /* загрузка = 0..50 % */
    set_progress(s_dl_hwnd, pct);
}

/* ── Запись пути в autostart ─────────────────────────────────────────────── */
static void autostart_add(const wchar_t *install_path) {
    HKEY hkey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_WRITE, &hkey) == ERROR_SUCCESS) {
        wchar_t val[MAX_PATH + 4];
        _snwprintf(val, MAX_PATH + 4, L"\"%s\\%s\"", install_path, APP_EXE);
        RegSetValueExW(hkey, APP_NAME, 0, REG_SZ,
                        (const BYTE *)val,
                        (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)));
        RegCloseKey(hkey);
    }
}

static void autostart_remove(void) {
    HKEY hkey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_WRITE, &hkey) == ERROR_SUCCESS) {
        RegDeleteValueW(hkey, APP_NAME);
        RegCloseKey(hkey);
    }
}

/* ── Рабочий поток установки ─────────────────────────────────────────────── */

DWORD WINAPI install_thread(LPVOID lpParam) {
    InstallContext *ctx = (InstallContext *)lpParam;
    HWND hwnd = ctx->hwnd_main;
    bool ok = true;
    wchar_t tmp_zip[MAX_PATH] = {0};

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    /* Создаём папку установки */
    if (!makedirs(ctx->install_path)) {
        log_msg(L"%s%s", STR_LOG_ERROR, L"Не удалось создать папку установки");
        ok = false; goto done;
    }
    set_progress(hwnd, 5);

    if (ctx->mode == MODE_BUNDLE) {
        /* ── Режим: распаковать встроенный payload ──────────────────────── */
        log_msg(STR_LOG_EXTRACTING);

        HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(IDR_PAYLOAD), RT_RCDATA);
        if (!hRes) {
            log_msg(L"%s%s", STR_LOG_ERROR, L"Ресурс payload не найден");
            ok = false; goto done;
        }
        HGLOBAL hData = LoadResource(NULL, hRes);
        const void *pData = LockResource(hData);
        DWORD data_size = SizeofResource(NULL, hRes);

        s_ext_ctx.hwnd      = hwnd;
        s_ext_ctx.base_pct  = 5;
        s_ext_ctx.range_pct = 85;
        s_ext_ctx.total     = 0;

        if (extract_zip_from_memory(pData, data_size,
                                    ctx->install_path,
                                    extract_progress_cb) != 0) {
            log_msg(L"%s%s", STR_LOG_ERROR, L"Ошибка распаковки архива");
            ok = false; goto done;
        }
    } else {
        /* ── Режим: скачать последнюю версию ───────────────────────────── */
        log_msg(STR_LOG_DOWNLOADING);
        s_dl_hwnd = hwnd;
        if (download_latest_release(tmp_zip, download_progress_cb) != 0) {
            log_msg(L"%s%s", STR_LOG_ERROR, L"Не удалось скачать релиз");
            ok = false; goto done;
        }
        log_msg(STR_LOG_EXTRACTING);

        s_ext_ctx.hwnd      = hwnd;
        s_ext_ctx.base_pct  = 50;
        s_ext_ctx.range_pct = 40;
        s_ext_ctx.total     = 0;

        if (extract_zip_from_file(tmp_zip, ctx->install_path,
                                   extract_progress_cb) != 0) {
            log_msg(L"%s%s", STR_LOG_ERROR, L"Ошибка распаковки скачанного архива");
            ok = false; goto done;
        }
    }
    set_progress(hwnd, 90);

    /* ── Копируем installer в папку установки (для --uninstall) ───────── */
    {
        wchar_t self[MAX_PATH], dest[MAX_PATH];
        GetModuleFileNameW(NULL, self, MAX_PATH);
        _snwprintf(dest, MAX_PATH, L"%s\\naleystogramm-setup.exe",
                   ctx->install_path);
        CopyFileW(self, dest, FALSE);
    }

    /* ── Ярлыки ───────────────────────────────────────────────────────── */
    if (ctx->shortcut_desktop || ctx->shortcut_startmenu) {
        log_msg(STR_LOG_SHORTCUTS);
        shortcuts_install(ctx->install_path,
                          ctx->shortcut_desktop, ctx->shortcut_startmenu);
    }

    /* ── Брандмауэр ────────────────────────────────────────────────────── */
    if (ctx->firewall_rule) {
        log_msg(STR_LOG_FIREWALL);
        wchar_t exe_path[MAX_PATH];
        _snwprintf(exe_path, MAX_PATH, L"%s\\%s", ctx->install_path, APP_EXE);
        firewall_add_rule(exe_path);
    }

    /* ── Автозапуск ────────────────────────────────────────────────────── */
    if (ctx->autostart)
        autostart_add(ctx->install_path);

    /* ── Реестр (Add/Remove Programs) ───────────────────────────────────── */
    log_msg(STR_LOG_REGISTRY);
    registry_add_uninstall(ctx->install_path);
    set_progress(hwnd, 100);

    log_msg(STR_LOG_DONE);

done:
    if (tmp_zip[0]) download_cleanup(tmp_zip);
    CoUninitialize();
    PostMessageW(hwnd, WM_INSTALL_DONE, ok ? 1 : 0, 0);
    return 0;
}
