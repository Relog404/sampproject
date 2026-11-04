#Requires -Version 5.1
<#
    Пересборка гейммода railway.amx компилятором из пакета SA-MP
    (server\pawno\pawncc.exe, Pawn 3.2.3664 — пишет AMX версии 8).
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$serverDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$pawncc    = Join-Path $serverDir 'pawno\pawncc.exe'
$include   = Join-Path $serverDir 'pawno\include'
$gamemodes = Join-Path $serverDir 'gamemodes'

if (-not (Test-Path -LiteralPath $pawncc)) {
    throw "Не найден компилятор: $pawncc"
}

Write-Host "  Компилятор: $pawncc" -ForegroundColor Gray
Write-Host "  Исходник:   $gamemodes\railway.pwn" -ForegroundColor Gray
Write-Host ''

Push-Location -LiteralPath $gamemodes
try {
    & $pawncc 'railway.pwn' "-i$include" '-orailway.amx' '-d3'
    if ($LASTEXITCODE -ne 0) {
        throw "Компиляция завершилась с кодом $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

$amx = Join-Path $gamemodes 'railway.amx'
if (-not (Test-Path -LiteralPath $amx)) {
    throw 'railway.amx не создан.'
}

$size = (Get-Item -LiteralPath $amx).Length
$bytes = [System.IO.File]::ReadAllBytes($amx)
$magic = [System.BitConverter]::ToUInt16($bytes, 4)
$fileVersion = [int]$bytes[6]
$cip = [System.BitConverter]::ToInt32($bytes, 28)

Write-Host ''
Write-Host "  railway.amx: $size байт, файловая версия $fileVersion, magic 0x$('{0:X4}' -f $magic)" -ForegroundColor Gray

if ($fileVersion -ne 8) {
    throw "AMX версии $fileVersion — сервер 0.3.7 читает только версию 8. Соберите с -O0 -d1."
}
if ($cip -lt 0) {
    throw 'В railway.amx нет точки входа main(): сервер напишет "Run time error 20: Invalid index parameter (bad entry point)". Добавьте main() { } в railway.pwn и пересоберите.'
}

Write-Host '  Точка входа main() на месте (cip != -1).' -ForegroundColor Green
Write-Host "  Готово: railway.amx ($size байт)" -ForegroundColor Green
