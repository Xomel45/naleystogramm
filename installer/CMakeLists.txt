cmake_minimum_required(VERSION 3.20)
project(naleystogramm-installer C)

# ── Только для Windows (кросс-компиляция MinGW) ────────────────────────────
if(NOT WIN32 AND NOT CMAKE_CROSSCOMPILING)
    message(STATUS "Инсталлер собирается только под Windows (MinGW). Пропускаем.")
    return()
endif()

# ── Путь к payload.zip (создаётся deploy.sh перед сборкой инсталлера) ──────
set(PAYLOAD_ZIP "${CMAKE_CURRENT_SOURCE_DIR}/payload.zip"
    CACHE FILEPATH "Путь к payload.zip для встраивания в инсталлер")

if(NOT EXISTS "${PAYLOAD_ZIP}")
    message(WARNING
        "payload.zip не найден: ${PAYLOAD_ZIP}\n"
        "Запустите сначала: ./deploy.sh release win\n"
        "Затем: cmake --build build-win --target release-windows-installer")
    # Создаём пустой placeholder чтобы windres не падал
    file(WRITE "${PAYLOAD_ZIP}" "")
endif()

# ── Исходники ──────────────────────────────────────────────────────────────
set(INSTALLER_SOURCES
    installer.c
    install.c
    uninstall.c
    extract.c
    download.c
    registry.c
    shortcuts.c
    firewall.c
    strings.c
)

set(INSTALLER_RC installer.rc)

# ── Исполняемый файл ───────────────────────────────────────────────────────
add_executable(naleystogramm-setup WIN32
    ${INSTALLER_SOURCES}
    ${INSTALLER_RC}
)

# ── Определения компилятора ────────────────────────────────────────────────
target_compile_definitions(naleystogramm-setup PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00   # Windows 10+
    WINVER=0x0A00
    APP_VERSION=L"${PROJECT_VERSION}"
)

# ── Флаги C ────────────────────────────────────────────────────────────────
target_compile_options(naleystogramm-setup PRIVATE
    -std=c23
    -Wall
    -Wextra
    -O2
    $<$<CONFIG:Release>:-s>   # strip символы в Release
)

# ── Линкер: статическая сборка без зависимостей от DLL ────────────────────
# zlib: статически (zlib.a из MinGW)
# wininet, ole32, oleaut32, uuid: системные (на любой Windows)
target_link_libraries(naleystogramm-setup PRIVATE
    -static-libgcc
    wininet       # WinINet (HTTP/HTTPS загрузка)
    ole32         # COM (IShellLink, INetFwPolicy2)
    oleaut32      # COM automation
    uuid          # CLSID/IID constants
    shell32       # SHGetFolderPath, ShellExecute
    comdlg32      # SHBrowseForFolder
    comctl32      # InitCommonControlsEx, PROGRESS_CLASS
)

# ── Версия из родительского проекта ───────────────────────────────────────
if(DEFINED PROJECT_VERSION AND NOT PROJECT_VERSION STREQUAL "")
    target_compile_definitions(naleystogramm-setup PRIVATE
        APP_VERSION=L"${PROJECT_VERSION}"
    )
endif()

# ── Выходное имя ──────────────────────────────────────────────────────────
set_target_properties(naleystogramm-setup PROPERTIES
    OUTPUT_NAME "naleystogramm-setup"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# ── Target: release-windows-installer (для вызова из родительского cmake) ─
add_custom_target(release-windows-installer
    DEPENDS naleystogramm-setup
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_SOURCE_DIR}/builds/releases/${PROJECT_VERSION}-windows-installer"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:naleystogramm-setup>"
        "${CMAKE_SOURCE_DIR}/builds/releases/${PROJECT_VERSION}-windows-installer/naleystogramm-setup.exe"
    COMMENT "Установщик → builds/releases/${PROJECT_VERSION}-windows-installer/"
)
