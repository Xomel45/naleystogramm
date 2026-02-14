#!/usr/bin/env bash
# build.sh — собирает Linux и Windows версии
# Запуск: ./build.sh [linux|win|both]
set -e

TARGET="${1:-both}"
BUILD_DIR_LINUX="build-linux"
BUILD_DIR_WIN="build-win"

echo "=== naleystogramm build script ==="

build_linux() {
    echo ""
    echo "--- Building Linux ---"
    cmake -B "$BUILD_DIR_LINUX" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH=/usr/lib/qt6
    cmake --build "$BUILD_DIR_LINUX" --parallel "$(nproc)"
    echo "✓ Linux binary: $BUILD_DIR_LINUX/naleystogramm"
}

build_windows() {
    echo ""
    echo "--- Building Windows (cross-compile via mingw-w64) ---"

    # Check toolchain
    if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
        echo "ERROR: mingw-w64 not found!"
        echo "Install: sudo pacman -S mingw-w64-gcc"
        exit 1
    fi

    cmake -B "$BUILD_DIR_WIN" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake
    cmake --build "$BUILD_DIR_WIN" --parallel "$(nproc)"
    echo "✓ Windows binary: $BUILD_DIR_WIN/naleystogramm.exe"

    # Copy required Qt6 DLLs next to the .exe
    echo ""
    echo "--- Copying Qt6 DLLs ---"
    WIN_QT="/usr/x86_64-w64-mingw32/sys-root/mingw"
    EXE_DIR="$BUILD_DIR_WIN"

    for dll in \
        "$WIN_QT/bin/Qt6Core.dll" \
        "$WIN_QT/bin/Qt6Widgets.dll" \
        "$WIN_QT/bin/Qt6Network.dll" \
        "$WIN_QT/bin/Qt6Sql.dll" \
        "$WIN_QT/bin/Qt6Concurrent.dll" \
        "$WIN_QT/bin/libssl-3-x64.dll" \
        "$WIN_QT/bin/libcrypto-3-x64.dll" \
        "/usr/x86_64-w64-mingw32/lib/libgcc_s_seh-1.dll" \
        "/usr/x86_64-w64-mingw32/lib/libstdc++-6.dll" \
        "/usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll"
    do
        if [ -f "$dll" ]; then
            cp "$dll" "$EXE_DIR/"
            echo "  + $(basename $dll)"
        else
            echo "  ? Not found: $dll (may need manual copy)"
        fi
    done

    # Qt platform plugin
    mkdir -p "$EXE_DIR/platforms"
    if [ -f "$WIN_QT/lib/qt6/plugins/platforms/qwindows.dll" ]; then
        cp "$WIN_QT/lib/qt6/plugins/platforms/qwindows.dll" "$EXE_DIR/platforms/"
        echo "  + platforms/qwindows.dll"
    fi

    # SQLite driver
    mkdir -p "$EXE_DIR/sqldrivers"
    if [ -f "$WIN_QT/lib/qt6/plugins/sqldrivers/qsqlite.dll" ]; then
        cp "$WIN_QT/lib/qt6/plugins/sqldrivers/qsqlite.dll" "$EXE_DIR/sqldrivers/"
        echo "  + sqldrivers/qsqlite.dll"
    fi

    echo ""
    echo "Windows package ready in: $EXE_DIR/"
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
