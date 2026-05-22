/*
 * installer.c — Naleystogramm Installer
 * Win32 wizard UI (5 страниц), рабочий поток установки.
 */
#include "installer.h"
#include "install.h"
#include "uninstall.h"
#include "strings.h"
#include <shlobj.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>

/* ── Размеры окна ──────────────────────────────────────────────────────────── */
#define WND_W  520
#define WND_H  430
#define HDR_H  80    /* высота заголовка */
#define BTN_H  40    /* высота полосы кнопок внизу */
#define MARGIN 20

/* ── ID дочерних элементов ─────────────────────────────────────────────────── */
#define ID_BTN_BACK      1001
#define ID_BTN_NEXT      1002
#define ID_BTN_CANCEL    1003
#define ID_RAD_BUNDLE    1010
#define ID_RAD_DOWNLOAD  1011
#define ID_EDIT_PATH     1020
#define ID_BTN_BROWSE    1021
#define ID_CHK_DESKTOP   1030
#define ID_CHK_START     1031
#define ID_CHK_FIREWALL  1032
#define ID_CHK_AUTOSTART 1033
#define ID_PROGRESS      1040
#define ID_LOG           1041
#define ID_CHK_LAUNCH    1050

/* ── Цвета (тёмный заголовок как у приложения) ─────────────────────────────── */
#define CLR_HEADER_BG    RGB(30,  30,  40)
#define CLR_HEADER_TITLE RGB(220, 220, 255)
#define CLR_HEADER_SUB   RGB(160, 160, 200)
#define CLR_BODY_BG      RGB(248, 248, 252)
#define CLR_FOOTER_BG    RGB(230, 230, 238)
#define CLR_ACCENT       RGB(80,  120, 220)

/* ── Состояние ─────────────────────────────────────────────────────────────── */
static WizardPage    s_page = PAGE_WELCOME;
static HFONT         s_font_title;
static HFONT         s_font_body;
static HFONT         s_font_small;
static HBRUSH        s_brush_header;
static HBRUSH        s_brush_body;
static HBRUSH        s_brush_footer;
static HWND          s_hwnd_pages[PAGE_COUNT];   /* контейнеры страниц */
static InstallContext s_ictx;
static bool          s_installing = false;

/* ── Форматирование размера ─────────────────────────────────────────────────── */
static const wchar_t *page_title(WizardPage p) {
    switch (p) {
        case PAGE_WELCOME:  return STR_P1_TITLE;
        case PAGE_PATH:     return STR_P2_TITLE;
        case PAGE_OPTIONS:  return STR_P3_TITLE;
        case PAGE_PROGRESS: return STR_P4_TITLE;
        case PAGE_FINISH:   return STR_P5_TITLE;
        default:            return L"";
    }
}

static const wchar_t *page_subtitle(WizardPage p) {
    switch (p) {
        case PAGE_WELCOME:  return STR_P1_SUB;
        case PAGE_PATH:     return STR_P2_SUB;
        case PAGE_OPTIONS:  return STR_P3_SUB;
        case PAGE_PROGRESS: return s_installing ? STR_P4_SUB : STR_P4_DONE_OK;
        case PAGE_FINISH:   return g_state.install_success ? L"" : STR_P5_FAIL;
        default:            return L"";
    }
}

