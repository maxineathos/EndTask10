@echo off
setlocal enabledelayedexpansion
title EndTask10 Setup

:: Auto-elevate to admin (required to stop services that auto-restart apps)
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] Requesting administrator privileges...
    powershell -Command "Start-Process '%~dpnx0' -Verb RunAs -Wait" 2>nul
    exit /b 0
)

set "APP_DIR=%LOCALAPPDATA%\EndTask10"
set "BUILD_DIR=%~dp0build\bin\Release"

if not exist "%BUILD_DIR%\EndTask10Launcher.exe" (
    echo [ERROR] Build not found. Run build.bat first.
    pause & exit /b 1
)

echo =====================================
echo   EndTask10 - Setup
echo =====================================
echo.

:: Try to stop known services that auto-restart killed processes
echo [INFO] Stopping services that may auto-restart apps...
for %%s in (
    "Steam Client Service"
    "Steam Client Service64"
    "Epic Online Services"
    "EpicGamesLauncher"
) do (
    sc stop "%%~s" >nul 2>&1 && echo [OK] Stopped: %%~s || echo [---] Not running: %%~s
)

:: Try unload first (for new DLLs that support it)
"%APP_DIR%\EndTask10Launcher.exe" /unload >nul 2>&1

:: Create app dir
if not exist "%APP_DIR%" mkdir "%APP_DIR%"

:: Copy exe
copy /Y "%BUILD_DIR%\EndTask10Launcher.exe" "%APP_DIR%\" >nul
if errorlevel 1 ( echo [FAIL] exe copy & pause & exit /b 1 )
echo [OK] Copied EndTask10Launcher.exe

:: Copy DLL (retry if locked)
copy /Y "%BUILD_DIR%\EndTask10Hook.dll" "%APP_DIR%\" >nul 2>&1
if errorlevel 1 (
    echo [WARN] DLL in use by explorer. Trying again after unload...
    "%APP_DIR%\EndTask10Launcher.exe" /unload >nul 2>&1
    ping -n 3 127.0.0.1 >nul
    copy /Y "%BUILD_DIR%\EndTask10Hook.dll" "%APP_DIR%\" >nul 2>&1
    if errorlevel 1 (
        echo [FAIL] Copy later manually: %BUILD_DIR%\EndTask10Hook.dll -^> %APP_DIR%\
        echo        Or just use the build directory directly.
        goto :reg
    )
)
echo [OK] Copied EndTask10Hook.dll

:reg
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run" ^
    /v "EndTask10" /t REG_SZ ^
    /d "\"%APP_DIR%\EndTask10Launcher.exe\"" /f >nul 2>&1
echo [OK] Added to startup

echo.
echo Injecting...
"%APP_DIR%\EndTask10Launcher.exe"

echo.
echo =====================================
echo   Ready! Right-click taskbar ^> Ctrl+Shift+E
echo.
echo   Note: Services stopped above prevent auto-restart for
echo   those apps (Steam, Epic, etc.) for this session.
echo.
echo   Unload: %APP_DIR%\EndTask10Launcher.exe /unload
echo =====================================
echo.
pause
