#Requires -Version 5.1
<#
    Railway — диагностика клиента.
    ------------------------------------------------------------------
    Проверяет:
      • какой билд samp.dll стоит в игре (R1 / R3-1 / R5-1 / R5-2 / 0.3.DL);
      • сможет ли CEF-плагин определить этот билд;
      • на месте ли cef.asi, папка cef\ и ASI loader.

    Запуск:
      powershell -ExecutionPolicy Bypass -File check-samp-build.ps1
      powershell -ExecutionPolicy Bypass -File check-samp-build.ps1 -GtaPath "D:\Games\GTA SA"

    Справка: omp-cef v1.4.0 ищет samp.dll по PE-подписи (TimeDateStamp +
    SizeOfImage), а если подпись незнакомая — по версии файла. Для R5-2
    версия файла 0.3.7.5, поэтому билд распознаётся как 0.3.7-R5 (тот же
    набор адресов, что и у R5-1).
#>
[CmdletBinding()]
param(
    [string]$GtaPath = '',
    [string]$SampDll = ''
)

$ErrorActionPreference = 'Stop'

function Write-Ok([string]$Text)    { Write-Host "  [ok]  $Text" -ForegroundColor Green }
function Write-Warn2([string]$Text) { Write-Host "  [!]   $Text" -ForegroundColor Yellow }
function Write-Bad([string]$Text)   { Write-Host "  [x]   $Text" -ForegroundColor Red }
function Write-Note([string]$Text)  { Write-Host "        $Text" -ForegroundColor DarkGray }

function Find-GtaFolder {
    $registryKeys = @(
        'HKLM:\SOFTWARE\WOW6432Node\Rockstar Games\GTA San Andreas',
        'HKLM:\SOFTWARE\Rockstar Games\GTA San Andreas',
        'HKLM:\SOFTWARE\WOW6432Node\SAMP',
        'HKLM:\SOFTWARE\SAMP'
    )

    foreach ($key in $registryKeys) {
        if (-not (Test-Path -LiteralPath $key)) { continue }
        $item = Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
        foreach ($property in @('Install Dir', 'InstallDir', 'GTA:SA Path', 'Path', 'GTA_SA_PATH')) {
            $value = $item.PSObject.Properties[$property]
            if ($value -and $value.Value -and (Test-Path -LiteralPath (Join-Path $value.Value 'samp.dll'))) {
                return $value.Value.TrimEnd('\')
            }
        }
    }

    $paths = @(
        'C:\Program Files (x86)\Rockstar Games\GTA San Andreas',
        'C:\Program Files\Rockstar Games\GTA San Andreas',
        'C:\Games\GTA San Andreas',
        'D:\Games\GTA San Andreas'
    )
    foreach ($path in $paths) {
        if (Test-Path -LiteralPath (Join-Path $path 'samp.dll')) { return $path }
    }
    return ''
}

function Get-PeSignature([string]$Path) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 512) { throw 'Файл слишком мал для PE-заголовка.' }

    $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
    if ($peOffset -le 0 -or $peOffset + 84 -ge $bytes.Length) { throw 'Некорректный PE-заголовок.' }

    $signature = [BitConverter]::ToUInt32($bytes, $peOffset)
    if ($signature -ne 0x00004550) { throw 'Это не PE-файл (нет подписи PE).' }

    $timestamp  = [BitConverter]::ToUInt32($bytes, $peOffset + 8)
    $sizeOfImage = [BitConverter]::ToUInt32($bytes, $peOffset + 80)

    return [pscustomobject]@{
        TimeDateStamp = $timestamp
        SizeOfImage   = $sizeOfImage
    }
}

Write-Host ''
Write-Host '  ============================================' -ForegroundColor DarkYellow
Write-Host '   RAILWAY — проверка клиента SA-MP' -ForegroundColor Yellow
Write-Host '  ============================================' -ForegroundColor DarkYellow
Write-Host ''

if ($SampDll -eq '') {
    if ($GtaPath -eq '') { $GtaPath = Find-GtaFolder }
    if ($GtaPath -eq '') {
        Write-Bad 'Папка GTA San Andreas не найдена.'
        Write-Note 'Укажите путь вручную: -GtaPath "C:\Games\GTA San Andreas"'
        exit 1
    }
    $SampDll = Join-Path $GtaPath 'samp.dll'
}

if (-not (Test-Path -LiteralPath $SampDll)) {
    Write-Bad "samp.dll не найден: $SampDll"
    Write-Note 'Сначала установите клиент SA-MP 0.3.7 R5-2 (sa-mp.mp) в папку с GTA.'
    exit 1
}

$gameDir = Split-Path -Parent $SampDll
Write-Host "  Папка игры: $gameDir" -ForegroundColor Gray
Write-Host "  samp.dll:   $((Get-Item -LiteralPath $SampDll).Length) байт" -ForegroundColor Gray
Write-Host ''