/* ── Создание шрифтов ──────────────────────────────────────────────────────── */
static HFONT make_font(int size, bool bold) {
    return CreateFontW(-size, 0, 0, 0,
                        bold ? FW_BOLD : FW_NORMAL,
                        FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

/* ── Вспомогательный макрос создания контрола ─────────────────────────────── */
static HWND make_ctrl(const wchar_t *cls, const wchar_t *text,
                       DWORD style, int x, int y, int w, int h,
                       HWND parent, HMENU id, HFONT font) {
    HWND hw = CreateWindowExW(0, cls, text,
                               WS_CHILD | WS_VISIBLE | style,
                               x, y, w, h,
                               parent, id, GetModuleHandleW(NULL), NULL);
    if (font) SendMessageW(hw, WM_SETFONT, (WPARAM)font, TRUE);
    return hw;
}

/* ── Обновить заголовок (перерисовка) ────────────────────────────────────── */
static void update_header(HWND hwnd) {
    RECT rc = {0, 0, WND_W, HDR_H};
    InvalidateRect(hwnd, &rc, FALSE);
}

/* ── Переключить страницу ────────────────────────────────────────────────── */
static void show_page(HWND hwnd, WizardPage page) {
    for (int i = 0; i < PAGE_COUNT; i++)
        ShowWindow(s_hwnd_pages[i], i == (int)page ? SW_SHOW : SW_HIDE);
    s_page = page;
    update_header(hwnd);

    /* Кнопки */
    HWND hBack   = GetDlgItem(hwnd, ID_BTN_BACK);
    HWND hNext   = GetDlgItem(hwnd, ID_BTN_NEXT);
    HWND hCancel = GetDlgItem(hwnd, ID_BTN_CANCEL);

    EnableWindow(hBack,   page > PAGE_WELCOME && page < PAGE_PROGRESS);
    SetWindowTextW(hNext, page == PAGE_OPTIONS  ? STR_INSTALL
                         : page == PAGE_FINISH  ? STR_FINISH
                                                : STR_NEXT);
    EnableWindow(hNext,   page != PAGE_PROGRESS);
    EnableWindow(hCancel, page != PAGE_FINISH);
    ShowWindow(hCancel,   page != PAGE_FINISH ? SW_SHOW : SW_HIDE);
}

/* ══════════════════════════════════════════════════════════════════════════
   СТРАНИЦА 1: Добро пожаловать
   ══════════════════════════════════════════════════════════════════════════ */
static HWND create_page_welcome(HWND parent) {
    int y0 = HDR_H + 10;
    int cw = WND_W - MARGIN * 2;
    HWND pg = CreateWindowExW(0, L"STATIC", NULL,
                               WS_CHILD | SS_BLACKRECT,
                               0, HDR_H, WND_W, WND_H - HDR_H - BTN_H,
                               parent, NULL, GetModuleHandleW(NULL), NULL);

    make_ctrl(L"STATIC", STR_P1_BODY,
               SS_LEFT | SS_WORDELLIPSIS,
               MARGIN, y0, cw, 60, pg, NULL, s_font_body);
    y0 += 70;

    make_ctrl(L"BUTTON", STR_P1_BUNDLE,
               BS_AUTORADIOBUTTON | WS_GROUP,
               MARGIN, y0, cw, 22, pg, (HMENU)ID_RAD_BUNDLE, s_font_body);
    make_ctrl(L"STATIC", STR_P1_BUNDLE_SUB,
               SS_LEFT,
               MARGIN + 18, y0 + 22, cw - 18, 18, pg, NULL, s_font_small);
    y0 += 50;

    make_ctrl(L"BUTTON", STR_P1_DOWNLOAD,
               BS_AUTORADIOBUTTON,
               MARGIN, y0, cw, 22, pg, (HMENU)ID_RAD_DOWNLOAD, s_font_body);
    make_ctrl(L"STATIC", STR_P1_DL_SUB,
               SS_LEFT,
               MARGIN + 18, y0 + 22, cw - 18, 18, pg, NULL, s_font_small);

    /* Выбрать bundle по умолчанию */
    CheckRadioButton(pg, ID_RAD_BUNDLE, ID_RAD_DOWNLOAD, ID_RAD_BUNDLE);
    return pg;
}

/* ══════════════════════════════════════════════════════════════════════════
   СТРАНИЦА 2: Папка установки
   ══════════════════════════════════════════════════════════════════════════ */
static HWND create_page_path(HWND parent) {
    int y0 = HDR_H + 15;
    int cw = WND_W - MARGIN * 2;
    HWND pg = CreateWindowExW(0, L"STATIC", NULL,
                               WS_CHILD | SS_BLACKRECT,
                               0, HDR_H, WND_W, WND_H - HDR_H - BTN_H,
                               parent, NULL, GetModuleHandleW(NULL), NULL);

    make_ctrl(L"STATIC", STR_P2_LABEL,
               SS_LEFT, MARGIN, y0, cw, 20, pg, NULL, s_font_body);
    y0 += 24;

    make_ctrl(L"EDIT", g_state.install_path,
               WS_BORDER | ES_AUTOHSCROLL,
               MARGIN, y0, cw - 90, 24, pg, (HMENU)ID_EDIT_PATH, s_font_body);
    make_ctrl(L"BUTTON", STR_BROWSE,
               0,
               WND_W - MARGIN - 80, y0, 80, 24, pg, (HMENU)ID_BTN_BROWSE, s_font_body);
    y0 += 36;

    make_ctrl(L"STATIC", STR_P2_SPACE,
               SS_LEFT, MARGIN, y0, cw, 20, pg, NULL, s_font_small);
    return pg;
}

/* ══════════════════════════════════════════════════════════════════════════
   СТРАНИЦА 3: Параметры
   ══════════════════════════════════════════════════════════════════════════ */
static HWND create_page_options(HWND parent) {
    int y0 = HDR_H + 15;
    int cw = WND_W - MARGIN * 2;
    HWND pg = CreateWindowExW(0, L"STATIC", NULL,
                               WS_CHILD | SS_BLACKRECT,
                               0, HDR_H, WND_W, WND_H - HDR_H - BTN_H,
                               parent, NULL, GetModuleHandleW(NULL), NULL);

    make_ctrl(L"BUTTON", STR_P3_DESKTOP,
               BS_AUTOCHECKBOX | WS_GROUP, MARGIN, y0, cw, 24,
               pg, (HMENU)ID_CHK_DESKTOP, s_font_body);
    y0 += 32;
    make_ctrl(L"BUTTON", STR_P3_START,
               BS_AUTOCHECKBOX, MARGIN, y0, cw, 24,
               pg, (HMENU)ID_CHK_START, s_font_body);
    y0 += 32;
    make_ctrl(L"BUTTON", STR_P3_FIREWALL,
               BS_AUTOCHECKBOX, MARGIN, y0, cw, 24,
               pg, (HMENU)ID_CHK_FIREWALL, s_font_body);
    y0 += 32;
    make_ctrl(L"BUTTON", STR_P3_AUTOSTART,
               BS_AUTOCHECKBOX, MARGIN, y0, cw, 24,
               pg, (HMENU)ID_CHK_AUTOSTART, s_font_body);

    /* Значения по умолчанию */
    CheckDlgButton(pg, ID_CHK_DESKTOP,  BST_CHECKED);
    CheckDlgButton(pg, ID_CHK_START,    BST_CHECKED);
    CheckDlgButton(pg, ID_CHK_FIREWALL, BST_CHECKED);
    return pg;
}

/* ══════════════════════════════════════════════════════════════════════════
   СТРАНИЦА 4: Прогресс
   ══════════════════════════════════════════════════════════════════════════ */
static HWND create_page_progress(HWND parent) {
    int y0 = HDR_H + 15;
    int cw = WND_W - MARGIN * 2;
    HWND pg = CreateWindowExW(0, L"STATIC", NULL,
                               WS_CHILD | SS_BLACKRECT,
                               0, HDR_H, WND_W, WND_H - HDR_H - BTN_H,
                               parent, NULL, GetModuleHandleW(NULL), NULL);

    HWND hProg = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
                                  WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                  MARGIN, y0, cw, 20, pg,
                                  (HMENU)ID_PROGRESS, GetModuleHandleW(NULL), NULL);
    SendMessageW(hProg, WM_SETFONT, (WPARAM)s_font_body, TRUE);
    SendMessageW(hProg, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    g_state.hwnd_progress = hProg;
    y0 += 30;

    HWND hLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", NULL,
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                 LBS_NOSEL | LBS_NOINTEGRALHEIGHT,
                                 MARGIN, y0, cw,
                                 WND_H - HDR_H - BTN_H - y0 - 10,
                                 pg, (HMENU)ID_LOG,
                                 GetModuleHandleW(NULL), NULL);
    SendMessageW(hLog, WM_SETFONT, (WPARAM)s_font_small, TRUE);
    g_state.hwnd_log = hLog;
    return pg;
}

/* ══════════════════════════════════════════════════════════════════════════
   СТРАНИЦА 5: Финиш
   ══════════════════════════════════════════════════════════════════════════ */
static HWND create_page_finish(HWND parent) {
    int y0 = HDR_H + 20;
    int cw = WND_W - MARGIN * 2;
    HWND pg = CreateWindowExW(0, L"STATIC", NULL,
                               WS_CHILD | SS_BLACKRECT,
                               0, HDR_H, WND_W, WND_H - HDR_H - BTN_H,
                               parent, NULL, GetModuleHandleW(NULL), NULL);

    make_ctrl(L"STATIC", STR_P5_OK,
               SS_LEFT | SS_WORDELLIPSIS,
               MARGIN, y0, cw, 80, pg, NULL, s_font_body);
    y0 += 90;

    make_ctrl(L"BUTTON", STR_P5_LAUNCH,
               BS_AUTOCHECKBOX, MARGIN, y0, cw, 24,
               pg, (HMENU)ID_CHK_LAUNCH, s_font_body);
    CheckDlgButton(pg, ID_CHK_LAUNCH, BST_CHECKED);
    return pg;
}

/* ══════════════════════════════════════════════════════════════════════════
   WM_PAINT для шапки
   ══════════════════════════════════════════════════════════════════════════ */
static void paint_header(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    /* Фон шапки */
    RECT rc_hdr = {0, 0, WND_W, HDR_H};
    FillRect(hdc, &rc_hdr, s_brush_header);

    /* Заголовок страницы */
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_HEADER_TITLE);
    SelectObject(hdc, s_font_title);
    RECT rc_title = {MARGIN, 14, WND_W - MARGIN, HDR_H / 2};
    DrawTextW(hdc, page_title(s_page), -1, &rc_title,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* Подзаголовок */
    SetTextColor(hdc, CLR_HEADER_SUB);
    SelectObject(hdc, s_font_body);
    RECT rc_sub = {MARGIN + 10, HDR_H / 2, WND_W - MARGIN, HDR_H - 10};
    DrawTextW(hdc, page_subtitle(s_page), -1, &rc_sub,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* Разделительная линия */
    HPEN pen = CreatePen(PS_SOLID, 1, CLR_ACCENT);
    HPEN old = SelectObject(hdc, pen);
    MoveToEx(hdc, 0, HDR_H - 1, NULL);
    LineTo(hdc, WND_W, HDR_H - 1);
    SelectObject(hdc, old);
    DeleteObject(pen);

    /* Фон тела */
    RECT rc_body = {0, HDR_H, WND_W, WND_H - BTN_H};
    FillRect(hdc, &rc_body, s_brush_body);

    /* Фон кнопочной полосы */
    RECT rc_footer = {0, WND_H - BTN_H, WND_W, WND_H};
    FillRect(hdc, &rc_footer, s_brush_footer);

    EndPaint(hwnd, &ps);
}

/* ══════════════════════════════════════════════════════════════════════════
   Диалог выбора папки (Browse)
   ══════════════════════════════════════════════════════════════════════════ */
static void do_browse(HWND hwnd) {
    BROWSEINFOW bi = {0};
    bi.hwndOwner = hwnd;
    bi.lpszTitle = STR_P2_LABEL;
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            wchar_t full[MAX_PATH];
            _snwprintf(full, MAX_PATH, L"%s\\" APP_NAME, path);
            SetDlgItemTextW(GetParent(GetParent(hwnd)),
                             ID_EDIT_PATH, full);
            /* Обновляем и g_state */
            wcsncpy(g_state.install_path, full, MAX_PATH - 1);
        }
        CoTaskMemFree(pidl);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
   Сбор данных со страниц при нажатии Next
   ══════════════════════════════════════════════════════════════════════════ */
static void collect_page_data(HWND hwnd) {
    (void)hwnd;
    HWND pg = s_hwnd_pages[s_page];
    switch (s_page) {
    case PAGE_WELCOME:
        g_state.mode = (IsDlgButtonChecked(pg, ID_RAD_BUNDLE) == BST_CHECKED)
                       ? MODE_BUNDLE : MODE_DOWNLOAD;
        break;
    case PAGE_PATH: {
        wchar_t buf[MAX_PATH];
        GetDlgItemTextW(pg, ID_EDIT_PATH, buf, MAX_PATH);
        wcsncpy(g_state.install_path, buf, MAX_PATH - 1);
        break;
    }
    case PAGE_OPTIONS:
        g_state.shortcut_desktop  = IsDlgButtonChecked(pg, ID_CHK_DESKTOP)  == BST_CHECKED;
        g_state.shortcut_startmenu= IsDlgButtonChecked(pg, ID_CHK_START)    == BST_CHECKED;
        g_state.firewall_rule     = IsDlgButtonChecked(pg, ID_CHK_FIREWALL) == BST_CHECKED;
        g_state.autostart         = IsDlgButtonChecked(pg, ID_CHK_AUTOSTART)== BST_CHECKED;
        break;
    default: break;
    }
}

/* ══════════════════════════════════════════════════════════════════════════
   Запустить установку
   ══════════════════════════════════════════════════════════════════════════ */
static void start_installation(HWND hwnd) {
    s_ictx.mode             = g_state.mode;
    s_ictx.shortcut_desktop = g_state.shortcut_desktop;
    s_ictx.shortcut_startmenu = g_state.shortcut_startmenu;
    s_ictx.firewall_rule    = g_state.firewall_rule;
    s_ictx.autostart        = g_state.autostart;
    s_ictx.hwnd_main        = hwnd;
    wcsncpy(s_ictx.install_path, g_state.install_path, MAX_PATH - 1);
    g_state.hwnd_main       = hwnd;

    s_installing = true;
    HANDLE hThread = CreateThread(NULL, 0, install_thread, &s_ictx, 0, NULL);
    if (hThread) CloseHandle(hThread);
}

/* ══════════════════════════════════════════════════════════════════════════
   WndProc
   ══════════════════════════════════════════════════════════════════════════ */
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT:
        paint_header(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;   /* фон рисуем сами в WM_PAINT */

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        /* Белый фон для контролов на теле страницы */
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, CLR_BODY_BG);
        return (LRESULT)s_brush_body;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        switch (id) {
        case ID_BTN_CANCEL:
            if (s_installing) {
                g_state.cancelled = true;
            } else {
                DestroyWindow(hwnd);
            }
            break;

        case ID_BTN_BROWSE:
            do_browse(hwnd);
            break;

        case ID_BTN_BACK:
            if (s_page > PAGE_WELCOME)
                show_page(hwnd, (WizardPage)(s_page - 1));
            break;

        case ID_BTN_NEXT:
            collect_page_data(hwnd);
            if (s_page == PAGE_OPTIONS) {
                show_page(hwnd, PAGE_PROGRESS);
                start_installation(hwnd);
            } else if (s_page == PAGE_FINISH) {
                if (IsDlgButtonChecked(s_hwnd_pages[PAGE_FINISH],
                                        ID_CHK_LAUNCH) == BST_CHECKED) {
                    wchar_t exe[MAX_PATH];
                    _snwprintf(exe, MAX_PATH, L"%s\\%s",
                               g_state.install_path, APP_EXE);
                    ShellExecuteW(NULL, L"open", exe, NULL,
                                   g_state.install_path, SW_SHOWNORMAL);
                }
                DestroyWindow(hwnd);
            } else {
                show_page(hwnd, (WizardPage)(s_page + 1));
            }
            break;
        }
        return 0;
    }

    case WM_INSTALL_PROGRESS:
        if (g_state.hwnd_progress)
            SendMessageW(g_state.hwnd_progress, PBM_SETPOS, wp, 0);
        return 0;

    case WM_INSTALL_LOG: {
        wchar_t *text = (wchar_t *)lp;
        if (g_state.hwnd_log && text) {
            int idx = (int)SendMessageW(g_state.hwnd_log, LB_ADDSTRING, 0, (LPARAM)text);
            SendMessageW(g_state.hwnd_log, LB_SETTOPINDEX, idx, 0);
            free(text);
        }
        return 0;
    }

    case WM_INSTALL_DONE:
        s_installing = false;
        g_state.install_success = (wp != 0);
        update_header(hwnd);   /* обновляем subtitle на "Установка завершена" */
        show_page(hwnd, PAGE_FINISH);
        /* Обновляем текст финишной страницы в зависимости от результата */
        if (!g_state.install_success) {
            HWND pg = s_hwnd_pages[PAGE_FINISH];
            HWND hStatic = GetWindow(pg, GW_CHILD);
            if (hStatic) SetWindowTextW(hStatic, STR_P5_FAIL);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ══════════════════════════════════════════════════════════════════════════
   Создание главного окна
   ══════════════════════════════════════════════════════════════════════════ */
static HWND create_main_window(HINSTANCE hInst) {
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"NaleystogrammInstaller";
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP));
    RegisterClassExW(&wc);

    /* Центрирование на экране */
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    int wx = (sx - WND_W) / 2;
    int wy = (sy - WND_H) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"NaleystogrammInstaller",
        STR_TITLE,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        wx, wy, WND_W, WND_H,
        NULL, NULL, hInst, NULL);

    if (!hwnd) return NULL;

    /* Шрифты */
    s_font_title = make_font(16, true);
    s_font_body  = make_font(11, false);
    s_font_small = make_font(9,  false);

    /* Кисти */
    s_brush_header = CreateSolidBrush(CLR_HEADER_BG);
    s_brush_body   = CreateSolidBrush(CLR_BODY_BG);
    s_brush_footer = CreateSolidBrush(CLR_FOOTER_BG);

    /* Кнопки внизу */
    int btn_y = WND_H - BTN_H + (BTN_H - 26) / 2;
    make_ctrl(L"BUTTON", STR_BACK,   0,
               WND_W - 310, btn_y, 90, 26,
               hwnd, (HMENU)ID_BTN_BACK,   s_font_body);
    make_ctrl(L"BUTTON", STR_NEXT,   BS_DEFPUSHBUTTON,
               WND_W - 210, btn_y, 90, 26,
               hwnd, (HMENU)ID_BTN_NEXT,   s_font_body);
    make_ctrl(L"BUTTON", STR_CANCEL, 0,
               WND_W - 110, btn_y, 90, 26,
               hwnd, (HMENU)ID_BTN_CANCEL, s_font_body);

    /* Страницы */
    s_hwnd_pages[PAGE_WELCOME]  = create_page_welcome(hwnd);
    s_hwnd_pages[PAGE_PATH]     = create_page_path(hwnd);
    s_hwnd_pages[PAGE_OPTIONS]  = create_page_options(hwnd);
    s_hwnd_pages[PAGE_PROGRESS] = create_page_progress(hwnd);
    s_hwnd_pages[PAGE_FINISH]   = create_page_finish(hwnd);

    /* Путь установки по умолчанию */
    wchar_t pf[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL,
                                    SHGFP_TYPE_CURRENT, pf))) {
        _snwprintf(g_state.install_path, MAX_PATH, L"%s\\" APP_NAME, pf);
    } else {
        wcscpy(g_state.install_path, L"C:\\Program Files\\" APP_NAME);
    }
    /* Установить текст edit box на странице PATH */
    SetDlgItemTextW(hwnd, ID_EDIT_PATH, g_state.install_path);

    show_page(hwnd, PAGE_WELCOME);
    return hwnd;
}

/* ══════════════════════════════════════════════════════════════════════════
   WinMain
   ══════════════════════════════════════════════════════════════════════════ */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hPrev; (void)lpCmd;

    /* Общие контролы (progress bar, listbox) */
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icc);

    /* Определяем язык */
    strings_init();

    /* Парсим аргументы командной строки */
    int argc;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argc >= 2 && wcscmp(argv[1], L"--uninstall") == 0) {
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        strings_init();
        int r = run_uninstall(hInst);
        CoUninitialize();
        LocalFree(argv);
        return r;
    }

    if (argc >= 3 && wcscmp(argv[1], L"--uninstall-cleanup") == 0) {
        run_uninstall_cleanup(argv[2]);
        LocalFree(argv);
        return 0;
    }

    LocalFree(argv);

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    HWND hwnd = create_main_window(hInst);
    if (!hwnd) { CoUninitialize(); return 1; }

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    /* Очистка ресурсов */
    DeleteObject(s_font_title);
    DeleteObject(s_font_body);
    DeleteObject(s_font_small);
    DeleteObject(s_brush_header);
    DeleteObject(s_brush_body);
    DeleteObject(s_brush_footer);

    CoUninitialize();
    return (int)msg.wParam;
}
