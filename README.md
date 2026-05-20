<div align="center">

# Naleystogramm

**Зашифрованный P2P-мессенджер без серверов и слежки**

[![Version](https://img.shields.io/badge/version-0.7.4-7c6aff?style=flat-square)](https://github.com/Xomel45/naleystogramm/releases)
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
chmod +x Naleystogramm-0.7.4-x86_64.AppImage
./Naleystogramm-0.7.4-x86_64.AppImage

# Arch Linux / pacman
sudo pacman -U naleystogramm-0.7.4-1-x86_64.pkg.tar.zst

# Debian / Ubuntu / Mint
sudo dpkg -i naleystogramm_0.7.4_amd64.deb

# Fedora / RHEL / openSUSE
sudo dnf install ./naleystogramm-0.7.4-1.x86_64.rpm
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
sudo pacman -S qt6-base qt6-multimedia opus openssl cmake

# Ubuntu / Debian
sudo apt install qt6-base-dev qt6-multimedia-dev libopus-dev libssl-dev cmake
```

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

# Только Windows: cmake + DLL + zip → builds/releases/0.7.4-windows/
./deploy.sh release win

# Linux AppImage → builds/releases/0.7.4-linux/
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

Артефакты: `builds/releases/0.7.4-linux/` и `builds/releases/0.7.4-windows/` (+ `.zip`)

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

### v0.7.4 «ыЪы»

**Сеть**
- Новый тип подключения «Разблокированный порт» в настройках сети — для случаев когда порт пробит вручную на роутере без UPnP
- `PortForwardingMode::OpenPort`: внешний IP определяется автоматически (как в UPnP-режиме), анонсируемый порт задаётся вручную
- Индикатор в статус-баре: в режиме Open Port вместо «UPnP ✓/✗» отображается «Open Port: XXXXX ✓/✗» с проверкой доступности через TCP self-connect
- `NetworkManager::checkOpenPort()` — асинхронная проверка достижимости порта с таймаутом 5 сек

**Сплеш-экран**
- Тема сплеш-экрана берётся из настроек пользователя (раньше была фиксирована)
- Прогресс-бар стал вдвое толще (10 px)
- Надпись «Made by Xomelz & Claude» плавно появляется по мере заполнения прогресс-бара (opacity 0 → 1)

**Сборка и упаковка**
- Windows релиз полностью перенесён в `deploy.sh`: одна команда `./deploy.sh release win` выполняет cmake configure → сборку → копирование DLL → создание zip-архива с максимальным сжатием
- `./deploy.sh release all` — все платформы одной командой: AppImage + `.pkg.tar.zst` + `.deb` + `.rpm` + Windows + zip
- CMake-таргет `release-windows` удалён (был тонкой обёрткой над deploy.sh)
- `--build` флаг больше не нужен для Windows: сборка всегда запускается инкрементально

**Исправления**
- Кнопка «Сохранить» в панели настроек отображалась чёрной без наведения — исправлено (конфликт CSS-селекторов `QWidget#headerBar QWidget`)
- Текст кнопок «Очистить» и «Экспорт» в панели логов обрезался — исправлено (убран `setFixedHeight`)

---

### v0.7.3 «Кокошонка»

**Упаковка**
- Новые форматы релиза: `.pkg.tar.zst` (Arch/pacman), `.deb` (Debian/Ubuntu), `.rpm` (Fedora/RHEL) — в дополнение к AppImage и Windows
- `deploy.sh release pkg/deb/rpm` — сборка отдельного формата; `linux-all` — все Linux форматы; `all` — все платформы
- `.pkg.tar.zst` содержит `.PKGINFO` с зависимостями (`qt6-base`, `openssl`), устанавливается через `pacman -U`
- Сборка использует `nproc - 2` ядра вместо всех доступных

**Безопасность и криптография**
- `remoteShellEnabled` по умолчанию `false` вместо `true` — удалённый шелл больше не включён у новых пользователей
- Double Ratchet: `dhRatchet` переведён на паттерн «commit on success» — state не изменяется при ошибке генерации DH-ключа
- Double Ratchet: все EVP-вызовы в `aesgcmEncrypt`/`aesgcmDecrypt` теперь проверяются; сбой любого шага возвращает ошибку вместо тихого продолжения
- Double Ratchet: `encrypt` проверяет `state.initialized` перед шифрованием
- X3DH: каждый DH-результат проверяется индивидуально до конкатенации IKM
- X3DH: `kdf()` проверяет возврат `EVP_PKEY_derive`
- X3DH: `generateBundle` возвращает `false` при сбое Ed25519, вместо нулевой подписи с кодом успеха

**Прочее**
- `SessionManager`: все сеттеры используют отложенное сохранение (`QTimer` 500 мс) вместо записи на диск при каждом изменении
- `SessionManager`: версия берётся из `APP_VERSION` (CMake `PROJECT_VERSION`) — больше не дублируется хардкодом
- Убран `#include "../ui/thememanager.h"` из core-модуля
- Минимальная версия пира: **0.7.3**

---

### v0.7.2 «Дырявый носок»

**Безопасность**
- Запрет загрузки и сохранения приватных ключей в открытом виде — при сбое KeyProtector операция прерывается вместо тихого plaintext-фолбэка; существующие plaintext-ключи мигрируют в зашифрованный формат при первом запуске
- `PRAGMA secure_delete=ON` — удалённые строки и страницы БД перезаписываются нулями (защита от forensic-восстановления)
- SPK-подпись стала обязательной: соединение с пиром без `ik_ed` отклоняется (устранён обход MITM-защиты для «старых клиентов»)
- Rate limiting: более 200 фреймов в секунду от одного пира — принудительный разрыв соединения
- Убран ключевой материал (частичные hex-дампы CK/RK/MK/DH) из debug-логов
- Минимальная версия пира для установки соединения: **0.7.2**; HANDSHAKE и HANDSHAKE_ACK теперь содержат поле `version`

**Криптография**
- OTPK pool увеличен с 10 до 100 ключей — снижает вероятность отсутствия one-time pre-key при параллельных сессиях

**Интерфейс**
- Динамическая инверсия иконок: все иконки автоматически адаптируются к цвету темы (`ThemeManager::applyIcon` / `tintedIcon`) — иконки корректно отображаются как на тёмных, так и на светлых темах
- Все кнопки главного окна получили иконки: профиль, редактирование, настройки, добавить контакт
- Все кнопки чата получили иконки: прикрепить файл, звонок, микрофон (со сменой на `input_voice_active` при записи), отправить
- Кнопки окна звонка получили иконки: принять, отклонить, заглушить микрофон (с переключением `mic_on`/`mic_off`), завершить
- Кнопка «Назад» в настройках получила иконку
- Заголовки всех секций настроек получили тематические иконки
- Кнопка воспроизведения голосовых сообщений получила иконки `media_play`/`media_pause`
- Контекстное меню «Копировать» по правой кнопке на пузырях сообщений
- Иконки статуса доставки (`msg_sent` / `msg_delivered`) видны сразу при отправке
- Иконки в контекстном меню контактов (блок, заглушить, удалить и др.) адаптированы к теме через `tintedIcon`
- Иконка кнопки Settings заменена с palette на gear (`settings_btn.png`)
- Удалены 7 дублирующихся иконок из ресурсов

**Прочее**
- Кодовое имя: «Клутой ШлЕпка» → «Дырявый носок»

---

<div align="center">

*v0.7.4 «ыЪы»*

</div>
