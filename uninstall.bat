@echo off
setlocal
title EndTask10 Uninstall

set "APP_DIR=%LOCALAPPDATA%\EndTask10"

echo =====================================
echo   EndTask10 - Uninstall
echo =====================================
echo.

:: Unload DLL
if exist "%APP_DIR%\EndTask10Launcher.exe" (
    "%APP_DIR%\EndTask10Launcher.exe" /unload
)

:: Remove startup entry
reg delete "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run" /v "EndTask10" /f >nul 2>&1
echo [OK] Removed from startup

:: Remove files
if exist "%APP_DIR%" (
    rmdir /s /q "%APP_DIR%" >nul 2>&1
    echo [OK] Removed %APP_DIR%
)

echo.
echo Uninstall complete.
echo.
pause