$known = @(
    [pscustomobject]@{ Timestamp = 0x5542F47A; Size = 0x330000; Version = '0.3.7-R1' },
    [pscustomobject]@{ Timestamp = 0x5C0B4243; Size = 0x27E000; Version = '0.3.7-R3-1' },
    [pscustomobject]@{ Timestamp = 0x6372C39E; Size = 0x27E000; Version = '0.3.7-R5-1' },
    [pscustomobject]@{ Timestamp = 0x5A6A3130; Size = 0x2BE000; Version = '0.3.DL-R1' }
)

try {
    $pe = Get-PeSignature $SampDll
} catch {
    Write-Bad "Не удалось разобрать PE-заголовок: $($_.Exception.Message)"
    exit 1
}

Write-Host ('  PE-подпись: TimeDateStamp = 0x{0:X8}, SizeOfImage = 0x{1:X}' -f $pe.TimeDateStamp, $pe.SizeOfImage) -ForegroundColor Gray

$match = $known | Where-Object { $_.Timestamp -eq $pe.TimeDateStamp -and $_.Size -eq $pe.SizeOfImage } | Select-Object -First 1

$fileVersion = ''
try {
    $info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($SampDll)
    $fileVersion = $info.FileVersion
} catch { }

Write-Host ''
if ($match) {
    Write-Ok "Билд распознан по PE-подписи: $($match.Version)"
    Write-Note 'CEF-плагин v1.4.0 поддерживает этот билд напрямую.'
} else {
    Write-Warn2 'PE-подпись не совпала ни с одним известным билдом.'
    if ($fileVersion) {
        Write-Note "Версия файла: $fileVersion"

        $major = 0; $minor = 0; $patch = 0; $revision = 0
        $parts = ($fileVersion -split '[^\d]+') | Where-Object { $_ -ne '' }
        if ($parts.Count -ge 4) {
            $major = [int]$parts[0]; $minor = [int]$parts[1]
            $patch = [int]$parts[2]; $revision = [int]$parts[3]
        }

        if ($major -eq 0 -and $minor -eq 3 -and $patch -eq 7 -and $revision -eq 5) {
            Write-Ok 'Версия 0.3.7.5 → SA-MP 0.3.7 R5-2 / R5-1.'
            Write-Note 'CEF-плагин определит его по версии файла как 0.3.7-R5'
            Write-Note 'и использует адреса R5-1 — они в R5-2 не менялись.'
        } elseif ($major -eq 0 -and $minor -eq 3 -and $patch -eq 7 -and $revision -eq 2) {
            Write-Ok 'Версия 0.3.7.2 → SA-MP 0.3.7 R3-1.'
        } elseif ($major -eq 0 -and $minor -eq 3 -and $patch -eq 8) {
            Write-Ok 'Версия 0.3.8 → SA-MP 0.3.DL.'
        } else {
            Write-Bad "Неизвестная версия samp.dll: $fileVersion"
            Write-Note 'CEF-плагин не сможет определить билд — интерфейс не заработает.'
            Write-Note 'Установите чистый клиент SA-MP 0.3.7 R5-2 с sa-mp.mp.'
        }
    } else {
        Write-Bad 'Версия файла недоступна — определить билд не удалось.'
    }
}

Write-Host ''
Write-Host '  Компоненты CEF:' -ForegroundColor Yellow

$checks = @(
    @{ Path = 'cef.asi';            Name = 'cef.asi (ASI-плагин)' },
    @{ Path = 'cef\client.dll';     Name = 'cef\client.dll' },
    @{ Path = 'cef\renderer.exe';   Name = 'cef\renderer.exe' },
    @{ Path = 'cef\libcef.dll';     Name = 'cef\libcef.dll' }
)

foreach ($item in $checks) {
    $full = Join-Path $gameDir $item.Path
    if (Test-Path -LiteralPath $full) {
        Write-Ok $item.Name
    } else {
        Write-Bad "нет: $($item.Name)"
    }
}

$loaders = @('dinput8.dll', 'vorbisFile.dll', 'dsound.dll', 'version.dll') |
    Where-Object { Test-Path -LiteralPath (Join-Path $gameDir $_) }

if ($loaders.Count -gt 0) {
    Write-Ok "ASI loader: $($loaders -join ', ')"
} else {
    Write-Bad 'ASI loader не найден (нужен dinput8.dll от Silent''s ASI Loader).'
    Write-Note 'Без него cef.asi не загрузится и CEF-интерфейс не появится.'
}

Write-Host ''
Write-Host '  Готово.' -ForegroundColor Green
Write-Host '  Если всё в порядке — заходите на сервер: интерфейс входа появится сам.' -ForegroundColor Gray
Write-Host ''
