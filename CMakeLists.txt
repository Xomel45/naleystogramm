cmake_minimum_required(VERSION 3.22)
project(naleystogramm VERSION 0.8.1 LANGUAGES CXX)

if(WIN32 OR MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS src/ui)

# ── Опция: статическая Windows сборка ─────────────────────────────────────────
option(WIN_STATIC "Полная статическая компоновка Windows .exe (требует static Qt6)" OFF)

if(WIN32 AND WIN_STATIC)
    if(NOT QT_STATIC_PREFIX)
        set(QT_STATIC_PREFIX "/usr/x86_64-w64-mingw32/sys-root/mingw/static"
            CACHE PATH "Префикс установки статической Qt6 для MinGW")
    endif()
    list(INSERT CMAKE_PREFIX_PATH 0 "${QT_STATIC_PREFIX}")
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
    WebSockets
    Multimedia
)

# ── Find OpenSSL ──────────────────────────────────────────────────────────────
if(WIN32)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()
find_package(OpenSSL REQUIRED)

# ── SQLCipher / Opus / QREncode ───────────────────────────────────────────────
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    if(NOT WIN32)
        pkg_check_modules(SQLCIPHER QUIET sqlcipher)
        pkg_check_modules(OPUS QUIET opus)
        pkg_check_modules(QRENCODE QUIET libqrencode)
    endif()
endif()

# ── Standalone Asio + nlohmann/json (header-only, для миграции Qt → std) ──────
# Используется начиная с Фазы 1. При наличии системных пакетов (pacman -S asio
# nlohmann-json) FetchContent пропустит клонирование.
include(FetchContent)

find_path(ASIO_SYSTEM_INCLUDE asio.hpp
    PATHS /usr/include /usr/local/include
    NO_DEFAULT_PATH)
if(ASIO_SYSTEM_INCLUDE)
    message(STATUS "asio:            системный (${ASIO_SYSTEM_INCLUDE})")
    set(ASIO_INCLUDE_DIR ${ASIO_SYSTEM_INCLUDE})
else()
    FetchContent_Declare(asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG        asio-1-30-2
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(asio)
    set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include)
    message(STATUS "asio:            FetchContent (${ASIO_INCLUDE_DIR})")
endif()

find_package(nlohmann_json QUIET CONFIG)
if(nlohmann_json_FOUND)
    message(STATUS "nlohmann/json:   системный (${nlohmann_json_VERSION})")
else()
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
    message(STATUS "nlohmann/json:   FetchContent v3.11.3")
endif()

# ══════════════════════════════════════════════════════════════════════════════
# ПРАВИЛО: core и crypto НЕ включают Qt напрямую.
#   ✅ Все Qt↔std конвертации — через src/crypto/qt_bridge.h (только из UI).
#   🔍 Проверить нарушения:  cmake --build . --target core-purity
#   🔒 Hard-запрет (после Phase 6): cmake -DSTRICT_NO_QT_IN_CORE=ON
# ══════════════════════════════════════════════════════════════════════════════
option(STRICT_NO_QT_IN_CORE
    "Запретить Qt в naleystogramm-core на уровне компилятора (включать после Phase 6)"
    OFF)

# ── Core sources (core/ + crypto/) ───────────────────────────────────────────
# Цель: naleystogramm-core — статическая библиотека без Qt.
# Статус: МИГРАЦИЯ. Qt ещё присутствует в Phase 6 файлах — убирается поэтапно.
set(CORE_SOURCES
    # Core
    src/core/identity.h
    src/core/identity.cpp
    src/core/device_pairing.h
    src/core/device_pairing.cpp
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
    src/core/updatechecker.h
    src/core/updatechecker.cpp
    src/core/demomode.h
    src/core/demomode.cpp
    src/core/sessionmanager.h
    src/core/sessionmanager.cpp
    src/core/logger.h
    src/core/logger.cpp
    src/core/types.h
    src/core/app.h
    src/core/app.cpp
    src/core/callmanager.h
    src/core/callmanager.cpp
    src/core/remoteshellmanager.h
    src/core/remoteshellmanager.cpp
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
)

