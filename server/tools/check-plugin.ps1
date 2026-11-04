#Requires -Version 5.1
<#
    Railway — проверка серверного плагина CEF.

    Отвечает на вопрос «почему в server_log.txt написано
    "Loading plugin: cef" → "Failed."».

    Что делает скрипт:
      1. проверяет, что server\plugins\cef.dll на месте, и считает SHA-256;
      2. читает PE-заголовок: MZ/PE, разрядность (нужна 32-битная), дату сборки;
      3. ищет в файле имена экспортов Supports / Load / Unload / AmxLoad;
      4. реально загружает DLL через LoadLibrary и показывает код ошибки
         Windows, если загрузка не удалась (на 64-битной системе проверка
         автоматически перезапускается в 32-битном PowerShell).

    Запуск:
      server\tools\check-plugin.cmd            (двойной клик)
      powershell -ExecutionPolicy Bypass -File server\tools\check-plugin.ps1
      powershell ... -File ...\check-plugin.ps1 -Path "D:\srv\plugins\cef.dll"
#>
[CmdletBinding()]
param(
    [string]$Path = '',
    [switch]$InWow64
)

$ErrorActionPreference = 'Stop'

function Write-Ok([string]$Text)   { Write-Host "  [ok] $Text" -ForegroundColor Green }
function Write-Warn2([string]$Text){ Write-Host "  [!]  $Text" -ForegroundColor Yellow }
function Write-Bad([string]$Text)  { Write-Host "  [!!] $Text" -ForegroundColor Red }
function Write-Note([string]$Text) { Write-Host "  $Text" -ForegroundColor Gray }

$toolsDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$serverDir = Split-Path -Parent $toolsDir
if ($Path -eq '') {
    $Path = Join-Path $serverDir 'plugins\cef.dll'
}

Write-Host ''
Write-Host '  ============================================' -ForegroundColor DarkYellow
Write-Host '   RAILWAY — проверка плагина cef.dll' -ForegroundColor Yellow
Write-Host '  ============================================' -ForegroundColor DarkYellow
Write-Host ''
Write-Note ("Файл: {0}" -f $Path)
Write-Note ("PowerShell: {0}-битный" -f ([IntPtr]::Size * 8))
Write-Host ''

# ------------------------------------------------------------------ 1. файл
if (-not (Test-Path -LiteralPath $Path)) {
    Write-Bad 'Плагин не найден: файла нет.'
    Write-Host ''
    Write-Note 'Именно это и даёт в логе строки:'
    Write-Note '    Loading plugin: cef'
    Write-Note '     Failed.'
    Write-Note 'и следом "Script[gamemodes/railway.amx]: Run time error 19".'
    Write-Host ''
    Write-Note 'Что сделать: из корня проекта выполнить'
    Write-Host '    powershell -ExecutionPolicy Bypass -File .\setup.ps1' -ForegroundColor Cyan
    Write-Note 'или скачать cef-samp-v1.4.0.zip вручную (см. server\plugins\README.txt)'
    Write-Note 'и положить из архива cef.dll в server\plugins\.'
    Write-Host ''
    exit 2
}

$file = Get-Item -LiteralPath $Path
Write-Ok ("Найден: {0:N0} байт, изменён {1}" -f $file.Length, $file.LastWriteTime)
if ($file.Length -lt 200KB) {
    Write-Warn2 'Размер подозрительно мал: похоже, скачался не тот файл или обрезанный архив.'
}
$hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLower()
Write-Note ("SHA-256: {0}" -f $hash)

# частая ошибка: в папку плагинов кладут клиентские файлы
$pluginsDir = Split-Path -Parent $Path
foreach ($wrong in @('cef.asi', 'cef.so', 'client.dll', 'renderer.exe')) {
    $candidate = Join-Path $pluginsDir $wrong
    if (Test-Path -LiteralPath $candidate) {
        Write-Warn2 ("В папке плагинов лежит {0} — это клиентский файл, серверу он не нужен." -f $wrong)
    }
}
Write-Host ''

