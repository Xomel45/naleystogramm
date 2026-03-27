#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════════════════
# deploy.sh — упаковка и развёртывание артефактов сборки Naleystogramm
# ══════════════════════════════════════════════════════════════════════════════
#
# Использование:
#   ./deploy.sh beta                  — сырой ELF → builds/beta/
#   ./deploy.sh release               — все платформы → builds/releases/VERSION-*/
#   ./deploy.sh release linux         — AppImage → builds/releases/VERSION-linux/
#   ./deploy.sh release win           — .exe+DLL → builds/releases/VERSION-windows/
#
# Опции:
#   --build     Пересобрать проект перед деплоем (через CMake)
#   --clean     Удалить целевую директорию перед копированием
#
# Примеры:
#   ./deploy.sh beta --build              Собрать и задеплоить beta
#   ./deploy.sh release linux --clean     AppImage поверх очищенной директории
#   ./deploy.sh release --build --clean   Полный цикл: сборка + релиз всех платформ
# ══════════════════════════════════════════════════════════════════════════════

set -euo pipefail

# ── Цвета и вывод ─────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

log()    { echo -e "${BLUE}[DEPLOY]${NC} $*"; }
ok()     { echo -e "${GREEN}[  OK  ]${NC} $*"; }
warn()   { echo -e "${YELLOW}[ WARN ]${NC} $*"; }
fail()   { echo -e "${RED}[ERROR ]${NC} $*" >&2; exit 1; }
header() { echo -e "\n${BOLD}${CYAN}══ $* ══${NC}"; }
rule()   { printf "${CYAN}%.0s─${NC}" {1..60}; echo; }

# ── Всегда запускаемся из корня проекта ───────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── Константы путей ───────────────────────────────────────────────────────────
APP_NAME="naleystogramm"
BUILD_LINUX="build-linux"                          # выход linux cmake
BUILD_WIN="build-win"                              # выход windows cmake (cross)
BUILDS_DIR="builds"                                # корень артефактов
WIN_MINGW_ROOT="/usr/x86_64-w64-mingw32"           # корень mingw-w64 тулчейна
WIN_QT_DLLS="${WIN_MINGW_ROOT}/bin"                # Qt6*.dll (напр. Qt6Core.dll)
WIN_QT_PLUGINS="${WIN_MINGW_ROOT}/lib/qt6/plugins" # плагины (platforms/, sqldrivers/ ...)

