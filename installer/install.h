#pragma once
#include "installer.h"

/*
 * Контекст установки, передаётся в рабочий поток.
 * Поле hwnd_main нужно для PostMessage.
 */
typedef struct {
    InstallMode  mode;
    wchar_t      install_path[MAX_PATH];
    bool         shortcut_desktop;
    bool         shortcut_startmenu;
    bool         firewall_rule;
    bool         autostart;
    HWND         hwnd_main;
} InstallContext;

/*
 * Точка входа рабочего потока (вызывается через CreateThread).
 * lpParam — указатель на InstallContext (живёт в стеке wizard).
 */
DWORD WINAPI install_thread(LPVOID lpParam);
