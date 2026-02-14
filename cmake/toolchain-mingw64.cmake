# Cross-compilation toolchain: Arch Linux → Windows x86_64 (mingw-w64)
# Usage:
#   cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake
#   cmake --build build-win

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# mingw-w64 toolchain binaries
set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

find_program(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
find_program(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
find_program(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR
        "mingw-w64 not found!\n"
        "Install: sudo pacman -S mingw-w64-gcc")
endif()

set(CMAKE_FIND_ROOT_PATH
    /usr/${TOOLCHAIN_PREFIX}
    /usr/${TOOLCHAIN_PREFIX}/sys-root/mingw
)

# Qt6 from AUR mingw-w64-qt6-base installs here
list(APPEND CMAKE_PREFIX_PATH
    /usr/${TOOLCHAIN_PREFIX}/sys-root/mingw
    /usr/lib/qt6   # fallback
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Windows-specific flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")

# Suppress console window on Windows release builds (set per-target in CMakeLists)
# WIN32 target property handles this