# ── UI + entry point sources ──────────────────────────────────────────────────
set(UI_SOURCES
    src/main.cpp

    # Плагины
    src/plugins/iplugin.h
    src/plugins/pluginformat.h
    src/plugins/pluginformat.cpp
    src/plugins/pluginmanager.h
    src/plugins/pluginmanager.cpp

    # UI
    src/ui/mainwindow.h
    src/ui/mainwindow.cpp
    src/ui/mainwindow.ui
    src/ui/thememanager.h
    src/ui/thememanager.cpp
    src/ui/customthememanager.h
    src/ui/customthememanager.cpp
    src/ui/chatwidget.h
    src/ui/chatwidget.cpp
    src/ui/voicewaveform.h
    src/ui/voicewaveform.cpp
    src/ui/contactswidget.h
    src/ui/contactswidget.cpp
    src/ui/sidedrawer.h
    src/ui/sidedrawer.cpp
    src/ui/updatebanner.h
    src/ui/updatebanner.cpp
    src/ui/settingspanel.h
    src/ui/settingspanel.cpp
    src/ui/toggleswitch.h
    src/ui/toggleswitch.cpp
    src/ui/settings/settingspagebase.h
    src/ui/settings/settingshelpers.h
    src/ui/settings/settingspage_main.h
    src/ui/settings/settingspage_main.cpp
    src/ui/settings/settingspage_profile.h
    src/ui/settings/settingspage_profile.cpp
    src/ui/settings/settingspage_network.h
    src/ui/settings/settingspage_network.cpp
    src/ui/settings/settingspage_demo.h
    src/ui/settings/settingspage_demo.cpp
    src/ui/settings/settingspage_privacy.h
    src/ui/settings/settingspage_privacy.cpp
    src/ui/settings/settingspage_security.h
    src/ui/settings/settingspage_security.cpp
    src/ui/settings/settingspage_interface.h
    src/ui/settings/settingspage_interface.cpp
    src/ui/settings/settingspage_devices.h
    src/ui/settings/settingspage_devices.cpp
    src/ui/settings/settingspage_updates.h
    src/ui/settings/settingspage_updates.cpp
    src/ui/settings/settingspage_debug.h
    src/ui/settings/settingspage_debug.cpp
    src/ui/settings/settingspage_plugins.h
    src/ui/settings/settingspage_plugins.cpp
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
    src/ui/qrcodewidget.h
    src/ui/qrcodewidget.cpp
    src/ui/dialogs/devicepairingdialog.h
    src/ui/dialogs/devicepairingdialog.cpp
    src/ui/dialogs/devicelinkdialog.h
    src/ui/dialogs/devicelinkdialog.cpp
    src/ui/filetransferwidget.h
    src/ui/filetransferwidget.cpp
    src/ui/splashscreen.h
    src/ui/splashscreen.cpp
    src/ui/callwindow.h
    src/ui/callwindow.cpp
    src/ui/shellwindow.h
    src/ui/shellwindow.cpp
    src/ui/shellmonitor.h
    src/ui/shellmonitor.cpp

    # Media
    src/media/mediaengine.h
    src/media/mediaengine.cpp

    # Ресурсы
    resources/resources.qrc
)

if(WIN32)
    list(APPEND UI_SOURCES
        resources/windows/app.rc
        src/platform/windowsfirewall.h
        src/platform/windowsfirewall.cpp
    )
    list(APPEND CMAKE_RC_FLAGS "-I \"${CMAKE_SOURCE_DIR}/resources/windows\"")
endif()

