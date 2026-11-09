@echo off
chcp 1251 >nul
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-cef-omp.ps1"
pause
