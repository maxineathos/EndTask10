@echo off
setlocal enabledelayedexpansion
title EndTask10 Setup

set "APP_DIR=%LOCALAPPDATA%\EndTask10"
set "EXE_NAME=EndTask10Launcher.exe"
set "DLL_NAME=EndTask10Hook.dll"

echo =====================================
echo   EndTask10 - One-Click Setup
echo   Ctrl+Shift+E to End Task
echo =====================================
echo.

:: Detect paths
set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR="

if exist "%SCRIPT_DIR%build\bin\Release\%EXE_NAME%" (
    set "BUILD_DIR=%SCRIPT_DIR%build\bin\Release"
) else if exist "%SCRIPT_DIR%%EXE_NAME%" (
    set "BUILD_DIR=%SCRIPT_DIR%"
) else if exist ".\build\bin\Release\%EXE_NAME%" (
    set "BUILD_DIR=.\build\bin\Release"
) else (
    echo [ERROR] Build output not found.
    echo         Run build.bat first or place built files in:
    echo         %SCRIPT_DIR%build\bin\Release\
    pause
    exit /b 1
)

echo Found build at: %BUILD_DIR%

:: Create app directory
if not exist "%APP_DIR%" mkdir "%APP_DIR%"

:: Copy files
copy /Y "%BUILD_DIR%\%EXE_NAME%" "%APP_DIR%\" >nul && echo [OK] Copied %EXE_NAME% || echo [FAIL] %EXE_NAME%
copy /Y "%BUILD_DIR%\%DLL_NAME%" "%APP_DIR%\" >nul && echo [OK] Copied %DLL_NAME% || echo [FAIL] %DLL_NAME%

:: Add to HKCU Run (auto-inject on login)
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run" ^
    /v "EndTask10" ^
    /t REG_SZ ^
    /d "\"%APP_DIR%\%EXE_NAME%\"" ^
    /f >nul 2>&1 && echo [OK] Added to startup

:: Inject immediately
echo.
echo Injecting into explorer.exe...
"%APP_DIR%\%EXE_NAME%"

echo.
echo =====================================
echo   Ready! Usage:
echo   1. Right-click a running app on taskbar
echo   2. Press Ctrl+Shift+E to end the task
echo.
echo   To uninstall, run uninstall.bat
echo =====================================
echo.
pause