# ── Translations ──────────────────────────────────────────────────────────────
set(TS_FILES
    translations/naleystogramm_ru.ts
    translations/naleystogramm_en.ts
    translations/naleystogramm_be.ts
    translations/naleystogramm_uk.ts
    translations/naleystogramm_de.ts
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

# ══════════════════════════════════════════════════════════════════════════════
# ██  naleystogramm-core  ██
# Статическая Qt-free библиотека (целевое состояние).
# Сейчас: Qt присутствует — убирается поэтапно (Фазы 1-7 плана миграции).
# ══════════════════════════════════════════════════════════════════════════════
add_library(naleystogramm-core STATIC ${CORE_SOURCES})

# PUBLIC: типы Qt используются в .h заголовках core — нужны всем, кто include core.
# После миграции этот блок будет удалён.
target_link_libraries(naleystogramm-core PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
    OpenSSL::SSL
    OpenSSL::Crypto
)

# ── Qt-запрет в core ──────────────────────────────────────────────────────────
# STRICT_NO_QT_IN_CORE=ON: компилятор получает -include no_qt.h, который
# через #pragma GCC poison делает любой #include <QXxx> ошибкой компиляции.
# Включать только когда Phase 6 завершена (все Qt убраны из core).
if(STRICT_NO_QT_IN_CORE)
    if(NOT (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"))
        message(FATAL_ERROR "STRICT_NO_QT_IN_CORE требует GCC или Clang")
    endif()
    target_compile_options(naleystogramm-core PRIVATE
        -include ${CMAKE_CURRENT_SOURCE_DIR}/src/core/no_qt.h
    )
    message(STATUS "STRICT_NO_QT_IN_CORE: ON — Qt запрещён в core на уровне компилятора")
else()
    message(STATUS "STRICT_NO_QT_IN_CORE: OFF — используй 'make core-purity' для проверки")
endif()

# Таргет для ручной проверки нарушений Qt в core (не ломает сборку)
add_custom_target(core-purity
    COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan
        "=== Проверка Qt-зависимостей в naleystogramm-core ==="
    COMMAND bash -c
        "violations=\$(grep -rn --include='*.h' --include='*.cpp' \
            -e '#include <Q' -e '#include \"Q' \
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ \
            ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/ \
          | grep -v 'qt_bridge\\.h' \
          | grep -v 'no_qt\\.h'); \
         if [ -n \"\$violations\" ]; then \
           echo ''; \
           echo '\\033[1;31m❌ НАРУШЕНИЯ — Qt в core:\\033[0m'; \
           echo \"\$violations\"; \
           echo ''; \
           echo 'Все Qt↔std конвертации должны идти через src/crypto/qt_bridge.h'; \
           exit 1; \
         else \
           echo '\\033[1;32m✅ Чисто — Qt в core не найден\\033[0m'; \
         fi"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Проверка отсутствия Qt в naleystogramm-core"
    VERBATIM
)

# sqlite3 для Storage (SQLCipher уже подключается ниже, если найден)
if(NOT SQLCIPHER_FOUND)
    pkg_check_modules(SQLITE3 QUIET sqlite3)
    if(SQLITE3_FOUND)
        target_link_libraries(naleystogramm-core PUBLIC ${SQLITE3_LINK_LIBRARIES})
        target_include_directories(naleystogramm-core PUBLIC ${SQLITE3_INCLUDE_DIRS})
    else()
        target_link_libraries(naleystogramm-core PUBLIC sqlite3)
    endif()
endif()

target_include_directories(naleystogramm-core PUBLIC
    src
    src/core
    src/crypto
    ${OPENSSL_INCLUDE_DIR}
)

# APP_VERSION используется в updatechecker.h и sessionmanager.cpp
target_compile_definitions(naleystogramm-core PUBLIC
    APP_VERSION="${PROJECT_VERSION}"
    APP_CODENAME="Шалунишко"
)

# Asio: PUBLIC — network.h включает asio.hpp, поэтому все зависимые таргеты
# (naleystogramm desktop) тоже должны иметь Asio include path.
target_include_directories(naleystogramm-core PUBLIC ${ASIO_INCLUDE_DIR})
target_compile_definitions(naleystogramm-core PUBLIC
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
)
if(nlohmann_json_FOUND)
    target_link_libraries(naleystogramm-core PRIVATE nlohmann_json::nlohmann_json)
else()
    target_link_libraries(naleystogramm-core PRIVATE nlohmann_json::nlohmann_json)
endif()

# Qt6Multimedia опционален — AudioRecorder (core)
if(Qt6Multimedia_FOUND)
    target_link_libraries(naleystogramm-core PUBLIC Qt6::Multimedia)
    target_compile_definitions(naleystogramm-core PUBLIC HAVE_QT_MULTIMEDIA)
    message(STATUS "Qt6Multimedia:   найден — голосовые сообщения включены")
else()
    message(STATUS "Qt6Multimedia:   не найден — голосовые сообщения отключены")
endif()

# Qt6WebSockets опционален — GroupManager (core + UI)
if(Qt6WebSockets_FOUND)
    target_sources(naleystogramm-core PRIVATE
        src/core/groupmanager.h
        src/core/groupmanager.cpp
    )
    target_link_libraries(naleystogramm-core PUBLIC Qt6::WebSockets)
    target_compile_definitions(naleystogramm-core PUBLIC HAVE_QT_WEBSOCKETS)
    message(STATUS "Qt6WebSockets:   найден — группы и каналы включены")
else()
    message(STATUS "Qt6WebSockets:   не найден — группы и каналы отключены")
endif()

# libcurl (UpdateChecker — HTTPS без QNetworkAccessManager)
find_package(CURL QUIET)
if(CURL_FOUND)
    message(STATUS "libcurl:         найден — UpdateChecker использует libcurl")
    target_compile_definitions(naleystogramm-core PRIVATE HAVE_CURL)
    target_link_libraries(naleystogramm-core PRIVATE CURL::libcurl)
else()
    message(STATUS "libcurl:         не найден — UpdateChecker отключён (stub)")
endif()

# SQLCipher (Storage)
if(SQLCIPHER_FOUND)
    message(STATUS "SQLCipher:       найден (${SQLCIPHER_VERSION}) — шифрование БД включено")
    target_compile_definitions(naleystogramm-core PUBLIC HAVE_SQLCIPHER)
    target_include_directories(naleystogramm-core PUBLIC ${SQLCIPHER_INCLUDE_DIRS})
    target_link_libraries(naleystogramm-core PUBLIC ${SQLCIPHER_LINK_LIBRARIES})
else()
    message(STATUS "SQLCipher:       не найден — шифрование БД отключено")
    message(STATUS "                 Установить: sudo apt install libsqlcipher-dev")
endif()

# ══════════════════════════════════════════════════════════════════════════════
# ██  naleystogramm  (Qt desktop executable)  ██
# ══════════════════════════════════════════════════════════════════════════════
add_executable(naleystogramm ${UI_SOURCES})

# GroupChatWidget + JoinGroupDialog — только при наличии Qt6WebSockets
if(Qt6WebSockets_FOUND)
    target_sources(naleystogramm PRIVATE
        src/ui/groupchatwidget.h
        src/ui/groupchatwidget.cpp
        src/ui/dialogs/joingroupdialog.h
        src/ui/dialogs/joingroupdialog.cpp
    )
endif()

add_dependencies(naleystogramm translations)

target_compile_definitions(naleystogramm PRIVATE
    TRANSLATIONS_DIR="${TRANSLATIONS_OUTPUT_DIR}"
    APP_VERSION="${PROJECT_VERSION}"
    APP_CODENAME="Шалунишко"
)

target_include_directories(naleystogramm PRIVATE
    src
    src/ui
    src/media
    src/plugins
)

# naleystogramm-core тянет Qt6::Core/Network/Sql/Concurrent/WebSockets транзитивно
target_link_libraries(naleystogramm PRIVATE
    naleystogramm-core
    Qt6::Widgets
)

# libopus: голосовые звонки (MediaEngine, UI — не в core)
if(OPUS_FOUND)
    message(STATUS "libopus:         найден ${OPUS_VERSION} — голосовые звонки включены")
    target_compile_definitions(naleystogramm PRIVATE HAVE_OPUS)
    target_include_directories(naleystogramm PRIVATE ${OPUS_INCLUDE_DIRS})
    target_link_libraries(naleystogramm PRIVATE ${OPUS_LIBRARIES})
else()
    message(STATUS "libopus:         не найден — голосовые звонки отключены")
    message(STATUS "                 Установить: sudo pacman -S opus  или  sudo apt install libopus-dev")
endif()

# libqrencode: QR-коды (UI только)
if(QRENCODE_FOUND)
    message(STATUS "libqrencode:     найден (${QRENCODE_VERSION}) — QR-коды включены")
    target_compile_definitions(naleystogramm PRIVATE HAVE_QRENCODE)
    target_include_directories(naleystogramm PRIVATE ${QRENCODE_INCLUDE_DIRS})
    target_link_libraries(naleystogramm PRIVATE ${QRENCODE_LIBRARIES})
else()
    message(STATUS "libqrencode:     не найден — QR-коды отключены (код привязки останется)")
    message(STATUS "                 Установить: sudo pacman -S qrencode  или  sudo apt install libqrencode-dev")
endif()

# ══════════════════════════════════════════════════════════════════════════════
# ██  WINDOWS-СПЕЦИФИЧНЫЙ БЛОК  ██
# ══════════════════════════════════════════════════════════════════════════════
if(WIN32)

    set_target_properties(naleystogramm PROPERTIES WIN32_EXECUTABLE ON)

    target_compile_definitions(naleystogramm PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
    )

    target_link_libraries(naleystogramm PRIVATE
        ws2_32
        iphlpapi
        crypt32
        bcrypt
        ncrypt
        secur32
        userenv
        version
        dwmapi
        uxtheme
        winmm
        shell32
        ole32
        oleaut32
        uuid
        gdi32
        user32
        advapi32
        kernel32
        comdlg32
        winspool
        imm32
        netapi32
        setupapi
        dnsapi
        mswsock
        wintrust
    )

    if(MINGW)
        if(WIN_STATIC)
            target_link_options(naleystogramm PRIVATE
                -static
                -static-libgcc
                -static-libstdc++
            )
            message(STATUS "WIN_STATIC: полная статическая линковка (-static -static-libgcc -static-libstdc++)")
        else()
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

    if(MSVC)
        set_property(TARGET naleystogramm PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

        target_link_options(naleystogramm PRIVATE
            "/MANIFEST:EMBED"
            "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/resources/windows/app.manifest"
        )
    endif()

    get_target_property(_qt6_core_type Qt6::Core TYPE)
    if(_qt6_core_type STREQUAL "STATIC_LIBRARY")
        message(STATUS "Qt6: статическая сборка — встраиваем плагины в .exe")
        qt_import_plugins(naleystogramm
            INCLUDE
                Qt6::QWindowsIntegrationPlugin
                Qt6::QWindowsVistaStylePlugin
                Qt6::QSQLiteDriverPlugin
                Qt6::QICOPlugin
                Qt6::QJpegPlugin
                Qt6::QGifPlugin
                Qt6::QGenericEnginePlugin
        )
    else()
        message(STATUS "Qt6: динамическая сборка — нужны platforms/qwindows.dll и др.")
        message(STATUS "     Скопировать через windeployqt: см. build.sh")
    endif()

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
    target_compile_options(naleystogramm-core PRIVATE /W4)
else()
    target_compile_options(naleystogramm PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(naleystogramm-core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Оптимизация Release ───────────────────────────────────────────────────────
if(NOT MSVC)
    set(_release_compile_opts
        $<$<CONFIG:Release>:
            -O2
            -ffunction-sections
            -fdata-sections
            -fvisibility=hidden
            -flto=auto
        >
    )
    set(_release_link_opts
        $<$<CONFIG:Release>:
            -Wl,--gc-sections
            -Wl,-s
            -flto=auto
        >
    )
    target_compile_options(naleystogramm       PRIVATE ${_release_compile_opts})
    target_compile_options(naleystogramm-core  PRIVATE ${_release_compile_opts})
    target_link_options(naleystogramm          PRIVATE ${_release_link_opts})
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
# ══════════════════════════════════════════════════════════════════════════════
set(DEPLOY_SCRIPT "${CMAKE_SOURCE_DIR}/deploy.sh")
set(BUILDS_DIR    "${CMAKE_SOURCE_DIR}/builds")
set(APP_VERSION   "${PROJECT_VERSION}")

if(UNIX AND NOT APPLE)
    add_custom_target(beta
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILDS_DIR}/beta"
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:naleystogramm>
            "${BUILDS_DIR}/beta/naleystogramm"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${TRANSLATIONS_OUTPUT_DIR}"
            "${BUILDS_DIR}/beta/translations"
        COMMAND bash -c
            "printf 'version=${APP_VERSION}\\nbuilt=%s\\ncommit=%s\\nplatform=linux-beta\\n' \
             \"$(date '+%Y-%m-%d %H:%M:%S')\" \
             \"$(git -C '${CMAKE_SOURCE_DIR}' rev-parse --short HEAD 2>/dev/null || echo unknown)\" \
             > '${BUILDS_DIR}/beta/build-info.txt'"
        COMMAND ${CMAKE_COMMAND} -E echo
            "✓ Beta готов: ${BUILDS_DIR}/beta/naleystogramm"
        DEPENDS naleystogramm translations
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Деплой beta → builds/beta/"
        VERBATIM
    )

    add_custom_target(release-linux
        COMMAND bash "${DEPLOY_SCRIPT}" release linux
        DEPENDS naleystogramm translations
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Деплой release Linux AppImage → builds/releases/${APP_VERSION}-linux/"
        VERBATIM
    )
endif()

if(WIN32 OR (CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows"))
    add_subdirectory(installer)
endif()

message(STATUS "")
message(STATUS "Deploy targets:")
if(UNIX AND NOT APPLE)
    message(STATUS "  make beta             → builds/beta/naleystogramm (сырой ELF)")
    message(STATUS "  make release-linux    → builds/releases/${APP_VERSION}-linux/ (AppImage)")
endif()
message(STATUS "  ./deploy.sh release win            → builds/releases/${APP_VERSION}-windows/ (.exe+DLL+zip)")
message(STATUS "  ./deploy.sh release win-installer  → builds/releases/${APP_VERSION}-windows-installer/ (setup.exe)")
message(STATUS "  ./deploy.sh --help                 → полная документация скрипта")
message(STATUS "")
