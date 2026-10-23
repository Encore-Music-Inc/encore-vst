@echo off
rem Double-click to build, test and install all Sick & Sad plugins.
rem Close Cubase first. Click Yes on the Windows admin prompt.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-and-install.ps1"
echo.
pause
