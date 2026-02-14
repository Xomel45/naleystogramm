cmake_minimum_required(VERSION 3.22)
project(naleystogramm VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # строго стандарт, без GNU-расширений
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS src/ui)

# ── Find Qt6 ──────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Sql
    Concurrent
)

# ── Find OpenSSL ──────────────────────────────────────────────────────────
find_package(OpenSSL REQUIRED)

# ── Sources ───────────────────────────────────────────────────────────────
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
    src/core/upnp.h
    src/core/upnp.cpp

    # Crypto
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
    src/ui/updatebanner.h
    src/ui/updatebanner.cpp
    src/ui/settingspanel.h
    src/ui/settingspanel.cpp
    src/ui/dialogs/addcontactdialog.h
    src/ui/dialogs/addcontactdialog.cpp
    src/ui/dialogs/incomingdialog.h
    src/ui/dialogs/incomingdialog.cpp  # FIX: вынесен в отдельный файл
)

# ── Executable ────────────────────────────────────────────────────────────
if(WIN32)
    # No console window on Windows
    add_executable(naleystogramm WIN32 ${SOURCES})
else()
    add_executable(naleystogramm ${SOURCES})
endif()

target_include_directories(naleystogramm PRIVATE
    src
    src/core
    src/crypto
    src/ui
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

# ── Windows: link winsock ─────────────────────────────────────────────────
if(WIN32)
    target_link_libraries(naleystogramm PRIVATE ws2_32 iphlpapi)
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(naleystogramm PRIVATE /W4)
else()
    target_compile_options(naleystogramm PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Build info ────────────────────────────────────────────────────────────
message(STATUS "Qt version:      ${Qt6_VERSION}")
message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
message(STATUS "Compiler:        ${CMAKE_CXX_COMPILER}")
message(STATUS "Target system:   ${CMAKE_SYSTEM_NAME}")
