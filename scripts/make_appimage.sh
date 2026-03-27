#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# Скрипт создания AppImage для Naleystogramm
# ═══════════════════════════════════════════════════════════════════════════════
#
# Использование:
#   ./scripts/make_appimage.sh [BUILD_DIR]
#
# BUILD_DIR по умолчанию: build-linux
#
# Требования:
#   - Собранный бинарник naleystogramm в BUILD_DIR
#   - Qt6 установлен в системе
#   - OpenSSL установлен
#   - wget или curl для загрузки инструментов
#
# ═══════════════════════════════════════════════════════════════════════════════

set -e  # Выход при ошибке

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

info() { echo -e "${BLUE}[INFO]${NC} $1"; }
success() { echo -e "${GREEN}[OK]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# ── Параметры ─────────────────────────────────────────────────────────────────

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${1:-$PROJECT_DIR/build-linux}"
APP_NAME="naleystogramm"
APP_VERSION=$(sed -n 's/^project([^ ]* VERSION \([0-9][0-9.]*\).*/\1/p' "$PROJECT_DIR/CMakeLists.txt" | head -1)
APP_VERSION="${APP_VERSION:-0.5.1}"
APPDIR="$BUILD_DIR/AppDir"
TOOLS_DIR="$BUILD_DIR/appimage-tools"

# ── Проверка предварительных условий ──────────────────────────────────────────

info "Проверка предварительных условий..."

if [ ! -f "$BUILD_DIR/$APP_NAME" ]; then
    error "Бинарник не найден: $BUILD_DIR/$APP_NAME\nСначала соберите проект: cd build-linux && cmake .. && make"
fi

# Проверяем наличие Qt
QMAKE=$(which qmake6 2>/dev/null || which qmake 2>/dev/null || echo "")
if [ -z "$QMAKE" ]; then
    error "qmake не найден. Убедитесь, что Qt6 установлен и qmake в PATH."
fi
QT_DIR=$($QMAKE -query QT_INSTALL_PREFIX)
QT_PLUGINS=$($QMAKE -query QT_INSTALL_PLUGINS)
QT_LIBS=$($QMAKE -query QT_INSTALL_LIBS)

info "Qt найден: $QT_DIR"
info "Qt plugins: $QT_PLUGINS"

# ── Загрузка инструментов ─────────────────────────────────────────────────────

mkdir -p "$TOOLS_DIR"

# linuxdeploy — современная альтернатива linuxdeployqt
LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"

if [ ! -f "$LINUXDEPLOY" ]; then
    info "Загрузка linuxdeploy..."
    wget -q --show-progress -O "$LINUXDEPLOY" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" || \
        curl -L -o "$LINUXDEPLOY" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY"
    success "linuxdeploy загружен"
fi

if [ ! -f "$LINUXDEPLOY_QT" ]; then
    info "Загрузка linuxdeploy-plugin-qt..."
    wget -q --show-progress -O "$LINUXDEPLOY_QT" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" || \
        curl -L -o "$LINUXDEPLOY_QT" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY_QT"
    success "linuxdeploy-plugin-qt загружен"
fi

# ── Создание структуры AppDir ─────────────────────────────────────────────────

info "Создание структуры AppDir..."

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$APPDIR/usr/share/translations"

# ── Копирование бинарника ─────────────────────────────────────────────────────

info "Копирование бинарника..."
cp "$BUILD_DIR/$APP_NAME" "$APPDIR/usr/bin/"
chmod +x "$APPDIR/usr/bin/$APP_NAME"
success "Бинарник скопирован"

# ── Копирование .desktop файла ────────────────────────────────────────────────

info "Копирование .desktop файла..."
cp "$PROJECT_DIR/naleystogramm.desktop" "$APPDIR/usr/share/applications/"
cp "$PROJECT_DIR/naleystogramm.desktop" "$APPDIR/"
success ".desktop файл скопирован"

# ── Копирование иконки ────────────────────────────────────────────────────────

info "Копирование иконки..."
ICON_SRC="$PROJECT_DIR/resources/icons/app_icon.png"
if [ -f "$ICON_SRC" ]; then
    cp "$ICON_SRC" "$APPDIR/usr/share/icons/hicolor/256x256/apps/${APP_NAME}.png"
    cp "$ICON_SRC" "$APPDIR/${APP_NAME}.png"
    success "Иконка скопирована"
else
    warn "Иконка не найдена: $ICON_SRC"
    # Создаём placeholder
    if command -v magick &> /dev/null; then
        magick -size 256x256 xc:'#7c3aed' \
            -fill '#a78bfa' -draw "roundrectangle 20,20 235,235 25,25" \
            "$APPDIR/${APP_NAME}.png"
        cp "$APPDIR/${APP_NAME}.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/"
        warn "Создана placeholder иконка"
    fi
fi

# ── Копирование переводов ─────────────────────────────────────────────────────

info "Копирование переводов..."
TRANSLATIONS_DIR="$BUILD_DIR/translations"
if [ -d "$TRANSLATIONS_DIR" ]; then
    cp -r "$TRANSLATIONS_DIR"/*.qm "$APPDIR/usr/share/translations/" 2>/dev/null || true
    success "Переводы скопированы"
else
    warn "Директория переводов не найдена: $TRANSLATIONS_DIR"
fi

# ── Копирование OpenSSL библиотек ─────────────────────────────────────────────

info "Копирование OpenSSL библиотек..."

# Находим OpenSSL библиотеки
OPENSSL_LIBS=$(ldd "$BUILD_DIR/$APP_NAME" | grep -E 'libssl|libcrypto' | awk '{print $3}' | grep -v '^$')

for lib in $OPENSSL_LIBS; do
    if [ -f "$lib" ]; then
        cp "$lib" "$APPDIR/usr/lib/"
        info "  Скопирован: $(basename $lib)"
    fi
done

# Также копируем символьные ссылки
for lib in libssl.so libssl.so.3 libcrypto.so libcrypto.so.3; do
    LIBPATH="/usr/lib/$lib"
    [ -f "$LIBPATH" ] && cp -L "$LIBPATH" "$APPDIR/usr/lib/" 2>/dev/null || true
    LIBPATH="/usr/lib/x86_64-linux-gnu/$lib"
    [ -f "$LIBPATH" ] && cp -L "$LIBPATH" "$APPDIR/usr/lib/" 2>/dev/null || true
done

success "OpenSSL библиотеки скопированы"

# ── Создание AppRun скрипта ───────────────────────────────────────────────────

info "Создание AppRun скрипта..."

cat > "$APPDIR/AppRun" << 'APPRUN_EOF'
#!/bin/bash
# AppRun script for Naleystogramm

SELF=$(readlink -f "$0")
HERE=${SELF%/*}

# Устанавливаем пути к библиотекам и плагинам
export LD_LIBRARY_PATH="${HERE}/usr/lib:${HERE}/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins:${QT_PLUGIN_PATH}"
export QT_QPA_PLATFORM_PLUGIN_PATH="${HERE}/usr/plugins/platforms"
export QML_IMPORT_PATH="${HERE}/usr/qml:${QML_IMPORT_PATH}"

# Путь к переводам
export TRANSLATIONS_DIR="${HERE}/usr/share/translations"

# XDG пути
export XDG_DATA_DIRS="${HERE}/usr/share:${XDG_DATA_DIRS}"

# Запускаем приложение
exec "${HERE}/usr/bin/naleystogramm" "$@"
APPRUN_EOF

chmod +x "$APPDIR/AppRun"
success "AppRun создан"

# ── Ручное развёртывание Qt (обход проблемных SQL драйверов) ─────────────────

info "Развёртывание Qt зависимостей вручную..."

# Отключаем strip (не поддерживает современные библиотеки с .relr.dyn)
export NO_STRIP=1

# ── Функция для копирования библиотеки со всеми зависимостями ────────────────
copy_lib_deps() {
    local lib="$1"
    local dest="$APPDIR/usr/lib"

    if [ ! -f "$lib" ]; then
        return
    fi

    # Копируем саму библиотеку
    local basename=$(basename "$lib")
    if [ ! -f "$dest/$basename" ]; then
        cp -L "$lib" "$dest/" 2>/dev/null || true
    fi

    # Рекурсивно копируем зависимости
    ldd "$lib" 2>/dev/null | grep "=>" | awk '{print $3}' | while read dep; do
        if [ -f "$dep" ] && [ ! -f "$dest/$(basename $dep)" ]; then
            # Пропускаем системные библиотеки
            case "$dep" in
                /lib/ld-linux*|/lib64/ld-linux*|*/libc.so*|*/libm.so*|*/libpthread.so*|*/libdl.so*|*/librt.so*)
                    continue
                    ;;
            esac
            cp -L "$dep" "$dest/" 2>/dev/null || true
        fi
    done
}

