#pragma once
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Версия приложения ─────────────────────────────────────────── */
#ifndef APP_VERSION
#define APP_VERSION L"0.7.4"
#endif

#define APP_NAME        L"Naleystogramm"
#define APP_EXE         L"naleystogramm.exe"
#define APP_PUBLISHER   L"xomel45"
#define APP_URL         L"https://github.com/xomel45/naleystogramm"
#define APP_PORT        47821

/* Ключ в реестре для Установка и удаление программ */
#define REG_UNINSTALL_KEY \
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Naleystogramm"

/* ── ID ресурсов (installer.rc) ────────────────────────────────── */
#define IDI_APP         101
#define IDR_PAYLOAD     102   /* RCDATA — встроенный payload.zip    */
#define IDR_MANIFEST    1     /* RT_MANIFEST — UAC requireAdmin     */

/* ── WM_APP сообщения (поток → UI) ────────────────────────────── */
#define WM_INSTALL_PROGRESS  (WM_APP + 1)  /* wParam=0..100              */
#define WM_INSTALL_LOG       (WM_APP + 2)  /* lParam=(wchar_t*)строка    */
#define WM_INSTALL_DONE      (WM_APP + 3)  /* wParam=1 успех, 0 ошибка  */

/* ── Страницы wizard ───────────────────────────────────────────── */
typedef enum {
    PAGE_WELCOME = 0,
    PAGE_PATH,
    PAGE_OPTIONS,
    PAGE_PROGRESS,
    PAGE_FINISH,
    PAGE_COUNT
} WizardPage;

/* ── Режим установки ───────────────────────────────────────────── */
typedef enum {
    MODE_BUNDLE   = 0,   /* распаковать встроенный payload.zip    */
    MODE_DOWNLOAD = 1,   /* скачать последнюю версию с GitHub     */
} InstallMode;

/* ── Общее состояние инсталлера ────────────────────────────────── */
typedef struct {
    InstallMode  mode;
    wchar_t      install_path[MAX_PATH];
    bool         shortcut_desktop;
    bool         shortcut_startmenu;
    bool         firewall_rule;
    bool         autostart;
    bool         launch_after;
    HWND         hwnd_main;       /* главное окно wizard             */
    HWND         hwnd_progress;   /* прогресс-бар на странице 4      */
    HWND         hwnd_log;        /* лог-бокс на странице 4          */
    bool         install_success;
    bool         cancelled;
} InstallerState;

extern InstallerState g_state;

/* ── Утилиты ───────────────────────────────────────────────────── */
void log_msg(const wchar_t *fmt, ...);   /* пишет в лог-бокс + stderr  */
