#Requires -Version 5.1
<# Native SA-MP setup: no client mods, browser runtime or downloads. #>
[CmdletBinding()]
param([switch]$Build)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
foreach ($file in @('server\samp-server.exe', 'server\server.cfg', 'server\gamemodes\railway.amx', 'server\filterscripts\admin_console.amx')) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $file))) { throw "Missing file: $file" }
}
New-Item -ItemType Directory -Path (Join-Path $root 'server\scriptfiles') -Force | Out-Null
if ($Build) { & (Join-Path $root 'server\tools\build-gamemode.ps1') }
Write-Host 'Native TextDraw mode ready. No client modifications or plugins required.' -ForegroundColor Green
Write-Host 'Change rcon_password in server\server.cfg, then start server\samp-server.exe.'
Write-Host 'Existing scriptfiles\railway.db must be preserved. Default controls: Y menu, N engine, H doors.'
