#Requires -Version 5.1
<#
    Railway — установка клиентской части CEF в папку GTA San Andreas.
    ------------------------------------------------------------------
    Кладёт в игру:  cef.asi  и  папку  cef\  (client.dll, renderer.exe,
    libcef.dll, locales, resources ...).

    Источник: официальный релиз aurora-mp/omp-cef v1.4.0 (Windows x86),
    файл client-files-v1.4.0.zip, SHA-256 проверяется.

    Примеры:
      powershell -ExecutionPolicy Bypass -File install-client.ps1
      powershell -ExecutionPolicy Bypass -File install-client.ps1 -GtaPath "D:\Games\GTA SA"
      powershell -ExecutionPolicy Bypass -File install-client.ps1 -DownloadOnly
#>
[CmdletBinding()]
param(
    [string]$GtaPath = '',
    [switch]$DownloadOnly,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch { }

$ClientDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root      = Split-Path -Parent $ClientDir
$CacheDir  = Join-Path $Root '.cache'
$Payload   = Join-Path $ClientDir 'gta-files'

$Asset = [pscustomobject]@{
    Title  = 'CEF client files v1.4.0 (Windows x86, ~146 МБ)'
    Url    = 'https://github.com/aurora-mp/omp-cef/releases/download/v1.4.0/client-files-v1.4.0.zip'
    File   = 'client-files-v1.4.0.zip'
    Sha256 = '9bdcf634a6b473e648716deaa3779f55ac1211924ee9c55013897582cf34e55f'
    Size   = 153550522
}

function Write-Ok([string]$Text)    { Write-Host "  [ok] $Text" -ForegroundColor Green }
function Write-Warn2([string]$Text) { Write-Host "  [!]  $Text" -ForegroundColor Yellow }
function Write-Note([string]$Text)  { Write-Host "  $Text" -ForegroundColor Gray }

function Assert-FileHash([string]$Path, [string]$Expected, [long]$Size) {
    $info = Get-Item -LiteralPath $Path
    if ($Size -gt 0 -and $info.Length -ne $Size) {
        throw "Размер файла не совпадает: $($info.Length) вместо $Size байт."
    }
    $hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLower()
    if ($hash -ne $Expected.ToLower()) {
        throw "SHA-256 не совпадает.`n  ожидалось: $Expected`n  получено:  $hash"
    }
}

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
            if ($value -and $value.Value -and (Test-Path -LiteralPath $value.Value)) {
                $candidate = $value.Value.TrimEnd('\')
                if (Test-Path -LiteralPath (Join-Path $candidate 'gta_sa.exe')) { return $candidate }
            }
        }
    }

    $paths = @(
        'C:\Program Files (x86)\Rockstar Games\GTA San Andreas',
        'C:\Program Files\Rockstar Games\GTA San Andreas',
        'C:\Program Files (x86)\GTA San Andreas',
        'C:\Games\GTA San Andreas',
        'D:\Games\GTA San Andreas',
        'D:\GTA San Andreas'
    )

    foreach ($path in $paths) {
        if (Test-Path -LiteralPath (Join-Path $path 'gta_sa.exe')) { return $path }
    }
    return ''
}

function Get-Payload([pscustomobject]$Item) {
    if (-not (Test-Path -LiteralPath $CacheDir)) {
        New-Item -ItemType Directory -Path $CacheDir | Out-Null
    }

    $zipPath = Join-Path $CacheDir $Item.File

    if ((Test-Path -LiteralPath $zipPath) -and -not $Force) {
        try {
            Assert-FileHash $zipPath $Item.Sha256 $Item.Size
            Write-Ok "Архив уже скачан: $($Item.File)"
            return $zipPath
        } catch {
            Write-Warn2 'Кэш повреждён, качаем заново.'
            Remove-Item -LiteralPath $zipPath -Force
        }
    }

    Write-Note "Скачивание: $($Item.Title)"
    Write-Note $Item.Url

    $progress = $ProgressPreference
    $ProgressPreference = 'Continue'
    try {
        Invoke-WebRequest -Uri $Item.Url -OutFile $zipPath -UseBasicParsing -Headers @{ 'User-Agent' = 'railway-setup' }
    } finally {
        $ProgressPreference = $progress
    }

    Assert-FileHash $zipPath $Item.Sha256 $Item.Size
    Write-Ok 'Архив скачан, SHA-256 совпал.'
    return $zipPath
}

