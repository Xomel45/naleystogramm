#pragma once
#include <windows.h>

/* Определяется в strings.c при запуске по GetUserDefaultUILanguage() */
extern int g_lang;   /* 0 = ru, 1 = en */

void strings_init(void);

#define LANG_RU 0
#define LANG_EN 1

/* Макрос: выбрать строку по текущему языку */
#define S(ru, en)  (g_lang == LANG_RU ? (ru) : (en))

/* ── Общее ────────────────────────────────────────────────────── */
#define STR_TITLE         S(L"Установка Naleystogramm",   L"Naleystogramm Setup")
#define STR_CANCEL        S(L"Отмена",                    L"Cancel")
#define STR_BACK          S(L"< Назад",                   L"< Back")
#define STR_NEXT          S(L"Далее >",                   L"Next >")
#define STR_FINISH        S(L"Завершить",                 L"Finish")
#define STR_INSTALL       S(L"Установить",                L"Install")
#define STR_CLOSE         S(L"Закрыть",                   L"Close")
#define STR_BROWSE        S(L"Обзор...",                  L"Browse...")
#define STR_YES           S(L"Да",                        L"Yes")
#define STR_NO            S(L"Нет",                       L"No")

/* ── Страница 1: Добро пожаловать ─────────────────────────────── */
#define STR_P1_TITLE      S(L"Добро пожаловать",          L"Welcome")
#define STR_P1_SUB        S(L"Выберите режим установки",  L"Choose installation mode")
#define STR_P1_BODY       S( \
    L"Этот мастер установит Naleystogramm " APP_VERSION L" на ваш компьютер.\r\n\r\n" \
    L"Выберите, откуда взять файлы приложения:", \
    L"This wizard will install Naleystogramm " APP_VERSION L" on your computer.\r\n\r\n" \
    L"Choose where to get the application files:")
#define STR_P1_BUNDLE     S(L"Установить эту версию (" APP_VERSION L")",  \
                             L"Install this version (" APP_VERSION L")")
#define STR_P1_BUNDLE_SUB S(L"Файлы уже встроены в этот установщик",     \
                             L"Files are already bundled in this installer")
#define STR_P1_DOWNLOAD   S(L"Загрузить последнюю версию с GitHub",       \
                             L"Download latest version from GitHub")
#define STR_P1_DL_SUB     S(L"Будет скачан актуальный релиз (нужен интернет)", \
                             L"Latest release will be downloaded (internet required)")

/* ── Страница 2: Папка установки ─────────────────────────────── */
#define STR_P2_TITLE      S(L"Папка установки",     L"Installation folder")
#define STR_P2_SUB        S(L"Куда установить?",    L"Where to install?")
#define STR_P2_LABEL      S(L"Установить в:",       L"Install to:")
#define STR_P2_SPACE      S(L"Требуется место: ~150 МБ", L"Required space: ~150 MB")

/* ── Страница 3: Параметры ───────────────────────────────────── */
#define STR_P3_TITLE      S(L"Параметры",                         L"Options")
#define STR_P3_SUB        S(L"Что создать при установке?",        L"What to create during installation?")
#define STR_P3_DESKTOP    S(L"Ярлык на рабочем столе",            L"Desktop shortcut")
#define STR_P3_START      S(L"Ярлык в меню Пуск",                 L"Start menu shortcut")
#define STR_P3_FIREWALL   S(L"Открыть порт 47821 в брандмауэре",  L"Open port 47821 in firewall")
#define STR_P3_AUTOSTART  S(L"Запускать при входе в систему",     L"Run at startup")

/* ── Страница 4: Прогресс ────────────────────────────────────── */
#define STR_P4_TITLE      S(L"Установка",                L"Installing")
#define STR_P4_SUB        S(L"Подождите, идёт установка...", L"Please wait...")
#define STR_P4_DONE_OK    S(L"Установка завершена!",     L"Installation complete!")
#define STR_P4_DONE_FAIL  S(L"Ошибка установки.",        L"Installation failed.")

/* ── Страница 5: Финиш ───────────────────────────────────────── */
#define STR_P5_TITLE      S(L"Готово!",                                L"Done!")
#define STR_P5_OK         S( \
    L"Naleystogramm " APP_VERSION L" успешно установлен.\r\n\r\n" \
    L"Программа добавлена в \"Установка и удаление программ\".", \
    L"Naleystogramm " APP_VERSION L" has been installed successfully.\r\n\r\n" \
    L"The program was added to \"Programs and Features\".")
#define STR_P5_FAIL       S( \
    L"Установка завершилась с ошибкой.\r\n\r\n" \
    L"Смотрите журнал для подробностей.", \
    L"Installation finished with errors.\r\n\r\n" \
    L"See the log for details.")
#define STR_P5_LAUNCH     S(L"Запустить Naleystogramm",    L"Launch Naleystogramm")

/* ── Диалог удаления ─────────────────────────────────────────── */
#define STR_UNINSTALL_TITLE  S(L"Удаление Naleystogramm",    L"Uninstall Naleystogramm")
#define STR_UNINSTALL_CONFIRM S(L"Удалить Naleystogramm и все его компоненты?", \
                                 L"Remove Naleystogramm and all its components?")
#define STR_UNINSTALL_DONE   S(L"Naleystogramm удалён.",     L"Naleystogramm has been removed.")
#define STR_UNINSTALL_ERR    S(L"Ошибка при удалении.",      L"Error during uninstall.")

/* ── Лог-сообщения (от рабочего потока) ─────────────────────── */
#define STR_LOG_EXTRACTING   S(L"Распаковка файлов...",          L"Extracting files...")
#define STR_LOG_DOWNLOADING  S(L"Получение информации о релизе...", L"Fetching release info...")
#define STR_LOG_DOWNLOADING2 S(L"Загрузка архива...",            L"Downloading archive...")
#define STR_LOG_SHORTCUTS    S(L"Создание ярлыков...",           L"Creating shortcuts...")
#define STR_LOG_FIREWALL     S(L"Настройка брандмауэра...",      L"Configuring firewall...")
#define STR_LOG_REGISTRY     S(L"Запись в реестр...",            L"Writing registry...")
#define STR_LOG_DONE         S(L"Готово.",                       L"Done.")
#define STR_LOG_ERROR        S(L"Ошибка: ",                      L"Error: ")
