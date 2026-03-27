#!/usr/bin/env bash
# build.sh — собирает Linux (AppImage) и Windows версии
# Запуск: ./build.sh [linux|win|both]
set -e

TARGET="${1:-both}"
BUILD_DIR_LINUX="build-linux"
BUILD_DIR_WIN="build-win"
APP_NAME="naleystogramm"

echo "=== naleystogramm build script ==="

build_linux() {
    echo ""
    echo "--- Building Linux ---"
    cmake -B "$BUILD_DIR_LINUX" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH=/usr/lib/qt6
    cmake --build "$BUILD_DIR_LINUX" --parallel "$(nproc)"
    echo "✓ Linux binary: $BUILD_DIR_LINUX/$APP_NAME"

    echo ""
    echo "--- Packaging AppImage ---"

    # Проверяем что linuxdeploy есть
    if ! command -v linuxdeploy &> /dev/null; then
        echo "ERROR: linuxdeploy not found!"
        echo "Run the setup commands from the README"
        exit 1
    fi
    if ! command -v linuxdeploy-plugin-qt &> /dev/null; then
        echo "ERROR: linuxdeploy-plugin-qt not found!"
        exit 1
    fi

    # Создаём .desktop файл если нет
    if [ ! -f "$APP_NAME.desktop" ]; then
        cat > "$APP_NAME.desktop" <<EOF
[Desktop Entry]
Name=Naleystogramm
Exec=$APP_NAME
Icon=$APP_NAME
Type=Application
Categories=Utility;
EOF
        echo "  + создан $APP_NAME.desktop"
    fi

    # Иконка — берём если есть, иначе linuxdeploy сам разберётся
    ICON_ARG=""
    for ext in png svg ico; do
        if [ -f "assets/$APP_NAME.$ext" ]; then
            ICON_ARG="--icon-file=assets/$APP_NAME.$ext"
            break
        fi
    done

    QMAKE=$(which qmake6 2>/dev/null || which qmake 2>/dev/null || echo "")
    if [ -z "$QMAKE" ]; then
        # Пробуем найти через qt6
        QMAKE=$(find /usr/lib/qt6 -name "qmake" 2>/dev/null | head -1 || true)
    fi

    export QMAKE
    export OUTPUT="${APP_NAME}-x86_64.AppImage"
    export DISABLE_COPYRIGHT_FILES_DEPLOYMENT=1
    export LINUXDEPLOY_PLUGIN_STRIP=0

    linuxdeploy \
        --appdir AppDir \
        --executable "$BUILD_DIR_LINUX/$APP_NAME" \
        --desktop-file "$APP_NAME.desktop" \
        $ICON_ARG \
        --plugin qt \
        --output appimage

    echo ""
    echo "✓ AppImage готов: $OUTPUT"
    echo "  Юзер просто качает и запускает, Qt не нужен"
}

