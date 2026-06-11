<div align="center">

# Naleystogramm

**Зашифрованный P2P-мессенджер без серверов и слежки**

[![Version](https://img.shields.io/badge/version-0.8.2-7c6aff?style=flat-square)](https://github.com/Xomel45/naleystogramm/releases)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-4a4a7a?style=flat-square)](#установка)
[![Qt](https://img.shields.io/badge/Qt-6.x-41cd52?style=flat-square)](https://www.qt.io/)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue?style=flat-square)](LICENSE)

</div>

---

## О проекте

Naleystogramm — десктопный мессенджер с прямым зашифрованным соединением между пользователями. Никаких центральных серверов, никакой регистрации, никакого хранения переписки на чужих машинах.

Соединение устанавливается напрямую по TCP — как в старые добрые времена ICQ, только всё зашифровано Double Ratchet + AES-256-GCM.

---

## Скриншоты

<div align="center">

### Сплеш-экран
<img src="assets/splash.png" width="480" alt="Splash screen"/>

### Главное окно
<img src="assets/main.png" width="800" alt="Главное окно — контакты и чат"/>

### Добавление контакта
<img src="assets/add-contact.png" width="480" alt="Добавление контакта по строке подключения"/>

### Настройки — Профиль
<img src="assets/settings-1.png" width="600" alt="Настройки — имя и аватар"/>

### Настройки — Безопасность и интерфейс
<img src="assets/settings-2.png" width="600" alt="Настройки — безопасность и темы"/>

### Настройки — Язык и отладка
<img src="assets/settings-3.png" width="600" alt="Настройки — язык и логи"/>

</div>

---

## Возможности

### Сообщения и файлы
- Текстовые сообщения с E2E-шифрованием (Double Ratchet)
- Передача файлов с паузой/возобновлением (AES-256-GCM)
- Голосовые сообщения (WAV PCM 16-bit 16 кГц)
- Индикаторы онлайн/оффлайн в реальном времени

### Голосовые звонки
- VoIP на базе Opus (32 кбит/с, DTX)
- Jitter-буфер с Opus PLC при потере пакетов
- Зашифрованные медиапотоки (AES-256-GCM по UDP)

### Сеть
- Прямое P2P-соединение (TCP)
- UPnP для автоматического пробоса портов
- Ручная настройка IP/порта (для VPN, статических адресов)
- Режим ретрансляции через собственный relay-сервер
- Автоматическое переподключение с keepalive PING/PONG

### Безопасность
- End-to-end шифрование на каждое сообщение (Double Ratchet)
- Шифрование базы данных (SQLCipher, опционально)
- Удалённый шелл с защитой от эскалации привилегий
- Верификация собеседника по Safety Number

### Интерфейс
- 7 встроенных тем + поддержка пользовательских тем (zip/tar.gz/7z)
- Русский и английский языки
- Демо-режим (скрывает реальные данные в UI)
- Проверка обновлений через GitHub Releases

---

## Установка

### Linux

Скачай нужный пакет из [Releases](https://github.com/Xomel45/naleystogramm/releases):

```bash
# AppImage — работает на любом дистрибутиве, Qt не нужен
chmod +x Naleystogramm-0.8.2-x86_64.AppImage
./Naleystogramm-0.8.2-x86_64.AppImage

# Arch Linux / pacman
sudo pacman -U naleystogramm-0.8.2-1-x86_64.pkg.tar.zst

# Debian / Ubuntu / Mint
sudo dpkg -i naleystogramm_0.8.2_amd64.deb

# Fedora / RHEL / openSUSE
sudo dnf install ./naleystogramm-0.8.2-1.x86_64.rpm
```

### Windows

Скачай архив из [Releases](https://github.com/Xomel45/naleystogramm/releases), распакуй и запусти `naleystogramm.exe`.

> При первом запуске Windows попросит права администратора — это нужно для открытия порта в брандмауэре.

---

## Как подключиться к кому-то

1. Запусти приложение
2. В главном окне нажми **«+ Добавить контакт»**
3. Вставь строку подключения собеседника в формате `Имя@UUID@IP:Порт`
4. Строку подключения собеседник находит в своих настройках → **«Скопировать строку подключения»**

---

## Сборка из исходников

### Зависимости (Linux)

```bash
# Arch / Manjaro
sudo pacman -S qt6-base qt6-multimedia opus openssl cmake qrencode

# Ubuntu / Debian
sudo apt install qt6-base-dev qt6-multimedia-dev libopus-dev libssl-dev cmake libqrencode-dev
```

> `qt6-multimedia`, `opus` и `qrencode` — опциональные: без них сборка проходит, но голосовые сообщения, голосовые звонки и QR-код привязки устройств будут недоступны.

### Linux

```bash
cmake -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --target naleystogramm -j$(( $(nproc) - 2 ))
```

### Windows (кросс-компиляция с Linux через MinGW-w64)

```bash
# Только dev-сборка бинаря (без упаковки)
cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake
cmake --build build-win -j$(( $(nproc) - 2 ))
```

### Релизная сборка

```bash
# Все платформы одной командой (рекомендуется)
./deploy.sh release all

# Только Windows: cmake + DLL + zip → builds/releases/0.7.5-windows/
./deploy.sh release win

# Linux AppImage → builds/releases/0.7.5-linux/
./deploy.sh release linux

# Все Linux форматы: AppImage + .pkg.tar.zst + .deb + .rpm
./deploy.sh release linux-all

# Пакет для конкретного дистрибутива
./deploy.sh release arch      # или pkg  → .pkg.tar.zst (Arch, Manjaro, Garuda, CachyOS...)
./deploy.sh release debian    # или deb  → .deb         (Ubuntu, Mint, Kali, Pop!_OS...)
./deploy.sh release rh        # или rpm  → .rpm         (Fedora, RHEL, openSUSE, Nobara...)

# Авто-определение дистрибутива — соберёт нужный пакет сам
./deploy.sh release my

# Очистить директории перед сборкой
./deploy.sh release all --clean
```

Артефакты: `builds/releases/0.8.2-linux/` и `builds/releases/0.8.2-windows/` (+ `.zip`)

---

## Технологии

| | |
|---|---|
| **UI** | Qt 6, QSS темы |
| **Шифрование** | OpenSSL — AES-256-GCM, Double Ratchet, X25519 ECDH |
| **Голос** | Opus, Qt6Multimedia |
| **База данных** | SQLite / SQLCipher |
| **Сеть** | TCP (JSON-фреймы), UDP (медиа) |
| **Сборка** | CMake 3.22+, кросс-компиляция MinGW-w64 |

---

## Структура проекта

```
src/
├── core/         — сетевой стек, E2E, хранилище, сессия
├── media/        — медиадвижок (Opus, UDP)
├── ui/           — виджеты, темы, диалоги
└── main.cpp
assets/           — скриншоты для README
cmake/            — тулчейн MinGW-w64
scripts/          — make_appimage.sh
translations/     — .ts/.qm файлы локализации
deploy.sh         — скрипт сборки релизов
```

---

## Changelog

См. [CHANGELOG.md](CHANGELOG.md).

Последний релиз — **v0.8.2 «Оплошность»**.

*Изменения в Android-версии — см. [naleystogramm-mobile](https://github.com/Xomel45/naleystogramm-mobile)*

---

<div align="center">

*v0.8.2 «Оплошность»*

</div>
