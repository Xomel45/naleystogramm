cmake_minimum_required(VERSION 3.22)
project(naleystogramm VERSION 0.5.3 LANGUAGES CXX)

# RC (Windows Resource Compiler) нужен только при сборке под Windows.
# На Linux windres не установлен, поэтому включаем язык только там.
# CMake 3.22+ поддерживает enable_language() после project().
if(WIN32 OR MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # строго стандарт, без GNU-расширений
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS src/ui)

# ── Опция: статическая Windows сборка ─────────────────────────────────────────
# Включить: cmake -DWIN_STATIC=ON ...
# Требует статической сборки Qt6 для MinGW (mingw-w64-qt6-*-static из AUR).
# При OFF: libgcc/libstdc++/libpthread + OpenSSL — статические,
#          Qt6 — динамические (DLL нужны рядом с .exe).
option(WIN_STATIC "Полная статическая компоновка Windows .exe (требует static Qt6)" OFF)

# ── Windows: путь к статической Qt6 ──────────────────────────────────────────
# Если WIN_STATIC=ON — укажи где лежит статическая Qt6:
#   cmake -DWIN_STATIC=ON -DQT_STATIC_PREFIX=/путь/к/qt6-static ...
# По умолчанию пробуем стандартный AUR путь для mingw-w64-qt6-*-static
if(WIN32 AND WIN_STATIC)
    if(NOT QT_STATIC_PREFIX)
        # Стандартный путь AUR пакета mingw-w64-qt6-base-static
        set(QT_STATIC_PREFIX "/usr/x86_64-w64-mingw32/sys-root/mingw/static"
            CACHE PATH "Префикс установки статической Qt6 для MinGW")
    endif()
    # Добавляем в поиск ПЕРЕД стандартными путями (статика в приоритете)
    list(INSERT CMAKE_PREFIX_PATH 0 "${QT_STATIC_PREFIX}")
    # Предпочитаем .a над .dll.a при поиске библиотек
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a .lib .dll.a)
    message(STATUS "WIN_STATIC=ON  — Qt6 static prefix: ${QT_STATIC_PREFIX}")
else()
    message(STATUS "WIN_STATIC=OFF — Qt6 будет динамической (DLL потребуются)")
endif()

# ── Find Qt6 ──────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Sql
    Concurrent
    LinguistTools
    OPTIONAL_COMPONENTS
    Multimedia
)

# ── Find OpenSSL ──────────────────────────────────────────────────────────────
# Windows: форсируем статические .a архивы вместо импорт-библиотек .dll.a
# Проверено: /usr/x86_64-w64-mingw32/lib/libssl.a и libcrypto.a существуют.
# OPENSSL_USE_STATIC_LIBS заставляет FindOpenSSL.cmake искать .a (не .dll.a).
if(WIN32)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()
find_package(OpenSSL REQUIRED)

# ── SQLCipher (опционально: шифрование базы данных at-rest) ───────────────────
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SQLCIPHER QUIET sqlcipher)
    # Opus: отключаем при кросс-компиляции под Windows — pkg-config находит
    # системный Linux opus вместо MinGW sysroot, что ломает сборку.
    if(NOT WIN32)
        pkg_check_modules(OPUS QUIET opus)
    endif()