# ── Копирование Qt библиотек ─────────────────────────────────────────────────
info "Копирование Qt библиотек..."

# Получаем список Qt библиотек из бинарника
QT_USED_LIBS=$(ldd "$APPDIR/usr/bin/$APP_NAME" | grep -i qt | awk '{print $3}')
for lib in $QT_USED_LIBS; do
    copy_lib_deps "$lib"
done

# Дополнительные Qt библиотеки, которые могут понадобиться плагинам
for lib in "$QT_LIBS"/libQt6{Svg,DBus,XcbQpa,OpenGL,OpenGLWidgets}.so*; do
    [ -f "$lib" ] && copy_lib_deps "$lib"
done

success "Qt библиотеки скопированы"

# ── Копирование Qt плагинов ──────────────────────────────────────────────────
info "Копирование Qt плагинов..."

mkdir -p "$APPDIR/usr/plugins"

# Platforms (обязательно)
mkdir -p "$APPDIR/usr/plugins/platforms"
cp "$QT_PLUGINS/platforms/libqxcb.so" "$APPDIR/usr/plugins/platforms/" 2>/dev/null || true
cp "$QT_PLUGINS/platforms/libqwayland"*.so "$APPDIR/usr/plugins/platforms/" 2>/dev/null || true

# Platform themes
mkdir -p "$APPDIR/usr/plugins/platformthemes"
cp "$QT_PLUGINS/platformthemes/"*.so "$APPDIR/usr/plugins/platformthemes/" 2>/dev/null || true

