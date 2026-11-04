#Requires -Version 5.1
<#
    Railway — «Железнодорожный вокзал»
    ------------------------------------------------------------------
    Установка CEF-компонентов для сервера.

    Что делает скрипт:
      1. Скачивает официальный серверный плагин CEF для SA-MP
         (aurora-mp/omp-cef v1.4.0) и проверяет SHA-256.
      2. Кладёт plugins\cef.dll и pawno\include\cef.inc в папку server.
      3. По желанию сразу ставит клиентскую часть в папку GTA
         (вызывает client\install-client.ps1).

    Примеры:
      powershell -ExecutionPolicy Bypass -File setup.ps1
      powershell -ExecutionPolicy Bypass -File setup.ps1 -GtaPath "C:\Games\GTA San Andreas"
      powershell -ExecutionPolicy Bypass -File setup.ps1 -ServerOnly
#>
[CmdletBinding()]
param(
    [string]$GtaPath = '',
    [switch]$ServerOnly,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch { }

$Root      = Split-Path -Parent $MyInvocation.MyCommand.Path
$ServerDir = Join-Path $Root 'server'
$ClientDir = Join-Path $Root 'client'
$CacheDir  = Join-Path $Root '.cache'

$Plugin = [pscustomobject]@{
    Title  = 'CEF server plugin (SA-MP) v1.4.0'
    Url    = 'https://github.com/aurora-mp/omp-cef/releases/download/v1.4.0/cef-samp-v1.4.0.zip'
    File   = 'cef-samp-v1.4.0.zip'
    Sha256 = '7bc5d05b086f90708659ef893f0b952eb9fe347497a07845485b5935a69c8d28'
    Size   = 1113197
}

function Write-Step([string]$Text) {
    Write-Host ''
    Write-Host "  $Text" -ForegroundColor Cyan
}

function Write-Ok([string]$Text) {
    Write-Host "  [ok] $Text" -ForegroundColor Green
}

function Write-Warn2([string]$Text) {
    Write-Host "  [!]  $Text" -ForegroundColor Yellow
}

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

function Get-RemoteFile([pscustomobject]$Asset) {
    if (-not (Test-Path -LiteralPath $CacheDir)) {
        New-Item -ItemType Directory -Path $CacheDir | Out-Null
    }

    $target = Join-Path $CacheDir $Asset.File

    if ((Test-Path -LiteralPath $target) -and -not $Force) {
        try {
            Assert-FileHash $target $Asset.Sha256 $Asset.Size
            Write-Ok "Найден в кэше: $($Asset.File)"
            return $target
        } catch {
            Write-Warn2 'Кэш повреждён, скачиваем заново.'
            Remove-Item -LiteralPath $target -Force
        }
    }

    Write-Host "  Скачивание: $($Asset.Title)" -ForegroundColor Gray
    Write-Host "  $($Asset.Url)" -ForegroundColor DarkGray

    $progress = $ProgressPreference
    $ProgressPreference = 'Continue'
    try {
        Invoke-WebRequest -Uri $Asset.Url -OutFile $target -UseBasicParsing -Headers @{ 'User-Agent' = 'railway-setup' }
    } finally {
        $ProgressPreference = $progress
    }

    Assert-FileHash $target $Asset.Sha256 $Asset.Size
    Write-Ok "Скачано и проверено: $($Asset.File)"
    return $target
}

Write-Host ''
Write-Host '  ============================================' -ForegroundColor DarkYellow
Write-Host '   RAILWAY — установка CEF' -ForegroundColor Yellow
Write-Host '   aurora-mp/omp-cef v1.4.0 (SA-MP)' -ForegroundColor DarkGray
Write-Host '  ============================================' -ForegroundColor DarkYellow

if (-not (Test-Path -LiteralPath $ServerDir)) {
    throw "Не найдена папка server рядом со скриптом: $ServerDir"
}

# ------------------------------------------------------------------ сервер
Write-Step '1/2  Серверный плагин CEF'

$zip = Get-RemoteFile $Plugin

$temp = Join-Path $CacheDir 'extract-plugin'
if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
New-Item -ItemType Directory -Path $temp | Out-Null
Expand-Archive -LiteralPath $zip -DestinationPath $temp -Force

$dll = Get-ChildItem -LiteralPath $temp -Recurse -Filter 'cef.dll' | Select-Object -First 1
$inc = Get-ChildItem -LiteralPath $temp -Recurse -Filter 'cef.inc' | Select-Object -First 1

if (-not $dll) { throw 'В архиве плагина не найден cef.dll.' }
if (-not $inc) { throw 'В архиве плагина не найден cef.inc.' }

$pluginsDir = Join-Path $ServerDir 'plugins'
$includeDir = Join-Path $ServerDir 'pawno\include'
if (-not (Test-Path -LiteralPath $pluginsDir)) { New-Item -ItemType Directory -Path $pluginsDir | Out-Null }
if (-not (Test-Path -LiteralPath $includeDir)) { New-Item -ItemType Directory -Path $includeDir | Out-Null }

Copy-Item -LiteralPath $dll.FullName -Destination (Join-Path $pluginsDir 'cef.dll') -Force
Copy-Item -LiteralPath $inc.FullName -Destination (Join-Path $includeDir 'cef.inc') -Force

Write-Ok 'server\plugins\cef.dll'
Write-Ok 'server\pawno\include\cef.inc'

Remove-Item -LiteralPath $temp -Recurse -Force

# ------------------------------------------------------------------ клиент
if (-not $ServerOnly) {
    Write-Step '2/2  Клиентская часть для GTA'

    $clientScript = Join-Path $ClientDir 'install-client.ps1'
    if (-not (Test-Path -LiteralPath $clientScript)) {
        Write-Warn2 "Не найден $clientScript — клиентскую часть установите вручную."
    } else {
        # не используем автоматическую переменную $args — только явные параметры
        $clientArgs = @{}
        if ($Force) { $clientArgs['Force'] = $true }
        if ($GtaPath -ne '') { $clientArgs['GtaPath'] = $GtaPath }
        & $clientScript @clientArgs
    }
}

# ------------------------------------------------------------------ итог
Write-Host ''
Write-Host '  Готово.' -ForegroundColor Green
Write-Host ''
Write-Host '  Дальше:' -ForegroundColor Yellow
Write-Host '    1) откройте server\server.cfg и смените rcon_password;'
Write-Host '    2) запустите server\samp-server.exe (порт 7777, CEF — UDP 7779);'
Write-Host '    3) если сервер в интернете — разрешите входящие UDP 7777 и 7779.'
Write-Host ''
Write-Host '  Подробная инструкция: docs\УСТАНОВКА.md' -ForegroundColor DarkGray
Write-Host ''