endif()

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp

    # Core
    src/core/identity.h
    src/core/identity.cpp
    src/core/network.h
    src/core/network.cpp
    src/core/storage.h
    src/core/storage.cpp
    src/core/filetransfer.h
    src/core/filetransfer.cpp
    src/core/audiorecorder.h
    src/core/audiorecorder.cpp
    src/core/upnp.h
    src/core/upnp.cpp
    src/core/systeminfo.h
    src/core/systeminfo.cpp

    # Crypto
    src/crypto/securedata.h
    src/crypto/openssl_raii.h
    src/crypto/keyprotector.h
    src/crypto/keyprotector.cpp
    src/crypto/e2e.h
    src/crypto/e2e.cpp
    src/crypto/x3dh.h
    src/crypto/x3dh.cpp
    src/crypto/ratchet.h
    src/crypto/ratchet.cpp

    # UI
    src/ui/mainwindow.h
    src/ui/mainwindow.cpp
    src/ui/mainwindow.ui
    src/ui/thememanager.h
    src/ui/thememanager.cpp
    src/ui/chatwidget.h
    src/ui/chatwidget.cpp
    src/ui/contactswidget.h
    src/ui/contactswidget.cpp
    src/core/updatechecker.h
    src/core/updatechecker.cpp
    src/core/demomode.h
    src/core/demomode.cpp
    src/core/sessionmanager.h
    src/core/sessionmanager.cpp
    src/core/logger.h
    src/core/logger.cpp
    src/ui/updatebanner.h
    src/ui/updatebanner.cpp
    src/ui/settingspanel.h
    src/ui/settingspanel.cpp
    src/ui/logpanel.h
    src/ui/logpanel.cpp
    src/ui/dialogs/addcontactdialog.h
    src/ui/dialogs/addcontactdialog.cpp
    src/ui/dialogs/incomingdialog.h
    src/ui/dialogs/incomingdialog.cpp
    src/ui/dialogs/fileacceptdialog.h
    src/ui/dialogs/fileacceptdialog.cpp
    src/ui/dialogs/contactprofiledialog.h
    src/ui/dialogs/contactprofiledialog.cpp
    src/ui/filetransferwidget.h
    src/ui/filetransferwidget.cpp
    src/ui/splashscreen.h
    src/ui/splashscreen.cpp

    # Голосовые звонки
    src/core/callmanager.h
    src/core/callmanager.cpp
    src/media/mediaengine.h
    src/media/mediaengine.cpp
    src/ui/callwindow.h
    src/ui/callwindow.cpp

    # Удалённый шелл
    src/core/remoteshellmanager.h
    src/core/remoteshellmanager.cpp
    src/ui/shellwindow.h
    src/ui/shellwindow.cpp
    src/ui/shellmonitor.h
    src/ui/shellmonitor.cpp
)

# ── Windows: добавляем ресурс-файл (манифест UAC + иконка) ───────────────────
# windres скомпилирует .rc → .o и линкер встроит манифест в .exe.
# Это единственный способ надёжно встроить UAC-манифест без внешних файлов.
if(WIN32)
    list(APPEND SOURCES resources/windows/app.rc)

    # windres должен знать где искать app.manifest (рядом с app.rc)
    # CMAKE_RC_FLAGS применяется к ВСЕМ .rc файлам в проекте
    list(APPEND CMAKE_RC_FLAGS "-I \"${CMAKE_SOURCE_DIR}/resources/windows\"")
endif()

# ── Translations (i18n) ───────────────────────────────────────────────────────
set(TS_FILES
    translations/naleystogramm_ru.ts
    translations/naleystogramm_en.ts
)

qt_add_translation(QM_FILES ${TS_FILES})