# ------------------------------------------------------------------ 2. PE
$bytes = [System.IO.File]::ReadAllBytes($Path)
$machine = 0
$stamp = 0
try {
    if ($bytes.Length -lt 0x100) { throw 'файл слишком короткий' }
    if ($bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) { throw 'нет сигнатуры MZ — это не DLL' }
    $pe = [System.BitConverter]::ToInt32($bytes, 0x3C)
    if ($pe -le 0 -or $pe + 24 -ge $bytes.Length) { throw 'некорректный e_lfanew' }
    $sig = [System.BitConverter]::ToUInt32($bytes, $pe)
    if ($sig -ne 0x00004550) { throw 'нет PE-подписи' }
    $machine = [System.BitConverter]::ToUInt16($bytes, $pe + 4)
    $stamp   = [System.BitConverter]::ToInt32($bytes, $pe + 8)
} catch {
    Write-Bad ("Файл не похож на DLL: {0}" -f $_.Exception.Message)
    Write-Note 'Перекачайте архив: setup.ps1 -Force'
    Write-Host ''
    exit 4
}

$machineId = [int]$machine
$machines = @{ 332 = 'x86 (32-бит)'; 0x8664 = 'x64 (64-бит)'; 0x0200 = 'Itanium'; 0x01C4 = 'ARM' }
$machineName = $machines[$machineId]
if (-not $machineName) { $machineName = ("неизвестная (0x{0:X4})" -f $machine) }
$built = (Get-Date '1970-01-01').ToLocalTime().AddSeconds([int64]$stamp)

if ($machineId -eq 332) {
    Write-Ok ("Разрядность: {0} — подходит для samp-server.exe" -f $machineName)
} else {
    Write-Bad ("Разрядность: {0} — нужна 32-битная DLL (x86)" -f $machineName)
    Write-Note 'SA-MP-сервер 32-битный и не загрузит 64-битный плагин.'
    Write-Note 'Возьмите cef-samp-v1.4.0.zip (это и есть x86-сборка).'
}
Write-Note ("Дата сборки (PE TimeDateStamp): {0}" -f $built)

# ------------------------------------------------------------------ 3. экспорты
$ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
$required = @('Supports', 'Load', 'Unload', 'AmxLoad')
$missing = @()
foreach ($name in $required) {
    if (-not $ascii.Contains($name)) { $missing += $name }
}
if ($missing.Count -eq 0) {
    Write-Ok 'Имена экспортов Supports/Load/Unload/AmxLoad в файле есть.'
} else {
    Write-Bad ("Не найдены экспорты: {0}" -f ($missing -join ', '))
    Write-Note 'Это не плагин SA-MP (возможно, скачан клиентский client.dll).'
}
Write-Host ''

# ------------------------------------------------------------------ 4. LoadLibrary
if ([IntPtr]::Size -eq 8 -and -not $InWow64) {
    $wow = Join-Path $env:WINDIR 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
    if (Test-Path -LiteralPath $wow) {
        Write-Note 'Перезапускаю проверку в отдельном 32-битном PowerShell —'
        Write-Note 'он попробует реально загрузить DLL (текущее окно не пострадает).'
        Write-Host ''
        & $wow -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Path $Path -InWow64
        $childCode = $LASTEXITCODE
        if ($childCode -ne 0 -and ($childCode -lt 0 -or $childCode -gt 1000)) {
            Write-Host ''
            Write-Bad ("Процесс проверки упал (код {0})." -f $childCode)
            Write-Note 'Скорее всего, cef.dll падает уже внутри DllMain —'
            Write-Note 'так Windows ведёт себя с повреждённым или чужим бинарником.'
            Write-Note 'Перекачайте архив: setup.ps1 -Force'
        }
        exit $childCode
    }
    Write-Warn2 '32-битный PowerShell не найден — тест загрузки пропущен.'
    Write-Host ''
    exit 0
}

Add-Type -Namespace RailwayCef -Name K32 -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
public static extern IntPtr LoadLibraryW(string lpFileName);

[DllImport("kernel32.dll", SetLastError = true)]
public static extern bool FreeLibrary(IntPtr hModule);

[DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi)]
public static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);
'@

