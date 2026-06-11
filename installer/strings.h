#pragma once
#include <windows.h>

/* Определяется в strings.c при запуске по GetUserDefaultUILanguage() */
extern int g_lang;   /* 0=ru, 1=en, 2=de, 3=uk, 4=be */

void strings_init(void);

#define LANG_RU 0
#define LANG_EN 1
#define LANG_DE 2
#define LANG_UK 3
#define LANG_BE 4

/* Макрос: выбрать строку по текущему языку */
#define S(ru, en, de, uk, be) \
    (g_lang == LANG_RU ? (ru) : \
     g_lang == LANG_EN ? (en) : \
     g_lang == LANG_DE ? (de) : \
     g_lang == LANG_UK ? (uk) : (be))

/* ── Общее ────────────────────────────────────────────────────── */
#define STR_TITLE         S(L"Установка Naleystogramm",   L"Naleystogramm Setup", \
                             L"Naleystogramm-Installation", L"Встановлення Naleystogramm", \
                             L"Усталяванне Naleystogramm")
#define STR_CANCEL        S(L"Отмена",   L"Cancel",   L"Abbrechen",  L"Скасувати", L"Скасаваць")
#define STR_BACK          S(L"< Назад",  L"< Back",   L"< Zurück",   L"< Назад",   L"< Назад")
#define STR_NEXT          S(L"Далее >",  L"Next >",   L"Weiter >",   L"Далі >",    L"Далей >")
#define STR_FINISH        S(L"Завершить", L"Finish",  L"Fertigstellen", L"Завершити", L"Завяршыць")
#define STR_INSTALL       S(L"Установить", L"Install", L"Installieren", L"Встановити", L"Усталяваць")
#define STR_CLOSE         S(L"Закрыть",  L"Close",    L"Schließen",  L"Закрити",   L"Зачыніць")
#define STR_BROWSE        S(L"Обзор...", L"Browse...", L"Durchsuchen...", L"Огляд...", L"Агляд...")
#define STR_YES           S(L"Да",       L"Yes",      L"Ja",         L"Так",       L"Так")
#define STR_NO            S(L"Нет",      L"No",       L"Nein",       L"Ні",        L"Не")

/* ── Страница 1: Добро пожаловать ─────────────────────────────── */
#define STR_P1_TITLE      S(L"Добро пожаловать", L"Welcome", L"Willkommen", \
                             L"Ласкаво просимо",  L"Сардэчна запрашаем")
#define STR_P1_SUB        S(L"Выберите режим установки", L"Choose installation mode", \
                             L"Wählen Sie den Installationsmodus", \
                             L"Виберіть режим встановлення", L"Выберыце рэжым усталявання")
#define STR_P1_BODY       S( \
    L"Этот мастер установит Naleystogramm " APP_VERSION L" на ваш компьютер.\r\n\r\n" \
    L"Выберите, откуда взять файлы приложения:", \
    L"This wizard will install Naleystogramm " APP_VERSION L" on your computer.\r\n\r\n" \
    L"Choose where to get the application files:", \
    L"Dieser Assistent installiert Naleystogramm " APP_VERSION L" auf Ihrem Computer.\r\n\r\n" \
    L"Wählen Sie, woher die Anwendungsdateien stammen sollen:", \
    L"Цей майстер встановить Naleystogramm " APP_VERSION L" на ваш комп'ютер.\r\n\r\n" \
    L"Виберіть, звідки взяти файли програми:", \
    L"Гэты майстар усталюе Naleystogramm " APP_VERSION L" на ваш камп'ютар.\r\n\r\n" \
    L"Выберыце, адкуль узяць файлы праграмы:")
#define STR_P1_BUNDLE     S(L"Установить эту версию (" APP_VERSION L")", \
                             L"Install this version (" APP_VERSION L")", \
                             L"Diese Version installieren (" APP_VERSION L")", \
                             L"Встановити цю версію (" APP_VERSION L")", \
                             L"Усталяваць гэтую версію (" APP_VERSION L")")
#define STR_P1_BUNDLE_SUB S(L"Файлы уже встроены в этот установщик", \
                             L"Files are already bundled in this installer", \
                             L"Die Dateien sind bereits in diesem Installationsprogramm enthalten", \
                             L"Файли вже вбудовані в цей інсталятор", \
                             L"Файлы ўжо ўбудаваны ў гэты ўсталёўшчык")
#define STR_P1_DOWNLOAD   S(L"Загрузить последнюю версию с GitHub", \
                             L"Download latest version from GitHub", \
                             L"Neueste Version von GitHub herunterladen", \
                             L"Завантажити останню версію з GitHub", \
                             L"Спампаваць апошнюю версію з GitHub")