# Styles
mkdir -p "$APPDIR/usr/plugins/styles"
cp "$QT_PLUGINS/styles/"*.so "$APPDIR/usr/plugins/styles/" 2>/dev/null || true

# Image formats
mkdir -p "$APPDIR/usr/plugins/imageformats"
cp "$QT_PLUGINS/imageformats/"*.so "$APPDIR/usr/plugins/imageformats/" 2>/dev/null || true

# Icon engines
mkdir -p "$APPDIR/usr/plugins/iconengines"
cp "$QT_PLUGINS/iconengines/"*.so "$APPDIR/usr/plugins/iconengines/" 2>/dev/null || true

# TLS (для сетевого взаимодействия)
mkdir -p "$APPDIR/usr/plugins/tls"
cp "$QT_PLUGINS/tls/"*.so "$APPDIR/usr/plugins/tls/" 2>/dev/null || true

# Network information
mkdir -p "$APPDIR/usr/plugins/networkinformation"
cp "$QT_PLUGINS/networkinformation/"*.so "$APPDIR/usr/plugins/networkinformation/" 2>/dev/null || true

# SQL драйверы — ТОЛЬКО SQLite!
mkdir -p "$APPDIR/usr/plugins/sqldrivers"
cp "$QT_PLUGINS/sqldrivers/libqsqlite.so" "$APPDIR/usr/plugins/sqldrivers/" 2>/dev/null || true

# XCB GL integrations
mkdir -p "$APPDIR/usr/plugins/xcbglintegrations"
cp "$QT_PLUGINS/xcbglintegrations/"*.so "$APPDIR/usr/plugins/xcbglintegrations/" 2>/dev/null || true

# Platform input contexts
mkdir -p "$APPDIR/usr/plugins/platforminputcontexts"
cp "$QT_PLUGINS/platforminputcontexts/"*.so "$APPDIR/usr/plugins/platforminputcontexts/" 2>/dev/null || true

success "Qt плагины скопированы"

# ── Копирование зависимостей плагинов ────────────────────────────────────────
info "Копирование зависимостей плагинов..."

find "$APPDIR/usr/plugins" -name "*.so" | while read plugin; do
    copy_lib_deps "$plugin"
done

success "Зависимости плагинов скопированы"

# ── Копирование дополнительных системных библиотек ───────────────────────────
info "Копирование системных зависимостей..."

# Копируем зависимости основного бинарника
ldd "$APPDIR/usr/bin/$APP_NAME" | grep "=>" | awk '{print $3}' | while read lib; do
    if [ -f "$lib" ]; then
        case "$lib" in
            /lib/ld-linux*|/lib64/ld-linux*|*/libc.so*|*/libm.so*|*/libpthread.so*|*/libdl.so*|*/librt.so*)
                continue
                ;;
        esac
        basename=$(basename "$lib")
        [ ! -f "$APPDIR/usr/lib/$basename" ] && cp -L "$lib" "$APPDIR/usr/lib/" 2>/dev/null || true
    fi
done

success "Системные зависимости скопированы"

# ── Запуск linuxdeploy для создания AppImage ─────────────────────────────────
info "Создание AppImage..."

"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --output appimage || {
        error "Не удалось создать AppImage"
    }

# linuxdeploy создаёт AppImage в текущей директории, переносим в build
APPIMAGE_NAME="Naleystogramm-${APP_VERSION}-x86_64.AppImage"
CREATED_APPIMAGE=$(ls -1 Naleystogramm*.AppImage 2>/dev/null | head -1)

if [ -n "$CREATED_APPIMAGE" ] && [ -f "$CREATED_APPIMAGE" ]; then
    mv "$CREATED_APPIMAGE" "$BUILD_DIR/$APPIMAGE_NAME"
    success "AppImage создан: $BUILD_DIR/$APPIMAGE_NAME"
else
    # Пробуем найти с любым именем
    CREATED_APPIMAGE=$(ls -1 *.AppImage 2>/dev/null | head -1)
    if [ -n "$CREATED_APPIMAGE" ] && [ -f "$CREATED_APPIMAGE" ]; then
        mv "$CREATED_APPIMAGE" "$BUILD_DIR/$APPIMAGE_NAME"
        success "AppImage создан: $BUILD_DIR/$APPIMAGE_NAME"
    else
        error "AppImage не был создан!"
    fi
fi

# ── Вывод информации ──────────────────────────────────────────────────────────

echo ""
echo "═══════════════════════════════════════════════════════════════════════════════"
success "AppImage успешно создан!"
echo "═══════════════════════════════════════════════════════════════════════════════"
echo ""
echo "  Файл: $BUILD_DIR/$APPIMAGE_NAME"
echo "  Размер: $(du -h "$BUILD_DIR/$APPIMAGE_NAME" | cut -f1)"
echo ""
echo "  Запуск:"
echo "    chmod +x $APPIMAGE_NAME"
echo "    ./$APPIMAGE_NAME"
echo ""
echo "═══════════════════════════════════════════════════════════════════════════════"
