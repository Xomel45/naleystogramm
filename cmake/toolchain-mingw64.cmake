# Кросс-компиляция: Arch/Manjaro Linux → Windows x86_64 (mingw-w64)
#
# Использование (динамическая Qt6, OpenSSL статический):
#   cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake
#   cmake --build build-win
#
# Использование (ПОЛНОСТЬЮ статическая сборка, Qt6 тоже статическая):
#   cmake -B build-win \
#       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake \
#       -DWIN_STATIC=ON \
#       -DQT_STATIC_PREFIX=/usr/x86_64-w64-mingw32/sys-root/mingw/static
#   cmake --build build-win
#
# Требования для полной статики:
#   yay -S mingw-w64-qt6-base-static mingw-w64-qt6-tools-static
#   (плюс аналогичные -static пакеты для других модулей Qt)

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# ── Бинарники mingw-w64 ────────────────────────────────────────────────────────
set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

find_program(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc  REQUIRED)
find_program(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++  REQUIRED)
find_program(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR
        "mingw-w64 не найден!\n"
        "Установить: sudo pacman -S mingw-w64-gcc\n"
        "или:        yay -S mingw-w64-gcc")
endif()

if(NOT CMAKE_RC_COMPILER)
    message(WARNING
        "windres (${TOOLCHAIN_PREFIX}-windres) не найден!\n"
        "Манифест UAC НЕ будет встроен в .exe!\n"
        "Установить: sudo pacman -S mingw-w64-binutils")
endif()

# ── Пути поиска библиотек ──────────────────────────────────────────────────────
# CMAKE_FIND_ROOT_PATH — корень для поиска всего (libs, includes, packages)
set(CMAKE_FIND_ROOT_PATH
    /usr/${TOOLCHAIN_PREFIX}
    /usr/${TOOLCHAIN_PREFIX}/sys-root/mingw
)

# Qt6 из AUR пакетов mingw-w64-qt6-* устанавливается сюда
list(APPEND CMAKE_PREFIX_PATH
    /usr/${TOOLCHAIN_PREFIX}/sys-root/mingw
    /usr/${TOOLCHAIN_PREFIX}
)

# Режимы поиска: программы ищем в хост-системе, всё остальное — в target root
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ── Флаги линкера MinGW ────────────────────────────────────────────────────────
# Базовый минимум для toolchain-файла: только явные GCC-драйверные флаги.
# Все позиционные -Wl,-Bstatic/-Bdynamic управляются из CMakeLists.txt
# через target_link_options — там правильный порядок гарантируется структурой
# if(WIN_STATIC)/else().
#
# -static-libgcc  : libgcc.a → .exe (нет libgcc_s_seh-1.dll)
# -static-libstdc++: libstdc++.a → .exe (нет libstdc++-6.dll)
#
# Почему НЕТ -Wl,-Bdynamic здесь:
#   CMAKE_EXE_LINKER_FLAGS попадает в НАЧАЛО команды линкера — ДО объектных
#   файлов и библиотек. Если здесь поставить -Wl,-Bdynamic, он включает
#   динамический поиск как DEFAULT для всех библиотек, и -static из
#   target_link_options уже не может это надёжно перекрыть.
#   CMakeLists.txt сам добавит -Wl,-Bstatic,-lpthread,-lssp,-Bdynamic
#   в нужном месте (WIN_STATIC=OFF) или -static (WIN_STATIC=ON).
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")

# ── Оптимизации для Release сборки ────────────────────────────────────────────
# -O2            : оптимизация скорости
# -s             : стриппинг debug символов (уменьшает размер .exe)
# -ffunction-sections -fdata-sections : секции на функцию/данные
# -Wl,--gc-sections : удаляем неиспользуемые секции (уменьшает размер)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE_INIT
    "-O2 -s -Wl,--gc-sections")
set(CMAKE_CXX_FLAGS_RELEASE_INIT
    "-O2 -ffunction-sections -fdata-sections")