Write-Host ''
Write-Host '  ============================================' -ForegroundColor DarkYellow
Write-Host '   RAILWAY — клиент CEF для GTA San Andreas' -ForegroundColor Yellow
Write-Host '   aurora-mp/omp-cef v1.4.0' -ForegroundColor DarkGray
Write-Host '  ============================================' -ForegroundColor DarkYellow
Write-Host ''

$zipPath = Get-Payload $Asset

# --------------------------------------------------------- распаковка в client\gta-files
if ($DownloadOnly -or $GtaPath -eq '') {
    $target = $Payload
    Write-Note "Распаковка в: $target"
} else {
    $target = $GtaPath
}

if (-not (Test-Path -LiteralPath $target)) {
    New-Item -ItemType Directory -Path $target | Out-Null
}

Expand-Archive -LiteralPath $zipPath -DestinationPath $target -Force
Write-Ok "Файлы распакованы: $target"

if ($DownloadOnly) {
    Write-Host ''
    Write-Host '  Режим -DownloadOnly: файлы лежат в client\gta-files.' -ForegroundColor Gray
    Write-Host '  Скопируйте содержимое этой папки в корень GTA San Andreas.' -ForegroundColor Gray
    Write-Host ''
    return
}

if ($GtaPath -eq '') {
    $target = $Payload
    Write-Warn2 'Папка GTA не указана — файлы лежат в client\gta-files.'
    Write-Note 'Скопируйте cef.asi и папку cef\ в корень GTA San Andreas.'
    Write-Host ''
    return
}

# --------------------------------------------------------- проверки установки
$asiPath = Join-Path $target 'cef.asi'
$cefPath = Join-Path $target 'cef\client.dll'

if (-not (Test-Path -LiteralPath $asiPath)) { Write-Warn2 'cef.asi не найден в папке игры.' }
if (-not (Test-Path -LiteralPath $cefPath)) { Write-Warn2 'cef\client.dll не найден в папке игры.' }

if (-not (Test-Path -LiteralPath (Join-Path $target 'gta_sa.exe'))) {
    Write-Warn2 'В выбранной папке нет gta_sa.exe — проверьте путь.'
}
if (-not (Test-Path -LiteralPath (Join-Path $target 'samp.dll'))) {
    Write-Warn2 'В папке нет samp.dll — сначала установите клиент SA-MP 0.3.7 R5-2.'
}

# ASI loader — без него cef.asi не загрузится
$loaders = @('dinput8.dll', 'vorbisFile.dll', 'dsound.dll', 'version.dll')
$found = $loaders | Where-Object { Test-Path -LiteralPath (Join-Path $target $_) }

if ($found.Count -eq 0) {
    Write-Host ''
    Write-Warn2 'Не найден ASI loader (dinput8.dll / vorbisFile.dll).'
    Write-Note 'cef.asi — это ASI-плагин: без загрузчика он не запустится.'
    Write-Note 'Поставьте Silent''s ASI Loader (dinput8.dll) в корень игры и запустите проверку снова:'
    Write-Note '  powershell -ExecutionPolicy Bypass -File check-samp-build.ps1'
} else {
    Write-Ok "ASI loader найден: $($found -join ', ')"
}

Write-Host ''
Write-Host '  Готово.' -ForegroundColor Green
Write-Host '  Запускайте SA-MP 0.3.7 R5-2 и подключайтесь к серверу.' -ForegroundColor Gray
Write-Host ''
