#Requires -Version 5.1
<#
.SYNOPSIS
    Установщик Naleystogramm для Windows (PowerShell).

.DESCRIPTION
    Скачивает последнюю версию Naleystogramm с GitHub Releases и устанавливает её.
    Альтернатива GUI-инсталлеру naleystogramm-setup.exe для тех, кто предпочитает
    командную строку или автоматизирует развёртывание.

.PARAMETER InstallPath
    Папка установки. По умолчанию: "$Env:ProgramFiles\Naleystogramm"

.PARAMETER NoShortcuts
    Не создавать ярлыки на рабочем столе и в меню Пуск.

.PARAMETER NoFirewall
    Не добавлять правило брандмауэра для порта 47821.

.PARAMETER Uninstall
    Удалить Naleystogramm (читает путь из реестра).

.EXAMPLE
    # Установить с параметрами по умолчанию (требует прав администратора)
    powershell -ExecutionPolicy Bypass -File install.ps1

.EXAMPLE
    # Тихая установка в нестандартную папку
    powershell -ExecutionPolicy Bypass -File install.ps1 -InstallPath "D:\Apps\Naleystogramm" -NoShortcuts

.EXAMPLE
    # Удаление
    powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall
#>

[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$InstallPath = "$Env:ProgramFiles\Naleystogramm",
    [switch]$NoShortcuts,
    [switch]$NoFirewall,
    [switch]$Uninstall
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ── Цвета вывода ──────────────────────────────────────────────────────────────
function Write-Step  { param($msg) Write-Host "  [>>] $msg" -ForegroundColor Cyan    }
function Write-Ok    { param($msg) Write-Host "  [OK] $msg" -ForegroundColor Green   }
function Write-Warn  { param($msg) Write-Host "  [!!] $msg" -ForegroundColor Yellow  }
function Write-Fail  { param($msg) Write-Host "  [XX] $msg" -ForegroundColor Red; exit 1 }

# ── Заголовок ─────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "  Naleystogramm Installer (PowerShell)" -ForegroundColor White
Write-Host "  ──────────────────────────────────────" -ForegroundColor DarkGray
Write-Host ""

# ── Проверка прав администратора ──────────────────────────────────────────────
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Warn "Нет прав администратора. Перезапускаю с elevation..."
    $args = @("-ExecutionPolicy","Bypass","-File","`"$PSCommandPath`"")
    if ($InstallPath -ne "$Env:ProgramFiles\Naleystogramm") { $args += "-InstallPath","$InstallPath" }
    if ($NoShortcuts) { $args += "-NoShortcuts" }
    if ($NoFirewall)  { $args += "-NoFirewall"  }
    if ($Uninstall)   { $args += "-Uninstall"   }
    Start-Process powershell -ArgumentList $args -Verb RunAs -Wait
    exit
}

# ── Константы ─────────────────────────────────────────────────────────────────
$AppName    = "Naleystogramm"
$AppExe     = "naleystogramm.exe"
$AppPort    = 47821
$GitHubRepo = "xomel45/naleystogramm"
$RegKey     = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Naleystogramm"
$TempZip    = Join-Path $Env:TEMP "naleystogramm-latest.zip"

# ════════════════════════════════════════════════════════════════════════════════
# РЕЖИМ: УДАЛЕНИЕ
# ════════════════════════════════════════════════════════════════════════════════
if ($Uninstall) {
    Write-Host "  Удаление $AppName..." -ForegroundColor White
    Write-Host ""

    # Читаем путь установки из реестра
    if (-not (Test-Path $RegKey)) {
        Write-Fail "$AppName не найден в реестре. Возможно, не установлен?"
    }
    $regData    = Get-ItemProperty $RegKey
    $installDir = $regData.InstallLocation

    if (-not $installDir -or -not (Test-Path $installDir)) {
        Write-Warn "Папка установки не найдена: $installDir"
    }

    # Подтверждение
    $confirm = Read-Host "  Удалить $AppName из '$installDir'? [y/N]"
    if ($confirm -notmatch '^[yYдД]') {
        Write-Host "  Отменено." -ForegroundColor Gray
        exit 0
    }

    # Правило брандмауэра
    Write-Step "Удаление правила брандмауэра..."
    try {
        Remove-NetFirewallRule -DisplayName "$AppName P2P" -ErrorAction SilentlyContinue
        Write-Ok "Правило брандмауэра удалено"
    } catch {
        Write-Warn "Не удалось удалить правило брандмауэра: $_"
    }

    # Ярлыки
    Write-Step "Удаление ярлыков..."
    $desk  = [Environment]::GetFolderPath('CommonDesktopDirectory')
    $start = [Environment]::GetFolderPath('CommonPrograms')
    @(
        "$desk\$AppName.lnk",
        "$start\$AppName\$AppName.lnk"
    ) | ForEach-Object {
        if (Test-Path $_) { Remove-Item $_ -Force; Write-Ok "  Удалён: $_" }
    }
    $smDir = "$start\$AppName"
    if (Test-Path $smDir) { Remove-Item $smDir -Recurse -Force }

    # Автозапуск
    $runKey = "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run"
    if (Get-ItemProperty $runKey -Name $AppName -ErrorAction SilentlyContinue) {
        Remove-ItemProperty $runKey -Name $AppName -Force
        Write-Ok "Автозапуск удалён"
    }

    # Реестр
    Write-Step "Удаление записи из реестра..."
    Remove-Item $RegKey -Force -ErrorAction SilentlyContinue
    Write-Ok "Запись реестра удалена"

    # Файлы
    Write-Step "Удаление файлов из '$installDir'..."
    if (Test-Path $installDir) {
        Remove-Item $installDir -Recurse -Force
        Write-Ok "Папка удалена"
    }

    Write-Host ""
    Write-Host "  $AppName успешно удалён." -ForegroundColor Green
    Write-Host ""
    exit 0
}

# ════════════════════════════════════════════════════════════════════════════════
# РЕЖИМ: УСТАНОВКА
# ════════════════════════════════════════════════════════════════════════════════
Write-Host "  Путь установки: $InstallPath" -ForegroundColor Gray
Write-Host ""

# ── Шаг 1: Получение URL последнего релиза ────────────────────────────────────
Write-Step "Получение информации о последнем релизе..."
try {
    $release = Invoke-RestMethod `
        -Uri "https://api.github.com/repos/$GitHubRepo/releases/latest" `
        -Headers @{ 'User-Agent' = 'Naleystogramm-PS-Installer/1.0' }
} catch {
    Write-Fail "Не удалось получить информацию с GitHub: $_"
}

$version = $release.tag_name -replace '^v',''
$asset   = $release.assets | Where-Object { $_.name -match '-windows.*\.zip$' } | Select-Object -First 1

if (-not $asset) {
    Write-Fail "Windows-релиз не найден в assets. Активы: $($release.assets.name -join ', ')"
}

Write-Ok "Версия: $version  |  Файл: $($asset.name)  |  Размер: $([math]::Round($asset.size/1MB,1)) МБ"

# ── Шаг 2: Скачивание ─────────────────────────────────────────────────────────
Write-Step "Скачивание $($asset.name)..."
try {
    # Прогресс-бар PowerShell
    $ProgressPreference = 'Continue'
    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $TempZip -UseBasicParsing
    $ProgressPreference = 'SilentlyContinue'
} catch {
    Write-Fail "Ошибка скачивания: $_"
}
Write-Ok "Скачано: $TempZip ($([math]::Round((Get-Item $TempZip).Length/1MB,1)) МБ)"

# ── Шаг 3: Распаковка ─────────────────────────────────────────────────────────
Write-Step "Распаковка в $InstallPath..."
try {
    if (Test-Path $InstallPath) {
        Remove-Item $InstallPath -Recurse -Force
    }
    New-Item -ItemType Directory -Path $InstallPath -Force | Out-Null
    Expand-Archive -Path $TempZip -DestinationPath $InstallPath -Force
} catch {
    Write-Fail "Ошибка распаковки: $_"
} finally {
    Remove-Item $TempZip -Force -ErrorAction SilentlyContinue
}

# Expand-Archive может создать подпапку с именем архива — перемещаем если так
$subDirs = Get-ChildItem $InstallPath -Directory
if ($subDirs.Count -eq 1 -and (Get-ChildItem $InstallPath -File).Count -eq 0) {
    $sub = $subDirs[0].FullName
    Get-ChildItem $sub | Move-Item -Destination $InstallPath
    Remove-Item $sub -Force
}

$exePath = Join-Path $InstallPath $AppExe
if (-not (Test-Path $exePath)) {
    Write-Fail "$AppExe не найден в $InstallPath после распаковки"
}
Write-Ok "Файлы распакованы"

# ── Шаг 4: Ярлыки ─────────────────────────────────────────────────────────────
if (-not $NoShortcuts) {
    Write-Step "Создание ярлыков..."
    $wsh = New-Object -ComObject WScript.Shell

    # Рабочий стол
    $desk   = [Environment]::GetFolderPath('CommonDesktopDirectory')
    $lnk    = $wsh.CreateShortcut("$desk\$AppName.lnk")
    $lnk.TargetPath       = $exePath
    $lnk.WorkingDirectory = $InstallPath
    $lnk.Description      = "$AppName — P2P мессенджер"
    $lnk.Save()
    Write-Ok "  Рабочий стол: $desk\$AppName.lnk"

    # Меню Пуск
    $start  = [Environment]::GetFolderPath('CommonPrograms')
    $smDir  = "$start\$AppName"
    New-Item -ItemType Directory -Path $smDir -Force | Out-Null
    $lnk    = $wsh.CreateShortcut("$smDir\$AppName.lnk")
    $lnk.TargetPath       = $exePath
    $lnk.WorkingDirectory = $InstallPath
    $lnk.Description      = "$AppName — P2P мессенджер"
    $lnk.Save()
    Write-Ok "  Меню Пуск: $smDir\$AppName.lnk"
}

# ── Шаг 5: Брандмауэр ─────────────────────────────────────────────────────────
if (-not $NoFirewall) {
    Write-Step "Добавление правила брандмауэра (TCP $AppPort)..."
    try {
        # Удаляем старое правило если есть
        Remove-NetFirewallRule -DisplayName "$AppName P2P" -ErrorAction SilentlyContinue

        New-NetFirewallRule `
            -DisplayName "$AppName P2P" `
            -Description "$AppName P2P-мессенджер: входящие TCP на порту $AppPort" `
            -Direction  Inbound `
            -Action     Allow `
            -Protocol   TCP `
            -LocalPort  $AppPort `
            -Program    $exePath `
            -Profile    Any `
            | Out-Null
        Write-Ok "Правило брандмауэра добавлено (TCP $AppPort)"
    } catch {
        Write-Warn "Не удалось добавить правило брандмауэра: $_"
    }
}

