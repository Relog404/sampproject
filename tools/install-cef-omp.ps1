# Установка open.mp-компонента omp-cef.
#
# Компонент 1.3.0 не отдаёт гейммоду события из браузера: доставка
# событий — это вызов ProcessMainThreadTasks() из onTick, которого в
# 1.3.0 нет вообще. Поэтому версия фиксирована: ниже 1.4.0 смысла нет.
#
# Скрипт сам определяет разрядность omp-server.exe, качает нужный
# архив, кладёт Cef.dll в components/, убирает cef из строки plugins
# в server.cfg и удаляет кэш ресурсов прошлой версии. Всё, что делает,
# пишет в install-cef-omp.log рядом с собой.

$ErrorActionPreference = 'Stop'
$Version = '1.4.0'

$LogPath = Join-Path $PSScriptRoot 'install-cef-omp.log'
'' | Out-File -FilePath $LogPath -Encoding utf8

function Say([string]$Text = '') {
    Write-Host $Text
    Add-Content -Path $LogPath -Value $Text -Encoding utf8
}

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

function Get-FileVersion([string]$Path) {
    if (-not (Test-Path $Path)) { return '(нет файла)' }
    $v = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($Path).FileVersion
    if ($v) { return $v }
    return '(версия не читается)'
}

function Describe([string]$Path) {
    if (-not (Test-Path $Path)) { return "$Path — нет файла" }
    $i = Get-Item $Path
    return ('{0} | {1} байт | {2} | версия {3}' -f $i.FullName, $i.Length, $i.LastWriteTime, (Get-FileVersion $Path))
}

function Find-ServerExe([string]$Dir) {
    foreach ($name in @('omp-server.exe', 'open.mp-server.exe', 'openmp-server.exe', 'server.exe')) {
        $candidate = Join-Path $Dir $name
        if (Test-Path $candidate) { return $candidate }
    }
    $any = @(Get-ChildItem -Path $Dir -Filter *.exe -File -ErrorAction SilentlyContinue | Select-Object -First 1)
    if ($any.Count -gt 0) { return $any[0].FullName }
    return $null
}

function Stop-OpenMpServer {
    $procs = @(Get-Process -Name 'omp-server' -ErrorAction SilentlyContinue)
    if ($procs.Count -eq 0) { return $false }
    Say ''
    Say 'Запущен omp-server.exe — с ним Cef.dll занят и не заменится.'
    $answer = Read-Host 'Остановить его сейчас? (Y/n)'
    if ($answer -eq '' -or $answer -match '^[yYдД]') {
        $procs | Stop-Process -Force
        Start-Sleep -Seconds 2
        return $true
    }
    return $false
}

Say '=== Установка omp-cef ' + $Version + ' от ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss') + ' ==='

# ---------------------------------------------------------------- папка сервера
$ServerDir = Join-Path (Split-Path -Parent $PSScriptRoot) 'server'
if (-not (Test-Path (Join-Path $ServerDir 'server.cfg'))) { $ServerDir = $PSScriptRoot }
if (-not (Test-Path (Join-Path $ServerDir 'server.cfg'))) { $ServerDir = (Get-Location).Path }

$serverExe = Find-ServerExe $ServerDir
if (-not $serverExe) {
    Say "Не нашёл exe сервера в $ServerDir"
    Say 'Положите скрипт в папку tools/ рядом с server/ и запустите снова.'
    Read-Host 'Enter — выход'
    exit 1
}

$arch = Get-PeMachine $serverExe
if ($arch -ne 'x86' -and $arch -ne 'x64') {
    Say "Не удалось определить разрядность: $arch"
    Read-Host 'Enter — выход'
    exit 1
}
Say "Папка сервера: $ServerDir"
Say "Сервер: $serverExe ($arch)"

$target = Join-Path $ServerDir 'components/Cef.dll'
Say 'Сейчас в components:'
Say ('  ' + (Describe $target))

# ---------------------------------------------------------------- скачивание
$zipName = "cef-omp-v$Version-$arch.zip"
$url = "https://github.com/aurora-mp/omp-cef/releases/download/v$Version/$zipName"
$zip = Join-Path $env:TEMP $zipName
$tmp = Join-Path $env:TEMP ('cef-omp-extract-' + [guid]::NewGuid().ToString('N'))