build_windows() {
    echo ""
    echo "--- Building Windows (cross-compile via mingw-w64) ---"
    echo ""
    echo "    Режимы сборки:"
    echo "      ./build.sh win          — libgcc/stdc++/OpenSSL статические, Qt6 DLL"
    echo "      ./build.sh win static   — ПОЛНАЯ статика (требует static Qt6 из AUR)"
    echo ""

    # ── Проверка зависимостей ──────────────────────────────────────────────
    if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
        echo "ОШИБКА: mingw-w64 не найден!"
        echo "Установить: sudo pacman -S mingw-w64-gcc mingw-w64-binutils"
        exit 1
    fi

    if ! command -v x86_64-w64-mingw32-windres &> /dev/null; then
        echo "ПРЕДУПРЕЖДЕНИЕ: windres не найден — манифест UAC не встроится!"
        echo "Установить: sudo pacman -S mingw-w64-binutils"
    fi

    # Корень mingw-w64: DLL в bin/, плагины в lib/qt6/plugins/
    WIN_QT_ROOT="/usr/x86_64-w64-mingw32"
    WIN_QT="$WIN_QT_ROOT"  # обратная совместимость для ссылок ниже
    EXE_DIR="$BUILD_DIR_WIN"

    # ── Режим сборки: статика или динамика Qt6 ──────────────────────────────
    WIN_STATIC_FLAG=""
    if [ "${2}" = "static" ]; then
        echo "  РЕЖИМ: Полная статическая сборка"
        echo ""
        echo "  Требуются пакеты статической Qt6 для MinGW:"
        echo "    yay -S mingw-w64-qt6-base-static \\"
        echo "           mingw-w64-qt6-declarative-static \\"
        echo "           mingw-w64-qt6-tools-static"
        echo ""

        # Ищем статическую Qt6 в стандартных AUR местах
        QT_STATIC_PATH=""
        for candidate in \
            "${WIN_QT}/static" \
            "/usr/x86_64-w64-mingw32/static" \
            "/opt/qt6-mingw-static"
        do
            if [ -f "${candidate}/lib/libQt6Core.a" ]; then
                QT_STATIC_PATH="${candidate}"
                break
            fi
        done

        if [ -z "$QT_STATIC_PATH" ]; then
            echo "  ОШИБКА: статическая Qt6 не найдена!"
            echo "  Проверь что установлен mingw-w64-qt6-base-static из AUR."
            echo "  Или укажи путь вручную: cmake ... -DQT_STATIC_PREFIX=/path"
            echo ""
            echo "  Продолжаем с динамической Qt6 + статическими libgcc/OpenSSL..."
        else
            echo "  ✓ Найдена статическая Qt6: ${QT_STATIC_PATH}"
            WIN_STATIC_FLAG="-DWIN_STATIC=ON -DQT_STATIC_PREFIX=${QT_STATIC_PATH}"
        fi
    fi

    # ── Конфигурация CMake ─────────────────────────────────────────────────
    cmake -B "$BUILD_DIR_WIN" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake \
        ${WIN_STATIC_FLAG}

    # ── Компиляция ─────────────────────────────────────────────────────────
    cmake --build "$BUILD_DIR_WIN" --parallel "$(nproc)"
    echo ""
    echo "✓ Windows .exe собран: $BUILD_DIR_WIN/$APP_NAME.exe"

    # ── При динамической Qt6: копируем нужные DLL ──────────────────────────
    # OpenSSL — статический в .exe (libssl.a / libcrypto.a),
    # libgcc/libstdc++/libpthread — статические (-static-libgcc/-static-libstdc++)
    # Нужны только Qt6 DLL и её зависимости
    if [ -z "$WIN_STATIC_FLAG" ]; then
        echo ""
        echo "--- Копирование Qt6 DLL (OpenSSL и MinGW runtime статические) ---"

        # Qt6 основные DLL
        QT_DLLS=(
            "Qt6Core.dll"
            "Qt6Widgets.dll"
            "Qt6Network.dll"
            "Qt6Sql.dll"
            "Qt6Concurrent.dll"
            "Qt6Gui.dll"
        )
        for dll in "${QT_DLLS[@]}"; do
            src="$WIN_QT/bin/$dll"
            if [ -f "$src" ]; then
                cp "$src" "$EXE_DIR/"
                echo "  + $dll"
            else
                echo "  ? Не найден: $dll"
            fi
        done

        # Qt6 платформенный плагин — КРИТИЧНО: без него QApplication падает!
        mkdir -p "$EXE_DIR/platforms"
        QWINDOWS_DLL="$WIN_QT/lib/qt6/plugins/platforms/qwindows.dll"
        if [ -f "$QWINDOWS_DLL" ]; then
            cp "$QWINDOWS_DLL" "$EXE_DIR/platforms/"
            echo "  + platforms/qwindows.dll  ← КРИТИЧНО (без него крэш при старте)"
        else
            echo "  ОШИБКА: platforms/qwindows.dll не найден!"
            echo "          Приложение упадёт при запуске!"
        fi

        # Qt6 SQLite драйвер
        mkdir -p "$EXE_DIR/sqldrivers"
        QSQLITE_DLL="$WIN_QT/lib/qt6/plugins/sqldrivers/qsqlite.dll"
        if [ -f "$QSQLITE_DLL" ]; then
            cp "$QSQLITE_DLL" "$EXE_DIR/sqldrivers/"
            echo "  + sqldrivers/qsqlite.dll"
        else
            echo "  ? qsqlite.dll не найден (база данных не будет работать)"
        fi

        # Qt6 стили Windows
        mkdir -p "$EXE_DIR/styles"
        QWVISTA_DLL="$WIN_QT/lib/qt6/plugins/styles/qwindowsvistastyle.dll"
        if [ -f "$QWVISTA_DLL" ]; then
            cp "$QWVISTA_DLL" "$EXE_DIR/styles/"
            echo "  + styles/qwindowsvistastyle.dll"
        fi

        # Переводы Qt (диалоги "ОК/Отмена" и т.д.)
        mkdir -p "$EXE_DIR/translations"
        for qm_src in "$WIN_QT/translations/qt_ru.qm" "$WIN_QT/translations/qt_en.qm"; do
            if [ -f "$qm_src" ]; then
                cp "$qm_src" "$EXE_DIR/translations/"
                echo "  + translations/$(basename $qm_src)"
            fi
        done

        echo ""
        echo "  Итого: OpenSSL ✓ статический, libgcc ✓ статический, Qt6 — DLL"
        echo ""
        echo "  Минимальный набор для запуска .exe:"
        echo "    naleystogramm.exe"
        echo "    Qt6Core.dll, Qt6Widgets.dll, Qt6Network.dll, Qt6Sql.dll"
        echo "    Qt6Gui.dll, Qt6Concurrent.dll"
        echo "    platforms/qwindows.dll   ← обязательно!"
        echo "    sqldrivers/qsqlite.dll"
    else
        echo ""
        echo "  Режим полной статики: DLL не нужны, только naleystogramm.exe"
    fi

    echo ""
    echo "✓ Windows пакет готов в: $EXE_DIR/"
    echo ""
    echo "  При двойном клике на .exe Windows запросит права Администратора"
    echo "  (манифест UAC встроен в бинарник через windres)"
}

case "$TARGET" in
    linux) build_linux ;;
    win)   build_windows ;;
    both)  build_linux; build_windows ;;
    *)
        echo "Usage: $0 [linux|win|both]"
        exit 1
        ;;
esac

echo ""
echo "=== Done! ==="