$handle = [RailwayCef.K32]::LoadLibraryW($Path)
if ($handle -eq [IntPtr]::Zero) {
    $code = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    Write-Bad ("LoadLibrary не смогла загрузить DLL. Код ошибки Windows: {0}" -f $code)
    Write-Host ''
    switch ($code) {
        126 {
            Write-Note 'ERROR_MOD_NOT_FOUND — не найдена зависимость.'
            Write-Note 'Рядом с cef.dll должен лежать только он сам (сборка со статическим'
            Write-Note 'рантаймом), поэтому чаще всего файл просто битый: перекачайте'
            Write-Note 'setup.ps1 -Force.'
        }
        127 {
            Write-Note 'ERROR_PROC_NOT_FOUND — в системе нет функции, которую требует DLL.'
            Write-Note 'Обычно это старая Windows (7 без обновлений) или перепакованный файл.'
            Write-Note 'Обновите Windows либо возьмите оригинальный архив v1.4.0.'
        }
        193 {
            Write-Note 'ERROR_BAD_EXE_FORMAT — не та разрядность (DLL 64-битная).'
            Write-Note 'Нужен именно cef-samp-v1.4.0.zip (x86).'
        }
        1114 {
            Write-Note 'ERROR_DLL_INIT_FAILED — упал DllMain.'
            Write-Note 'Проверьте, что файл не тронут антивирусом; перекачайте архив.'
        }
        14001 {
            Write-Note 'ERROR_SXS_CANT_GEN_ACTCTX — нет нужного рантайма Visual C++.'
            Write-Note 'Поставьте «Visual C++ Redistributable 2015-2022 (x86)».'
        }
        default {
            Write-Note 'Расшифровки под этот код нет — пришлите его в ответ, разберёмся.'
        }
    }
    Write-Host ''
    exit 4
}

Write-Ok 'LoadLibrary загрузила cef.dll — зависимости в порядке.'

$procs = @('Supports', 'Load', 'Unload', 'AmxLoad', 'AmxUnload')
$notFound = @()
foreach ($name in $procs) {
    $addr = [RailwayCef.K32]::GetProcAddress($handle, $name)
    if ($addr -eq [IntPtr]::Zero) { $notFound += $name }
}
[void][RailwayCef.K32]::FreeLibrary($handle)

if ($notFound.Count -eq 0) {
    Write-Ok 'Экспорты на месте: Supports, Load, Unload, AmxLoad, AmxUnload.'
} else {
    Write-Bad ("DLL загрузилась, но не экспортирует: {0}" -f ($notFound -join ', '))
    Write-Note 'Это не серверный плагин SA-MP.'
}

# ------------------------------------------------------------------ 5. server.cfg
Write-Host ''
Write-Host '  server.cfg' -ForegroundColor Cyan

$cfgPath = Join-Path $serverDir 'server.cfg'
if (-not (Test-Path -LiteralPath $cfgPath)) {
    Write-Bad ("Не найден {0}" -f $cfgPath)
} else {
    $cfg = Get-Content -LiteralPath $cfgPath -Encoding Default
    $pluginLine = $cfg | Where-Object { $_ -match '^\s*plugins\s+' } | Select-Object -First 1
    if (-not $pluginLine) {
        Write-Bad 'В server.cfg нет строки "plugins ..."'
        Write-Note 'Добавьте:  plugins cef'
        Write-Note 'Без неё сервер вообще не попытается загрузить плагин.'
    } else {
        $value = ($pluginLine -replace '^\s*plugins\s+', '').Trim()
        if ($value -match '\.dll$') {
            Write-Warn2 ("Строка: plugins {0} — расширение .dll писать не нужно" -f $value)
            Write-Note 'SA-MP сам добавит .dll; правильно:  plugins cef'
        } elseif ($value -notmatch '(^|\s)cef(\s|$)') {
            Write-Bad ("Строка: plugins {0} — плагина cef в ней нет" -f $value)
            Write-Note 'Правильно:  plugins cef'
        } else {
            Write-Ok ("Строка: plugins {0}" -f $value)
        }
    }

    $gamemodeLine = $cfg | Where-Object { $_ -match '^\s*gamemode\d*\s+' } | Select-Object -First 1
    if ($gamemodeLine) {
        Write-Ok ("Строка: {0}" -f $gamemodeLine.Trim())
    }
    if ($cfg -match 'rcon_password\s+change_me_now') {
        Write-Warn2 'rcon_password всё ещё change_me_now — смените перед выкладкой!'
    }
}

$amxPath = Join-Path $serverDir 'gamemodes\railway.amx'
if (Test-Path -LiteralPath $amxPath) {
    $amxLen = (Get-Item -LiteralPath $amxPath).Length
    Write-Ok ("gamemodes\railway.amx: {0:N0} байт" -f $amxLen)
} else {
    Write-Bad 'Нет server\gamemodes\railway.amx — гейммод не скомпилирован.'
}

Write-Host ''
Write-Host '  Итог' -ForegroundColor Yellow
Write-Note 'Если все пункты выше [ok], а сервер всё равно пишет "Failed.",'
Write-Note 'проверьте в server.cfg строку "plugins cef" (без .dll) и что'
Write-Note 'запускается именно server\samp-server.exe, а файл лежит в server\plugins\.'
Write-Host ''
exit 0