Say ''
Say "Скачиваю $url"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
try {
    Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
    Say "Скачано: $((Get-Item $zip).Length) байт"
} catch {
    Say "Не скачалось: $($_.Exception.Message)"
    Say "Скачайте руками: $url"
    Read-Host 'Enter — выход'
    exit 1
}

Say 'Распаковываю...'
if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
Expand-Archive -Path $zip -DestinationPath $tmp -Force

$source = Join-Path $tmp 'components/Cef.dll'
if (-not (Test-Path $source)) {
    Say 'В архиве нет components\Cef.dll — прерываюсь.'
    Read-Host 'Enter — выход'
    exit 1
}

$sourceArch = Get-PeMachine $source
if ($sourceArch -ne $arch) {
    Say "Разрядность не совпала: сервер $arch, а в архиве $sourceArch."
    Read-Host 'Enter — выход'
    exit 1
}
Say ('В архиве: ' + (Describe $source))

# ---------------------------------------------------------------- установка
$componentsDir = Join-Path $ServerDir 'components'
if (-not (Test-Path $componentsDir)) { New-Item -ItemType Directory -Path $componentsDir | Out-Null }

Say ''
Say "Копирую в $target"
try {
    Copy-Item $source $target -Force
} catch {
    Say "Не получилось: $($_.Exception.Message)"
    if (Stop-OpenMpServer) {
        try { Copy-Item $source $target -Force }
        catch {
            Say "Снова не заменилось: $($_.Exception.Message)"
            Read-Host 'Enter — выход'
            exit 1
        }
    } else {
        Say 'Без остановки сервера Cef.dll не заменить.'
        Read-Host 'Enter — выход'
        exit 1
    }
}

Say 'Стало в components:'
Say ('  ' + (Describe $target))

$fileVersion = Get-FileVersion $target
if ($fileVersion -notlike "$Version*") {
    Say ''
    Say "ВНИМАНИЕ: в components лежит версия $fileVersion, а ожидалась $Version."
    Say 'Значит файл подменился не там, откуда его читает open.mp.'
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
            if ($line -match '\bcef\b') { $changed = $true }
            continue
        }
        $out.Add($line)
    }
    if ($changed) {
        [IO.File]::WriteAllText($cfg, (($out -join "`r`n") + "`r`n"))
        Say 'Убрал cef из строки plugins в server.cfg'
    } else {
        Say 'В server.cfg строки plugins с cef нет — уже чисто.'
    }
}

# ---------------------------------------------------------------- мусор
$oldPlugin = Join-Path $ServerDir 'plugins/cef.dll'
if (Test-Path $oldPlugin) {
    try { Remove-Item $oldPlugin -Force; Say 'Удалил plugins\cef.dll' }
    catch { Say 'plugins\cef.dll занят (сервер запущен) — удалите вручную' }
}

$pak = Join-Path $ServerDir 'scriptfiles/cef/railway.pak'
if (Test-Path $pak) {
    try { Remove-Item $pak -Force; Say 'Удалил кэш scriptfiles\cef\railway.pak' }
    catch { Say 'railway.pak занят — удалите вручную' }
}

Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue

# ---------------------------------------------------------------- итог
Say ''
Say 'Все найденные Cef.dll в папке сервера:'
$found = @(Get-ChildItem -Path $ServerDir -Filter Cef.dll -Recurse -File -ErrorAction SilentlyContinue)
if ($found.Count -eq 0) {
    Say '  не найдено ни одного — это плохо'
} else {
    foreach ($f in $found) { Say ('  ' + (Describe $f.FullName)) }
}

Say ''
Say '============================================'
Say ' Готово. Запустите omp-server.exe и в логе ждите:'
Say "   Successfully loaded component Cef Component ($Version.0)"
Say "   [CEF] Component initialized (v$Version)."
Say ' Если там по-прежнему 1.3.0.0 — пришлите этот лог.'
Say " Лог лежит тут: $LogPath"
Say '============================================'
Read-Host 'Enter — выход'
