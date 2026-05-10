@echo off
setlocal
title EndTask10 Build

echo =====================================
echo   EndTask10 - Build
echo =====================================
echo.

:: Find cmake
set "CMAKE="
for %%p in (
    "C:\Program Files (x86)\Microsoft Visual Studio\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    "C:\Program Files\CMake\bin\cmake.exe"
    "C:\Program Files (x86)\CMake\bin\cmake.exe"
) do (
    if exist %%p set "CMAKE=%%~p"
)

if not defined CMAKE (
    where cmake >nul 2>&1
    if !errorlevel! equ 0 (
        set "CMAKE=cmake"
    ) else (
        echo [ERROR] CMake not found. Please install CMake or add it to PATH.
        pause
        exit /b 1
    )
)

echo Using cmake: %CMAKE%

:: Configure
echo Configuring...
"%CMAKE%" -G "Visual Studio 18 2026" -A x64 -B build 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake configure failed.
    pause
    exit /b 1
)

:: Build
echo Building...
"%CMAKE%" --build build --config Release 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo.
echo =====================================
echo   Build complete!
echo   Output: build\bin\Release\
echo.
echo   Run setup.bat to install.
echo =====================================
echo.
pause