set(TRANSLATIONS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/translations")
file(MAKE_DIRECTORY ${TRANSLATIONS_OUTPUT_DIR})

add_custom_target(translations ALL DEPENDS ${QM_FILES}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${QM_FILES}
        ${TRANSLATIONS_OUTPUT_DIR}
    COMMENT "Копирование файлов переводов в ${TRANSLATIONS_OUTPUT_DIR}"
)

# ── Executable ────────────────────────────────────────────────────────────────
# Windows: НЕ используем ключевое слово WIN32 — это CONSOLE подсистема.
# Преимущество: видны debug-логи в терминале + нормальное Qt GUI окно.
# Когда захочется убрать консольное окно — добавить WIN32 обратно.
#
# Точка входа: стандартный int main(int argc, char *argv[])
# Никаких WinMain / wWinMain не нужно — Qt сам заботится об этом.
add_executable(naleystogramm ${SOURCES})

add_dependencies(naleystogramm translations)

target_compile_definitions(naleystogramm PRIVATE
    TRANSLATIONS_DIR="${TRANSLATIONS_OUTPUT_DIR}"
)

target_include_directories(naleystogramm PRIVATE
    src
    src/core
    src/crypto
    src/ui
    src/media
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(naleystogramm PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Qt6Multimedia опционален — может отсутствовать в MinGW cross-compile окружении
if(Qt6Multimedia_FOUND)
    target_link_libraries(naleystogramm PRIVATE Qt6::Multimedia)
    target_compile_definitions(naleystogramm PRIVATE HAVE_QT_MULTIMEDIA)
    message(STATUS "Qt6Multimedia:   найден — голосовые сообщения включены")
else()
    message(STATUS "Qt6Multimedia:   не найден — голосовые сообщения отключены")
endif()

# ── libopus: голосовые звонки ─────────────────────────────────────────────────
if(OPUS_FOUND)
    message(STATUS "libopus:         найден ${OPUS_VERSION} — голосовые звонки включены")
    target_compile_definitions(naleystogramm PRIVATE HAVE_OPUS)
    target_include_directories(naleystogramm PRIVATE ${OPUS_INCLUDE_DIRS})
    target_link_libraries(naleystogramm PRIVATE ${OPUS_LIBRARIES})
else()
    message(STATUS "libopus:         не найден — голосовые звонки отключены")
    message(STATUS "                 Установить: sudo pacman -S opus  или  sudo apt install libopus-dev")
endif()

# ── SQLCipher: включаем если найден ───────────────────────────────────────────
if(SQLCIPHER_FOUND)
    message(STATUS "SQLCipher:       найден (${SQLCIPHER_VERSION}) — шифрование БД включено")
    target_compile_definitions(naleystogramm PRIVATE HAVE_SQLCIPHER)
    target_include_directories(naleystogramm PRIVATE ${SQLCIPHER_INCLUDE_DIRS})
    target_link_libraries(naleystogramm PRIVATE ${SQLCIPHER_LINK_LIBRARIES})
else()
    message(STATUS "SQLCipher:       не найден — шифрование БД отключено")
    message(STATUS "                 Установить: sudo apt install libsqlcipher-dev")
endif()

# ══════════════════════════════════════════════════════════════════════════════
# ██  WINDOWS-СПЕЦИФИЧНЫЙ БЛОК  ██
# ══════════════════════════════════════════════════════════════════════════════
if(WIN32)

    # ── Убираем консольное окно (GUI-приложение) ──────────────────────────────
    # WIN32_EXECUTABLE=ON → -mwindows (MinGW) / /SUBSYSTEM:WINDOWS (MSVC).
    # Qt6::Core подтягивает WinMain→main шим автоматически через Qt6::EntryPoint.
    # На Linux/macOS свойство игнорируется.
    set_target_properties(naleystogramm PROPERTIES WIN32_EXECUTABLE ON)

    # ── Макросы: отключаем мусор из windows.h ─────────────────────────────────
    # NOMINMAX         — запрет макросов min/max (конфликт с std::min/max)
    # WIN32_LEAN_AND_MEAN — исключаем редкие заголовки Windows (ускоряет компиляцию)
    # UNICODE + _UNICODE — Unicode-сборка (Qt требует Unicode)
    # _WIN32_WINNT=0x0A00  — целевая платформа: Windows 10+ API
    # WINVER=0x0A00    — то же, для заголовков Windows
    target_compile_definitions(naleystogramm PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
    )

    # ── Системные библиотеки Windows ──────────────────────────────────────────
    # Перечислены ЯВНО чтобы статически линкуемые OpenSSL/Qt нашли все символы.
    # При динамической Qt6 многие из них уже тянут Qt DLLs, но явный список
    # страхует от "undefined reference" при статической сборке.
    target_link_libraries(naleystogramm PRIVATE
        ws2_32      # Windows Sockets 2: TCP/IP, UDP — нужен NetworkManager
        iphlpapi    # IP Helper API: интерфейсы, маршруты, UPnP
        crypt32     # CryptoAPI: хранилище сертификатов, CNG
        bcrypt      # BCryptGenRandom: CSPRNG для OpenSSL на Windows
        ncrypt      # NCrypt: криптографический провайдер (ключи TPM)
        secur32     # SSPI: SSL/TLS провайдер
        userenv     # GetUserProfileDirectory, переменные окружения пользователя
        version     # GetFileVersionInfo: версии PE-файлов
        dwmapi      # Desktop Window Manager: размытие, тёмная тема
        uxtheme     # Visual Styles: ComCtl32 v6 (нужен для кнопок Win10 стиля)
        winmm       # Multimedia: таймеры высокого разрешения
        shell32     # SHGetKnownFolderPath: пути AppData/Cache/Documents
        ole32       # COM runtime: нужен Qt (QClipboard, drag&drop)
        oleaut32    # OLE Automation: SafeArrays, BSTR (Qt использует косвенно)
        uuid        # RPCRT4 UUID: CoCreateGuid — генерация UUID
        gdi32       # GDI: рендеринг шрифтов, растровая графика
        user32      # Оконная подсистема: CreateWindow, сообщения
        advapi32    # Реестр, ACL, токены безопасности, сервисы
        kernel32    # Базовые функции ОС: файлы, потоки, память
        comdlg32    # Стандартные диалоги: файлы, шрифты, цвета
        winspool    # Print Spooler: нужен Qt Widgets (QPrinter API)
        imm32       # Input Method Manager: ввод CJK, нужен Qt
        netapi32    # Network Management API: информация о сети
        setupapi    # Setup API: устройства, нужен UPnP
        dnsapi      # DNS API: разрешение имён
        mswsock     # Winsock расширения: AcceptEx, TransmitFile
        wintrust    # WinTrust: верификация подписи PE
    )

    # ── Статическая компоновка MinGW рантайма ─────────────────────────────────
    #
    # ВАЖНО: все флаги передаются в ОДНОМ вызове target_link_options.
    # Два отдельных вызова создавали сломанный порядок флагов:
    #   ... -Wl,-Bdynamic ... (первый вызов) ... -static (второй вызов)
    # GNU ld обрабатывает -Bstatic/-Bdynamic позиционно — -Bdynamic
    # в середине команды re-включал динамический поиск ДО того как -static
    # успевал его отключить. Результат: libstdc++-6.dll или libwinpthread-1.dll
    # оставались динамическими даже при наличии флагов статической линковки.
    #
    if(MINGW)
        if(WIN_STATIC)
            # ── Режим WIN_STATIC=ON: единый автономный .exe ───────────────────────
            # -static         : линкуем ВСЁ статически — libgcc, libstdc++,
            #                   libwinpthread, Qt (статическая), OpenSSL.
            #                   Qt DLL-ы БОЛЬШЕ НЕ НУЖНЫ рядом с .exe.
            # -static-libgcc  : явная страховка — libgcc.a встроена в .exe.
            # -static-libstdc++: явная страховка — libstdc++.a встроена в .exe.
            #
            # Без --whole-archive и без -Bdynamic — нет конфликтов позиционных флагов.
            target_link_options(naleystogramm PRIVATE
                -static
                -static-libgcc
                -static-libstdc++
            )
            message(STATUS "WIN_STATIC: полная статическая линковка (-static -static-libgcc -static-libstdc++)")
        else()
            # ── Режим по умолчанию: динамическая Qt, статический рантайм ──────────
            # Qt DLL-ы нужны рядом с .exe, но MinGW рантайм встроен в .exe.
            #
            # ВАЖНО: -Wl,-Bstatic,-lpthread,-Bdynamic НЕ используется, т.к. OpenSSL
            # (libcrypto.a) содержит threads_pthread.obj, который резолвит pthread-символы
            # ПОСЛЕ того как -Bdynamic восстановлен → итог: динамический импорт libwinpthread.
            # Решение: линковать libwinpthread.a и libssp.a по полному пути — они всегда
            # трактуются как статические архивы вне зависимости от позиции -Bstatic/-Bdynamic.
            target_link_options(naleystogramm PRIVATE
                -static-libgcc
                -static-libstdc++
            )

            find_library(WINPTHREAD_STATIC NAMES libwinpthread.a
                PATHS ${CMAKE_FIND_ROOT_PATH} PATH_SUFFIXES lib NO_DEFAULT_PATH)
            find_library(LIBSSP_STATIC NAMES libssp.a
                PATHS ${CMAKE_FIND_ROOT_PATH} PATH_SUFFIXES lib NO_DEFAULT_PATH)
            if(WINPTHREAD_STATIC)
                target_link_libraries(naleystogramm PRIVATE ${WINPTHREAD_STATIC})
                message(STATUS "libwinpthread:   статически встроен (${WINPTHREAD_STATIC})")
            else()
                message(WARNING "libwinpthread.a не найден — libwinpthread-1.dll будет нужна рядом с .exe")
            endif()
            if(LIBSSP_STATIC)
                target_link_libraries(naleystogramm PRIVATE ${LIBSSP_STATIC})
                message(STATUS "libssp:          статически встроен (${LIBSSP_STATIC})")
            else()
                message(STATUS "libssp.a не найден — будет использована динамическая версия")
            endif()
        endif()
    endif()

    # ── MSVC: статический CRT (/MT) ───────────────────────────────────────────
    # /MT  = Release: msvcrt встроен в .exe (нет зависимости от vcruntime*.dll)
    # /MTd = Debug:   аналогично для отладочной сборки
    if(MSVC)
        set_property(TARGET naleystogramm PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

        # Встраиваем манифест через линкер MSVC
        # /MANIFEST:EMBED — манифест прямо в PE, не как внешний .manifest файл
        # /MANIFESTINPUT — наш app.manifest с requireAdministrator
        target_link_options(naleystogramm PRIVATE
            "/MANIFEST:EMBED"
            "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/resources/windows/app.manifest"
        )
        # /SUBSYSTEM:WINDOWS — задаётся через WIN32_EXECUTABLE выше (без консоли)
    endif()

    # WIN_STATIC логика перенесена в блок if(MINGW) выше —
    # все флаги в одном target_link_options для корректного порядка.

    # ── Qt6 плагины: встраиваем при статической Qt ────────────────────────────
    # При динамической Qt6 плагины — это .dll файлы в папках platforms/, sqldrivers/
    # При статической Qt6 плагины линкуются прямо в .exe через qt_import_plugins()
    get_target_property(_qt6_core_type Qt6::Core TYPE)
    if(_qt6_core_type STREQUAL "STATIC_LIBRARY")
        message(STATUS "Qt6: статическая сборка — встраиваем плагины в .exe")
        qt_import_plugins(naleystogramm
            INCLUDE
                Qt6::QWindowsIntegrationPlugin   # платформа Windows (qwindows) — КРИТИЧНО
                Qt6::QWindowsVistaStylePlugin    # стили Windows Vista/10/11
                Qt6::QSQLiteDriverPlugin         # SQLite драйвер для Qt SQL
                Qt6::QICOPlugin                  # ICO формат (иконки)
                Qt6::QJpegPlugin                 # JPEG (аватарки)
                Qt6::QGifPlugin                  # GIF
                Qt6::QGenericEnginePlugin        # шрифтовой движок (fallback)
        )
    else()
        message(STATUS "Qt6: динамическая сборка — нужны platforms/qwindows.dll и др.")
        message(STATUS "     Скопировать через windeployqt: см. build.sh")
    endif()

    # ── Финальный отчёт ───────────────────────────────────────────────────────
    message(STATUS "")
    message(STATUS "╔════════════════════════════════════════╗")
    message(STATUS "║       Windows сборка настроена         ║")
    message(STATUS "╠════════════════════════════════════════╣")
    message(STATUS "║  Подсистема: CONSOLE (debug логи видны)║")
    message(STATUS "║  UAC:        requireAdministrator       ║")
    message(STATUS "║  Манифест:   встроен в .exe (windres)  ║")
    if(WIN_STATIC AND MINGW)
        message(STATUS "║  Линковка:   ПОЛНАЯ СТАТИКА (-static)  ║")
    else()
        message(STATUS "║  Линковка:   libgcc/stdc++/pthread/ssl  ║")
        message(STATUS "║              статические, Qt6 — DLL     ║")
    endif()
    message(STATUS "╚════════════════════════════════════════╝")
    message(STATUS "")

endif() # WIN32

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(naleystogramm PRIVATE /W4)
else()
    target_compile_options(naleystogramm PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Build info ────────────────────────────────────────────────────────────────
message(STATUS "Qt version:      ${Qt6_VERSION}")
message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
message(STATUS "Compiler:        ${CMAKE_CXX_COMPILER}")
message(STATUS "Target system:   ${CMAKE_SYSTEM_NAME}")

# ── AppImage packaging (Linux only) ───────────────────────────────────────────
if(UNIX AND NOT APPLE)
    set(APPIMAGE_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/make_appimage.sh")
    set(DESKTOP_FILE "${CMAKE_SOURCE_DIR}/naleystogramm.desktop")
    set(ICON_FILE "${CMAKE_SOURCE_DIR}/resources/icons/app_icon.png")

    add_custom_target(appimage
        COMMAND ${CMAKE_COMMAND} -E echo "Creating AppImage..."
        COMMAND bash ${APPIMAGE_SCRIPT} ${CMAKE_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS naleystogramm translations
        COMMENT "Сборка портативного AppImage пакета"
        VERBATIM
    )

    add_custom_target(appimage-verbose
        COMMAND ${CMAKE_COMMAND} -E echo "Creating AppImage (verbose)..."
        COMMAND bash -x ${APPIMAGE_SCRIPT} ${CMAKE_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS naleystogramm translations
        COMMENT "Сборка AppImage с подробным выводом"
        VERBATIM
    )

    add_custom_target(appdir
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/AppDir/usr/bin
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/AppDir/usr/share/applications
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/AppDir/usr/share/icons/hicolor/256x256/apps
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/AppDir/usr/share/translations
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:naleystogramm> ${CMAKE_BINARY_DIR}/AppDir/usr/bin/
        COMMAND ${CMAKE_COMMAND} -E copy ${DESKTOP_FILE} ${CMAKE_BINARY_DIR}/AppDir/
        COMMAND ${CMAKE_COMMAND} -E copy ${DESKTOP_FILE} ${CMAKE_BINARY_DIR}/AppDir/usr/share/applications/
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ICON_FILE} ${CMAKE_BINARY_DIR}/AppDir/naleystogramm.png
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ICON_FILE} ${CMAKE_BINARY_DIR}/AppDir/usr/share/icons/hicolor/256x256/apps/naleystogramm.png
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${TRANSLATIONS_OUTPUT_DIR} ${CMAKE_BINARY_DIR}/AppDir/usr/share/translations
        DEPENDS naleystogramm translations
        COMMENT "Подготовка структуры AppDir"
        VERBATIM
    )

    message(STATUS "")
    message(STATUS "Цели AppImage:")
    message(STATUS "  make appimage         - Собрать портативный AppImage")
    message(STATUS "  make appimage-verbose - Собрать с подробным выводом")
    message(STATUS "  make appdir           - Подготовить AppDir без упаковки")
    message(STATUS "")
endif()

# ══════════════════════════════════════════════════════════════════════════════
# ██  DEPLOY TARGETS  ██
# make beta            — сырой ELF → builds/beta/               (только Linux)
# make release-linux   — AppImage → builds/releases/VERSION-linux/ (только Linux)
# make release-windows — .exe+DLL → builds/releases/VERSION-windows/ (Windows/cross)
# ══════════════════════════════════════════════════════════════════════════════
set(DEPLOY_SCRIPT "${CMAKE_SOURCE_DIR}/deploy.sh")
set(BUILDS_DIR    "${CMAKE_SOURCE_DIR}/builds")
set(APP_VERSION   "${PROJECT_VERSION}")

# ── Beta: Linux ELF для быстрой проверки ────────────────────────────────────
# Запуск: cmake --build build-linux --target beta
# Не упаковывает в AppImage — просто копирует бинарник для тестирования на машине.
if(UNIX AND NOT APPLE)
    add_custom_target(beta
        # Создаём директорию
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILDS_DIR}/beta"
        # Копируем ELF
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:naleystogramm>
            "${BUILDS_DIR}/beta/naleystogramm"
        # Копируем переводы рядом
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${TRANSLATIONS_OUTPUT_DIR}"
            "${BUILDS_DIR}/beta/translations"
        # Пишем build-info.txt с версией и датой
        COMMAND bash -c
            "printf 'version=${APP_VERSION}\\nbuilt=%s\\ncommit=%s\\nplatform=linux-beta\\n' \
             \"$(date '+%Y-%m-%d %H:%M:%S')\" \
             \"$(git -C '${CMAKE_SOURCE_DIR}' rev-parse --short HEAD 2>/dev/null || echo unknown)\" \
             > '${BUILDS_DIR}/beta/build-info.txt'"
        # Отчёт
        COMMAND ${CMAKE_COMMAND} -E echo
            "✓ Beta готов: ${BUILDS_DIR}/beta/naleystogramm"
        DEPENDS naleystogramm translations
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Деплой beta → builds/beta/"
        VERBATIM
    )

    # ── Release Linux: AppImage ──────────────────────────────────────────────
    # Запуск: cmake --build build-linux --target release-linux
    # Делегирует deploy.sh который вызывает make_appimage.sh.
    # Выход: builds/releases/VERSION-linux/Naleystogramm-VERSION-x86_64.AppImage
    add_custom_target(release-linux
        COMMAND bash "${DEPLOY_SCRIPT}" release linux
        DEPENDS naleystogramm translations
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Деплой release Linux AppImage → builds/releases/${APP_VERSION}-linux/"
        VERBATIM
    )
endif()

# ── Release Windows: .exe + Qt DLL ──────────────────────────────────────────
# Запуск: cmake --build build-win --target release-windows
# Делегирует deploy.sh который копирует .exe, Qt DLL и плагины.
# Выход: builds/releases/VERSION-windows/
# OpenSSL и MinGW runtime встроены статически — DLL не нужны.
if(WIN32)
    add_custom_target(release-windows
        COMMAND bash "${DEPLOY_SCRIPT}" release win
        DEPENDS naleystogramm translations
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Деплой release Windows → builds/releases/${APP_VERSION}-windows/"
        VERBATIM
    )
endif()

# ── Справка о deploy-целях ────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "Deploy targets:")
if(UNIX AND NOT APPLE)
    message(STATUS "  make beta             → builds/beta/naleystogramm (сырой ELF)")
    message(STATUS "  make release-linux    → builds/releases/${APP_VERSION}-linux/ (AppImage)")
endif()
if(WIN32)
    message(STATUS "  make release-windows  → builds/releases/${APP_VERSION}-windows/ (.exe+DLL)")
endif()
message(STATUS "  ./deploy.sh --help    → полная документация скрипта")
message(STATUS "")
