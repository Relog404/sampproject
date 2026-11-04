@echo off
setlocal
chcp 866 >nul
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%PS%" set "PS=powershell.exe"
"%PS%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0check-plugin.ps1" %*
echo.
echo   Нажмите любую клавишу, чтобы закрыть окно...
pause >nul
