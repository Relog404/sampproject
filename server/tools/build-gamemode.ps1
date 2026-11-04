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
if (Test-Path -LiteralPath $amx) {
    $size = (Get-Item -LiteralPath $amx).Length
    Write-Host ''
    Write-Host "  Готово: railway.amx ($size байт)" -ForegroundColor Green
} else {
    throw 'railway.amx не создан.'
}
