@echo off
setlocal
pwsh.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0BrowserVideoRegionPicker.ps1"
if errorlevel 1 pause
endlocal
