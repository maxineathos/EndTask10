@echo off
setlocal enabledelayedexpansion
title EndTask10 Uninstall

echo =====================================
echo   EndTask10 - Uninstall
echo =====================================
echo.

set "APP_DIR=%LOCALAPPDATA%\EndTask10"

:: Remove from startup
reg delete "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run" /v "EndTask10" /f >nul 2>&1
echo [OK] Removed from startup

:: Remove files
if exist "%APP_DIR%" (
    rmdir /s /q "%APP_DIR%" >nul 2>&1
    echo [OK] Removed %APP_DIR%
)

:: Restart explorer to unload injected DLL
echo.
set /p "RESTART=Restart explorer.exe to unload the DLL? (Y/N): "
if /i "!RESTART!"=="Y" (
    taskkill /f /im explorer.exe >nul 2>&1
    echo [OK] Explorer restarted - DLL unloaded
)

echo.
echo =====================================
echo   Uninstall complete.
echo =====================================
echo.
pause
