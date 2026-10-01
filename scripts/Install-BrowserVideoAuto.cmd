@echo off
setlocal
pwsh.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-BrowserVideoAuto.ps1" -OpenExtensionsPage
pause
