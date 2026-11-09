# Установка open.mp-компонента omp-cef.
#
# Компонент 1.3.0 не отдаёт гейммоду события из браузера: доставка
# событий — это вызов ProcessMainThreadTasks() из onTick, которого в
# 1.3.0 нет вообще. Поэтому версия фиксирована: ниже 1.4.0 смысла нет.
#
# Скрипт сам определяет разрядность omp-server.exe, качает нужный
# архив, кладёт Cef.dll в components/, убирает cef из строки plugins
# в server.cfg и удаляет кэш ресурсов прошлой версии.

$ErrorActionPreference = 'Stop'
$Version = '1.4.0'

function Get-PeMachine([string]$Path) {
    $fs = [IO.File]::OpenRead($Path)
    try {
        $buf = New-Object byte[] 4096
        [void]$fs.Read($buf, 0, 4096)
        $pe = [BitConverter]::ToUInt32($buf, 0x3C)
        $machine = [BitConverter]::ToUInt16($buf, $pe + 4)
        if ($machine -eq 0x8664) { return 'x64' }
        if ($machine -eq 0x014c) { return 'x86' }
        return ('unknown:0x{0:X4}' -f $machine)
    } finally {
        $fs.Close()
    }
}

function Stop-OpenMpServer {
    $procs = @(Get-Process -Name 'omp-server' -ErrorAction SilentlyContinue)
    if ($procs.Count -eq 0) { return $false }
    Write-Host ''
    Write-Host 'Запущен omp-server.exe — с ним Cef.dll занят и не заменится.'
    $answer = Read-Host 'Остановить его сейчас? (Y/n)'
    if ($answer -eq '' -or $answer -match '^[yYдД]') {
        $procs | Stop-Process -Force
        Start-Sleep -Seconds 2
        return $true
    }
    return $false
}

# ---------------------------------------------------------------- папка сервера
$ServerDir = Join-Path (Split-Path -Parent $PSScriptRoot) 'server'
if (-not (Test-Path (Join-Path $ServerDir 'omp-server.exe'))) {
    $ServerDir = $PSScriptRoot
}
if (-not (Test-Path (Join-Path $ServerDir 'omp-server.exe'))) {
    $ServerDir = Get-Location
}
$serverExe = Join-Path $ServerDir 'omp-server.exe'
if (-not (Test-Path $serverExe)) {
    Write-Host 'Не найден omp-server.exe. Положите скрипт в папку tools/ рядом с server/ и запустите снова.'
    Read-Host 'Enter — выход'
    exit 1
}

$arch = Get-PeMachine $serverExe
if ($arch -ne 'x86' -and $arch -ne 'x64') {
    Write-Host "Не удалось определить разрядность omp-server.exe: $arch"
    Read-Host 'Enter — выход'
    exit 1
}
Write-Host "Сервер: $serverExe ($arch)"

# ---------------------------------------------------------------- скачивание
$zipName = "cef-omp-v$Version-$arch.zip"
$url = "https://github.com/aurora-mp/omp-cef/releases/download/v$Version/$zipName"
$zip = Join-Path $env:TEMP $zipName
$tmp = Join-Path $env:TEMP ('cef-omp-extract-' + [guid]::NewGuid().ToString('N'))

Write-Host "Скачиваю $url"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
try {
    Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
} catch {
    Write-Host ''
    Write-Host "Не скачалось: $($_.Exception.Message)"
    Write-Host "Скачайте руками: $url"
    Read-Host 'Enter — выход'
    exit 1
}

Write-Host 'Распаковываю...'
if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
Expand-Archive -Path $zip -DestinationPath $tmp -Force

$source = Join-Path $tmp 'components/Cef.dll'
if (-not (Test-Path $source)) {
    Write-Host "В архиве нет components\Cef.dll — странный архив, прерываюсь."
    Read-Host 'Enter — выход'
    exit 1
}

$sourceArch = Get-PeMachine $source
if ($sourceArch -ne $arch) {
    Write-Host "Разрядность не совпала: сервер $arch, а в архиве $sourceArch."
    Read-Host 'Enter — выход'
    exit 1
}

# ---------------------------------------------------------------- установка
$componentsDir = Join-Path $ServerDir 'components'
if (-not (Test-Path $componentsDir)) { New-Item -ItemType Directory -Path $componentsDir | Out-Null }
$target = Join-Path $componentsDir 'Cef.dll'

Write-Host "Копирую в $target"
try {
    Copy-Item $source $target -Force
} catch {
    if (Stop-OpenMpServer) {
        try { Copy-Item $source $target -Force }
        catch {
            Write-Host "Не заменилось: $($_.Exception.Message)"
            Read-Host 'Enter — выход'
            exit 1
        }
    } else {
        Write-Host 'Без остановки сервера Cef.dll не заменить.'
        Read-Host 'Enter — выход'
        exit 1
    }
}
Write-Host "Готово: $(Get-PeMachine $target)"

$fileVersion = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($target).FileVersion
if ($fileVersion) {
    Write-Host "Версия файла: $fileVersion"
    if ($fileVersion -notlike "$Version*") {
        Write-Host "ВНИМАНИЕ: ожидалась версия $Version, а в файле $fileVersion."
    }
} else {
    Write-Host 'Версию файла прочитать не удалось — проверьте по строке загрузки в логе.'
}

# ---------------------------------------------------------------- server.cfg
$cfg = Join-Path $ServerDir 'server.cfg'
if (Test-Path $cfg) {
    $lines = [IO.File]::ReadAllText($cfg) -split "`r?`n"
    $out = New-Object System.Collections.Generic.List[string]
    $changed = $false
    foreach ($line in $lines) {
        if ($line -match '^\s*plugins\s') {
            $rest = @($line -split '\s+' | Where-Object { $_ -and $_ -ne 'plugins' -and $_ -ne 'cef' })
            if ($rest.Count -gt 0) { $out.Add('plugins ' + ($rest -join ' ')) }
            $changed = $true
            continue
        }
        $out.Add($line)
    }
    if ($changed) {
        [IO.File]::WriteAllText($cfg, (($out -join "`r`n") + "`r`n"))
        Write-Host 'Убрал cef из строки plugins в server.cfg'
    }
}

# ---------------------------------------------------------------- мусор
$oldPlugin = Join-Path $ServerDir 'plugins/cef.dll'
if (Test-Path $oldPlugin) {
    try { Remove-Item $oldPlugin -Force; Write-Host 'Удалил plugins\cef.dll' }
    catch { Write-Host 'plugins\cef.dll занят (сервер запущен) — удалите вручную' }
}

$pak = Join-Path $ServerDir 'scriptfiles/cef/railway.pak'
if (Test-Path $pak) {
    try { Remove-Item $pak -Force; Write-Host 'Удалил кэш scriptfiles\cef\railway.pak' }
    catch { Write-Host 'railway.pak занят — удалите вручную' }
}

Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ''
Write-Host '============================================'
Write-Host " Компонент omp-cef $Version ($arch) установлен."
Write-Host ' Запустите omp-server.exe и в логе ждите:'
Write-Host "   Successfully loaded component Cef Component ($Version.0)"
Write-Host "   [CEF] Component initialized (v$Version)."
Write-Host ' Если там по-прежнему 1.3.0.0 — файл подменился не там.'
Write-Host '============================================'
Read-Host 'Enter — выход'