# ── Шаг 6: Реестр (Программы и компоненты) ────────────────────────────────────
Write-Step "Регистрация в 'Программы и компоненты'..."
$uninstallCmd = "powershell -ExecutionPolicy Bypass -File `"$InstallPath\install.ps1`" -Uninstall"

# Копируем этот скрипт в папку установки
Copy-Item -Path $PSCommandPath -Destination "$InstallPath\install.ps1" -Force

$regProps = @{
    DisplayName     = $AppName
    DisplayVersion  = $version
    Publisher       = "xomel45"
    URLInfoAbout    = "https://github.com/$GitHubRepo"
    InstallLocation = $InstallPath
    DisplayIcon     = $exePath
    UninstallString = $uninstallCmd
    NoModify        = 1
    NoRepair        = 1
}
if (-not (Test-Path $RegKey)) {
    New-Item -Path $RegKey -Force | Out-Null
}
$regProps.GetEnumerator() | ForEach-Object {
    Set-ItemProperty -Path $RegKey -Name $_.Key -Value $_.Value
}
Write-Ok "Зарегистрировано в реестре"

# ── Финиш ─────────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "  ──────────────────────────────────────" -ForegroundColor DarkGray
Write-Host "  $AppName $version успешно установлен!" -ForegroundColor Green
Write-Host "  Путь: $InstallPath" -ForegroundColor Gray
Write-Host ""
Write-Host "  Для удаления:" -ForegroundColor DarkGray
Write-Host "    powershell -ExecutionPolicy Bypass -File `"$InstallPath\install.ps1`" -Uninstall" -ForegroundColor DarkGray
Write-Host ""

# Предложить запустить
$launch = Read-Host "  Запустить $AppName сейчас? [Y/n]"
if ($launch -notmatch '^[nNнН]') {
    Start-Process $exePath -WorkingDirectory $InstallPath
}