# ── Чтение версии из CMakeLists.txt ──────────────────────────────────────────
# Берём из строки вида: project(naleystogramm VERSION 0.3.2 LANGUAGES CXX RC)
get_version() {
    local v
    v=$(sed -n 's/^project([^ ]* VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt | head -1)
    echo "${v:-unknown}"
}
VERSION=$(get_version)

# ── Парсинг аргументов ────────────────────────────────────────────────────────
MODE="${1:-help}"
PLATFORM="${2:-both}"
DO_BUILD=false
DO_CLEAN=false

for arg in "$@"; do
    [[ "$arg" == "--build" ]] && DO_BUILD=true
    [[ "$arg" == "--clean" ]] && DO_CLEAN=true
done

# ── Вспомогательные функции ───────────────────────────────────────────────────

# Размер файла в человекочитаемом виде
file_size() { du -h "$1" 2>/dev/null | cut -f1 || echo "?"; }

# Получаем короткий git-хеш коммита
git_hash() { git rev-parse --short HEAD 2>/dev/null || echo "unknown"; }

# Сохраняем метаданные сборки рядом с артефактом
write_build_info() {
    local dir="$1"
    printf "version=%s\nbuilt=%s\ncommit=%s\nplatform=%s\n" \
        "$VERSION" \
        "$(date '+%Y-%m-%d %H:%M:%S')" \
        "$(git_hash)" \
        "${2:-linux}" \
        > "$dir/build-info.txt"
}

# Копируем файл с логированием; при отсутствии — warn, не fail
copy_file() {
    local src="$1" dst="$2" label="$3"
    if [[ -f "$src" ]]; then
        cp "$src" "$dst"
        ok "  + ${label:-$(basename "$src")}"
        return 0
    else
        warn "  ? Не найден: ${label:-$(basename "$src")}"
        return 1
    fi
}

# Создаём структуру builds/ если её нет (не добавлять в git!)
ensure_builds_tree() {
    mkdir -p "$BUILDS_DIR/beta"
    mkdir -p "$BUILDS_DIR/releases"
}

# L-3: безопасное удаление — только внутри BUILDS_DIR.
# Защищает от path-traversal через VERSION (например, "../../../etc").
# Использует realpath -m (без требования существования пути).
safe_clean() {
    local target="$1"
    local abs_builds abs_target
    abs_builds="$(realpath -m "$BUILDS_DIR")"
    abs_target="$(realpath -m "$target")"

    if [[ "$abs_target" != "$abs_builds/"* && "$abs_target" != "$abs_builds" ]]; then
        fail "safe_clean: цель '${target}' (→ ${abs_target}) находится вне ${abs_builds} — отказ в удалении!"
    fi

    log "Очистка $target ..."
    rm -rf "${target:?}"
}

# ── Сборка (опциональная) ─────────────────────────────────────────────────────

build_linux() {
    header "Сборка Linux (Release)"
    cmake -B "$BUILD_LINUX" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH=/usr/lib/qt6
    cmake --build "$BUILD_LINUX" --parallel "$(nproc)"
    ok "Linux бинарник собран: $BUILD_LINUX/$APP_NAME"
}

build_windows() {
    header "Сборка Windows (cross-compile MinGW)"
    cmake -B "$BUILD_WIN" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake
    cmake --build "$BUILD_WIN" --parallel "$(nproc)"
    ok "Windows .exe собран: $BUILD_WIN/$APP_NAME.exe"
}

# ── РЕЖИМ: Beta ───────────────────────────────────────────────────────────────
# Назначение: быстрая проверка, итерации.
# Артефакт:   сырой ELF без AppImage-упаковки (нет зависимостей Qt рядом!).
# Путь:       builds/beta/naleystogramm
deploy_beta() {
    header "Beta Deploy → ${BUILDS_DIR}/beta/"
    ensure_builds_tree

    # Сборка если запрошена или бинарника нет
    if $DO_BUILD || [[ ! -f "$BUILD_LINUX/$APP_NAME" ]]; then
        build_linux
    fi

    local dest="${BUILDS_DIR}/beta"

    # Опциональная очистка
    $DO_CLEAN && { safe_clean "$dest"; }

    # Копирование ELF
    cp "$BUILD_LINUX/$APP_NAME" "$dest/$APP_NAME"
    chmod +x "$dest/$APP_NAME"

    # Копируем переводы (нужны при запуске из этой же директории)
    if [[ -d "$BUILD_LINUX/translations" ]]; then
        mkdir -p "$dest/translations"
        cp "$BUILD_LINUX/translations/"*.qm "$dest/translations/" 2>/dev/null || true
        ok "  + translations/"
    fi

    write_build_info "$dest" "linux-beta"

    rule
    ok "Beta готов!"
    echo "  Путь:    ${BOLD}$dest/$APP_NAME${NC}"
    echo "  Размер:  $(file_size "$dest/$APP_NAME")"
    echo "  Версия:  $VERSION  |  commit: $(git_hash)"
    echo ""
    echo -e "  ${YELLOW}Запуск (требует Qt6 в системе):${NC}"
    echo "    ./$dest/$APP_NAME"
    echo ""
}

# ── РЕЖИМ: Release Linux (AppImage) ──────────────────────────────────────────
# Назначение: финальный дистрибутив для Linux.
# Артефакт:   самодостаточный AppImage (Qt + плагины + переводы внутри).
# Путь:       builds/releases/VERSION-linux/Naleystogramm-VERSION-x86_64.AppImage
deploy_release_linux() {
    header "Release Linux → ${BUILDS_DIR}/releases/${VERSION}-linux/"
    ensure_builds_tree

    # Сборка если запрошена или бинарника нет
    if $DO_BUILD || [[ ! -f "$BUILD_LINUX/$APP_NAME" ]]; then
        build_linux
    fi

    local dest="${BUILDS_DIR}/releases/${VERSION}-linux"
    local appimage_name="Naleystogramm-${VERSION}-x86_64.AppImage"

    # Опциональная очистка
    $DO_CLEAN && { safe_clean "$dest"; }
    mkdir -p "$dest"

    # Запускаем make_appimage.sh из BUILD_LINUX чтобы linuxdeploy создал
    # AppImage именно там (он помещает .AppImage в рабочую директорию)
    log "Запуск make_appimage.sh..."
    (
        cd "$BUILD_LINUX"
        bash "$SCRIPT_DIR/scripts/make_appimage.sh" "$SCRIPT_DIR/$BUILD_LINUX"
    )

    # Находим созданный AppImage (make_appimage.sh кладёт его в BUILD_LINUX/)
    local created_appimage
    created_appimage=$(ls "$BUILD_LINUX"/Naleystogramm-*.AppImage 2>/dev/null | sort -V | tail -1 || true)

    if [[ -z "$created_appimage" || ! -f "$created_appimage" ]]; then
        fail "AppImage не найден в $BUILD_LINUX/ после сборки! Проверь вывод make_appimage.sh."
    fi

    # Переносим в релизную директорию с правильным именем (версия из CMakeLists.txt)
    cp "$created_appimage" "$dest/$appimage_name"
    chmod +x "$dest/$appimage_name"

    write_build_info "$dest" "linux"

    rule
    ok "Release Linux готов!"
    echo "  Директория:  ${BOLD}$dest/${NC}"
    echo "  AppImage:    $appimage_name"
    echo "  Размер:      $(file_size "$dest/$appimage_name")"
    echo "  Версия:      $VERSION  |  commit: $(git_hash)"
    echo ""
    echo "  Запуск (переносимый, Qt не нужен в системе):"
    echo "    chmod +x $dest/$appimage_name"
    echo "    ./$dest/$appimage_name"
    echo ""
}

# ── РЕЖИМ: Release Windows (.exe + DLL) ──────────────────────────────────────
# Назначение: финальный дистрибутив для Windows.
# Артефакт:   папка ready-to-run: .exe + Qt DLL + плагины + переводы.
# Путь:       builds/releases/VERSION-windows/
#
# Статически встроены в .exe (DLL НЕ нужны):
#   libssl.a / libcrypto.a  — OPENSSL_USE_STATIC_LIBS=TRUE
#   libgcc, libstdc++       — -static-libgcc / -static-libstdc++
# Копируется в пакет (нужна Qt6Core.dll):
#   libwinpthread-1.dll     — Qt6Core.dll динамически импортирует её
deploy_release_windows() {
    header "Release Windows → ${BUILDS_DIR}/releases/${VERSION}-windows/"
    ensure_builds_tree

    # Сборка если запрошена или .exe нет
    if $DO_BUILD || [[ ! -f "$BUILD_WIN/$APP_NAME.exe" ]]; then
        build_windows
    fi

    local dest="${BUILDS_DIR}/releases/${VERSION}-windows"

    # Опциональная очистка
    $DO_CLEAN && { safe_clean "$dest"; }

    # Создаём структуру директорий пакета
    mkdir -p "$dest"/{platforms,sqldrivers,styles,tls,networkinformation,translations}

    local ok_count=0
    local warn_count=0

    # Обёртка copy_file с подсчётом
    copy_tracked() {
        if copy_file "$@"; then
            (( ok_count++ )) || true
        else
            (( warn_count++ )) || true
        fi
    }

    log "Копирование .exe..."
    copy_tracked "$BUILD_WIN/$APP_NAME.exe" "$dest/$APP_NAME.exe" "$APP_NAME.exe"

    log "Копирование Qt6 основных DLL..."
    for dll in Qt6Core Qt6Widgets Qt6Network Qt6Sql Qt6Concurrent Qt6Gui Qt6Multimedia; do
        copy_tracked "${WIN_QT_DLLS}/${dll}.dll" "$dest/${dll}.dll" "${dll}.dll"
    done

    # Мультимедиа бэкенды (опционально: нужны для воспроизведения голосовых)
    mkdir -p "$dest/multimedia"
    for mm_dll in "${WIN_QT_PLUGINS}/multimedia/"*.dll; do
        [[ -f "$mm_dll" ]] && copy_tracked "$mm_dll" \
            "$dest/multimedia/$(basename "$mm_dll")" \
            "multimedia/$(basename "$mm_dll")"
    done

    # Платформенный плагин — КРИТИЧНО: QApplication упадёт без него!
    log "Копирование Qt6 платформенного плагина..."
    copy_tracked \
        "${WIN_QT_PLUGINS}/platforms/qwindows.dll" \
        "$dest/platforms/qwindows.dll" \
        "platforms/qwindows.dll  ← без него крэш!"

    log "Копирование Qt6 плагинов..."
    copy_tracked \
        "${WIN_QT_PLUGINS}/sqldrivers/qsqlite.dll" \
        "$dest/sqldrivers/qsqlite.dll" \
        "sqldrivers/qsqlite.dll"

    # Qt6.8+: qmodernwindowsstyle.dll; старые версии: qwindowsvistastyle.dll
    for style_dll in qmodernwindowsstyle qwindowsvistastyle; do
        if [[ -f "${WIN_QT_PLUGINS}/styles/${style_dll}.dll" ]]; then
            copy_tracked \
                "${WIN_QT_PLUGINS}/styles/${style_dll}.dll" \
                "$dest/styles/${style_dll}.dll" \
                "styles/${style_dll}.dll"
            break
        fi
    done

    # TLS плагин (для Qt Network / TLS соединений)
    for tls_dll in "${WIN_QT_PLUGINS}/tls/"*.dll; do
        [[ -f "$tls_dll" ]] && copy_tracked "$tls_dll" "$dest/tls/$(basename "$tls_dll")" \
            "tls/$(basename "$tls_dll")"
    done

    # Network information плагин
    for ni_dll in "${WIN_QT_PLUGINS}/networkinformation/"*.dll; do
        [[ -f "$ni_dll" ]] && copy_tracked "$ni_dll" \
            "$dest/networkinformation/$(basename "$ni_dll")" \
            "networkinformation/$(basename "$ni_dll")"
    done

    # Переводы приложения
    log "Копирование переводов..."
    if [[ -d "$BUILD_WIN/translations" ]]; then
        cp "$BUILD_WIN/translations/"*.qm "$dest/translations/" 2>/dev/null || true
        ok "  + translations/*.qm"
    fi

    # MinGW runtime + транзитивные зависимости Qt — полный список, выявлен
    # рекурсивным анализом objdump по всем DLL в пакете.
    # Системные DLL (kernel32, user32, api-ms-win-*, d3d*, dwrite и т.д.)
    # не включаются — они присутствуют на любой Windows 10+.
    log "Копирование MinGW runtime и зависимостей Qt..."
    for rt_dll in \
        libgcc_s_seh-1.dll \
        libstdc++-6.dll \
        libssp-0.dll \
        libwinpthread-1.dll \
        zlib1.dll \
        libpng16-16.dll \
        libfreetype-6.dll \
        libbrotlidec.dll \
        libbrotlicommon.dll \
        libbz2-1.dll \
        libharfbuzz-0.dll \
        libglib-2.0-0.dll \
        libgraphite2.dll \
        libintl-8.dll \
        libiconv-2.dll \
        libpcre2-8-0.dll \
        libpcre2-16-0.dll \
        libsqlite3-0.dll \
        libzstd.dll; do
        copy_tracked \
            "${WIN_MINGW_ROOT}/bin/${rt_dll}" \
            "$dest/${rt_dll}" \
            "${rt_dll}"
    done

    # Статусная информация
    log "  OpenSSL:  статически встроен в .exe (DLL не нужны)"
    log "  MinGW runtime: libgcc/libstdc++/libssp/libwinpthread — скопированы (требуют Qt DLL)"

    # Метаданные сборки
    write_build_info "$dest" "windows"

    # README для конечного пользователя (на русском и английском)
    cat > "$dest/README.txt" << EOF
Naleystogramm v${VERSION} — Windows Release
============================================

Требования / Requirements:
  - Windows 10 / 11 (x86_64)
  - Права Администратора (UAC встроен в .exe / Admin rights built-in)

Запуск / Launch:
  Двойной клик → Windows запросит права Администратора → разрешить.
  Double-click → Windows will ask for Admin rights → allow.

Структура папки:
  naleystogramm.exe         — основной исполняемый файл
  platforms/qwindows.dll    — Qt платформа Windows (обязательна!)
  sqldrivers/qsqlite.dll    — Qt SQLite (база данных контактов)
  styles/                   — Qt стили Windows 10/11
  tls/                      — Qt TLS плагины (зашифрованные соединения)
  networkinformation/        — Qt сетевая информация
  translations/             — переводы интерфейса

Встроено статически (отдельные DLL не нужны):
  OpenSSL ${VERSION} — шифрование (libssl.a / libcrypto.a)
  MinGW runtime    — libgcc, libstdc++, libpthread

Версия: ${VERSION}
Сборка: $(date '+%Y-%m-%d')
Коммит: $(git_hash)
EOF
    ok "  + README.txt"

    rule
    ok "Release Windows готов!"
    echo "  Директория:  ${BOLD}$dest/${NC}"
    echo "  .exe:        $APP_NAME.exe  ($(file_size "$dest/$APP_NAME.exe"))"
    echo "  Скопировано: $ok_count  |  Пропущено: $warn_count"
    echo ""
    echo "  Содержимое пакета:"
    find "$dest" -type f | sort | while IFS= read -r f; do
        printf "    %-52s %s\n" "${f#"$dest"/}" "$(file_size "$f")"
    done
    echo ""
    echo -e "  ${YELLOW}При двойном клике на .exe — Windows запросит права Администратора${NC}"
    echo "  (UAC requireAdministrator встроен через windres в PE-ресурс)"
    echo ""
}

# ── Вывод помощи ─────────────────────────────────────────────────────────────
show_help() {
    echo ""
    echo -e "${BOLD}${CYAN}deploy.sh${NC} — упаковщик артефактов Naleystogramm v${BOLD}${VERSION}${NC}"
    rule
    echo ""
    echo -e "  ${BOLD}Использование:${NC}"
    echo "    ./deploy.sh beta                  Сырой ELF → builds/beta/"
    echo "    ./deploy.sh release               Обе платформы → builds/releases/VERSION-*/"
    echo "    ./deploy.sh release linux         AppImage → builds/releases/${VERSION}-linux/"
    echo "    ./deploy.sh release win           .exe+DLL → builds/releases/${VERSION}-windows/"
    echo ""
    echo -e "  ${BOLD}Опции:${NC}"
    echo "    --build     Пересобрать через CMake перед деплоем"
    echo "    --clean     Удалить целевую директорию перед копированием"
    echo ""
    echo -e "  ${BOLD}Примеры:${NC}"
    echo "    ./deploy.sh beta --build              # собрать + beta"
    echo "    ./deploy.sh release linux --clean     # AppImage поверх чистой папки"
    echo "    ./deploy.sh release --build --clean   # полный цикл"
    echo ""
    echo -e "  ${BOLD}Структура вывода:${NC}"
    echo "    builds/"
    echo "    ├── beta/"
    echo "    │   ├── naleystogramm          (Linux ELF, требует Qt6 в системе)"
    echo "    │   ├── translations/"
    echo "    │   └── build-info.txt"
    echo "    └── releases/"
    echo "        ├── ${VERSION}-linux/"
    echo "        │   ├── Naleystogramm-${VERSION}-x86_64.AppImage"
    echo "        │   └── build-info.txt"
    echo "        └── ${VERSION}-windows/"
    echo "            ├── naleystogramm.exe"
    echo "            ├── platforms/qwindows.dll"
    echo "            ├── sqldrivers/qsqlite.dll"
    echo "            ├── styles/..."
    echo "            ├── translations/"
    echo "            ├── README.txt"
    echo "            └── build-info.txt"
    echo ""
    echo -e "  ${CYAN}CMake targets (альтернатива):${NC}"
    echo "    cmake --build build-linux --target beta"
    echo "    cmake --build build-linux --target release-linux"
    echo "    cmake --build build-win   --target release-windows"
    echo ""
}

# ── Точка входа ───────────────────────────────────────────────────────────────
case "$MODE" in
    beta)
        deploy_beta
        ;;
    release)
        case "$PLATFORM" in
            linux)
                deploy_release_linux
                ;;
            win|windows)
                deploy_release_windows
                ;;
            both|--build|--clean|*)
                # Если второй аргумент — опция, значит обе платформы
                deploy_release_linux
                deploy_release_windows
                ;;
        esac
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        show_help
        fail "Неизвестная команда: '$MODE'"
        ;;
esac