#define STR_P1_DL_SUB     S(L"Будет скачан актуальный релиз (нужен интернет)", \
                             L"Latest release will be downloaded (internet required)", \
                             L"Die aktuelle Version wird heruntergeladen (Internetverbindung erforderlich)", \
                             L"Буде завантажено актуальний реліз (потрібен інтернет)", \
                             L"Будзе спампаваны актуальны рэліз (патрэбны інтэрнэт)")

/* ── Страница 2: Папка установки ─────────────────────────────── */
#define STR_P2_TITLE      S(L"Папка установки", L"Installation folder", L"Installationsordner", \
                             L"Папка встановлення", L"Папка ўсталявання")
#define STR_P2_SUB        S(L"Куда установить?", L"Where to install?", L"Wohin installieren?", \
                             L"Куди встановити?", L"Куды ўсталяваць?")
#define STR_P2_LABEL      S(L"Установить в:", L"Install to:", L"Installieren in:", \
                             L"Встановити в:", L"Усталяваць у:")
#define STR_P2_SPACE      S(L"Требуется место: ~150 МБ", L"Required space: ~150 MB", \
                             L"Benötigter Speicherplatz: ~150 MB", \
                             L"Потрібно місця: ~150 МБ", L"Патрэбна месца: ~150 МБ")

/* ── Страница 3: Параметры ───────────────────────────────────── */
#define STR_P3_TITLE      S(L"Параметры", L"Options", L"Optionen", L"Параметри", L"Параметры")
#define STR_P3_SUB        S(L"Что создать при установке?", L"What to create during installation?", \
                             L"Was soll bei der Installation erstellt werden?", \
                             L"Що створити під час встановлення?", L"Што стварыць пры ўсталяванні?")
#define STR_P3_DESKTOP    S(L"Ярлык на рабочем столе", L"Desktop shortcut", \
                             L"Verknüpfung auf dem Desktop", \
                             L"Ярлик на робочому столі", L"Ярлык на працоўным стале")
#define STR_P3_START      S(L"Ярлык в меню Пуск", L"Start menu shortcut", \
                             L"Verknüpfung im Startmenü", \
                             L"Ярлик у меню Пуск", L"Ярлык у меню Пуск")
#define STR_P3_FIREWALL   S(L"Открыть порт 47821 в брандмауэре", L"Open port 47821 in firewall", \
                             L"Port 47821 in der Firewall öffnen", \
                             L"Відкрити порт 47821 у брандмауері", L"Адкрыць порт 47821 у брандмаўэры")
#define STR_P3_AUTOSTART  S(L"Запускать при входе в систему", L"Run at startup", \
                             L"Beim Systemstart ausführen", \
                             L"Запускати під час входу в систему", L"Запускаць пры ўваходзе ў сістэму")

/* ── Страница 4: Прогресс ────────────────────────────────────── */
#define STR_P4_TITLE      S(L"Установка", L"Installing", L"Installation", L"Встановлення", L"Усталяванне")
#define STR_P4_SUB        S(L"Подождите, идёт установка...", L"Please wait...", \
                             L"Bitte warten, Installation läuft...", \
                             L"Зачекайте, триває встановлення...", L"Пачакайце, ідзе ўсталяванне...")
#define STR_P4_DONE_OK    S(L"Установка завершена!", L"Installation complete!", \
                             L"Installation abgeschlossen!", \
                             L"Встановлення завершено!", L"Усталяванне завершана!")
#define STR_P4_DONE_FAIL  S(L"Ошибка установки.", L"Installation failed.", \
                             L"Installation fehlgeschlagen.", \
                             L"Помилка встановлення.", L"Памылка ўсталявання.")

/* ── Страница 5: Финиш ───────────────────────────────────────── */
#define STR_P5_TITLE      S(L"Готово!", L"Done!", L"Fertig!", L"Готово!", L"Гатова!")
#define STR_P5_OK         S( \
    L"Naleystogramm " APP_VERSION L" успешно установлен.\r\n\r\n" \
    L"Программа добавлена в \"Установка и удаление программ\".", \
    L"Naleystogramm " APP_VERSION L" has been installed successfully.\r\n\r\n" \
    L"The program was added to \"Programs and Features\".", \
    L"Naleystogramm " APP_VERSION L" wurde erfolgreich installiert.\r\n\r\n" \
    L"Das Programm wurde zu \"Programme und Features\" hinzugefügt.", \
    L"Naleystogramm " APP_VERSION L" успішно встановлено.\r\n\r\n" \
    L"Програму додано до \"Програми та компоненти\".", \
    L"Naleystogramm " APP_VERSION L" паспяхова ўсталяваны.\r\n\r\n" \
    L"Праграма дададзена ў \"Праграмы і кампаненты\".")
#define STR_P5_FAIL       S( \
    L"Установка завершилась с ошибкой.\r\n\r\n" \
    L"Смотрите журнал для подробностей.", \
    L"Installation finished with errors.\r\n\r\n" \
    L"See the log for details.", \
    L"Die Installation wurde mit Fehlern beendet.\r\n\r\n" \
    L"Details finden Sie im Protokoll.", \
    L"Встановлення завершилося з помилками.\r\n\r\n" \
    L"Див. журнал для подробиць.", \
    L"Усталяванне завяршылася з памылкамі.\r\n\r\n" \
    L"Гл. журнал для падрабязнасцяў.")
#define STR_P5_LAUNCH     S(L"Запустить Naleystogramm", L"Launch Naleystogramm", \
                             L"Naleystogramm starten", \
                             L"Запустити Naleystogramm", L"Запусціць Naleystogramm")

/* ── Диалог удаления ─────────────────────────────────────────── */
#define STR_UNINSTALL_NOTFOUND S(L"Naleystogramm не найден в реестре.", \
                                  L"Naleystogramm not found in registry.", \
                                  L"Naleystogramm wurde nicht in der Registrierung gefunden.", \
                                  L"Naleystogramm не знайдено в реєстрі.", \
                                  L"Naleystogramm не знойдзены ў рэестры.")
#define STR_UNINSTALL_TITLE  S(L"Удаление Naleystogramm", L"Uninstall Naleystogramm", \
                                L"Naleystogramm deinstallieren", \
                                L"Видалення Naleystogramm", L"Выдаленне Naleystogramm")
#define STR_UNINSTALL_CONFIRM S(L"Удалить Naleystogramm и все его компоненты?", \
                                 L"Remove Naleystogramm and all its components?", \
                                 L"Naleystogramm und alle Komponenten entfernen?", \
                                 L"Видалити Naleystogramm і всі його компоненти?", \
                                 L"Выдаліць Naleystogramm і ўсе яго кампаненты?")
#define STR_UNINSTALL_DONE   S(L"Naleystogramm удалён.", L"Naleystogramm has been removed.", \
                                L"Naleystogramm wurde entfernt.", \
                                L"Naleystogramm видалено.", L"Naleystogramm выдалены.")
#define STR_UNINSTALL_ERR    S(L"Ошибка при удалении.", L"Error during uninstall.", \
                                L"Fehler bei der Deinstallation.", \
                                L"Помилка під час видалення.", L"Памылка пры выдаленні.")

/* ── Лог-сообщения (от рабочего потока) ─────────────────────── */
#define STR_LOG_EXTRACTING   S(L"Распаковка файлов...", L"Extracting files...", \
                                L"Dateien werden entpackt...", \
                                L"Розпакування файлів...", L"Распакоўка файлаў...")
#define STR_LOG_DOWNLOADING  S(L"Получение информации о релизе...", L"Fetching release info...", \
                                L"Release-Informationen werden abgerufen...", \
                                L"Отримання інформації про реліз...", L"Атрыманне інфармацыі аб рэлізе...")
#define STR_LOG_DOWNLOADING2 S(L"Загрузка архива...", L"Downloading archive...", \
                                L"Archiv wird heruntergeladen...", \
                                L"Завантаження архіву...", L"Спампоўка архіва...")
#define STR_LOG_SHORTCUTS    S(L"Создание ярлыков...", L"Creating shortcuts...", \
                                L"Verknüpfungen werden erstellt...", \
                                L"Створення ярликів...", L"Стварэнне ярлыкоў...")
#define STR_LOG_FIREWALL     S(L"Настройка брандмауэра...", L"Configuring firewall...", \
                                L"Firewall wird konfiguriert...", \
                                L"Налаштування брандмауера...", L"Наладка брандмаўэра...")
#define STR_LOG_REGISTRY     S(L"Запись в реестр...", L"Writing registry...", \
                                L"Registrierung wird geschrieben...", \
                                L"Запис у реєстр...", L"Запіс у рэестр...")
#define STR_LOG_DONE         S(L"Готово.", L"Done.", L"Fertig.", L"Готово.", L"Гатова.")
#define STR_LOG_ERROR        S(L"Ошибка: ", L"Error: ", L"Fehler: ", L"Помилка: ", L"Памылка: ")
