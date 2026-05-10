@echo off
setlocal
title EndTask10 Build

set "CMAKE="
for %%p in (
    "C:\Program Files (x86)\Microsoft Visual Studio\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    "C:\Program Files\CMake\bin\cmake.exe"
    "C:\Program Files (x86)\CMake\bin\cmake.exe"
) do if exist %%p set "CMAKE=%%~p"

if not defined CMAKE (
    where cmake >nul 2>&1 && set "CMAKE=cmake" || (
        echo [ERROR] CMake not found.
        pause & exit /b 1
    )
)

echo Using cmake: %CMAKE%
echo Configuring...
"%CMAKE%" -G "Visual Studio 18 2026" -A x64 -B build 2>&1 || "%CMAKE%" -G "Visual Studio 17 2022" -A x64 -B build 2>&1
if errorlevel 1 ( echo [ERROR] Configure failed. & pause & exit /b 1 )

echo Building...
"%CMAKE%" --build build --config Release 2>&1
if errorlevel 1 ( echo [ERROR] Build failed. & pause & exit /b 1 )

echo.
echo =====================================
echo   Build complete!
echo   Output: build\bin\Release\
echo.
echo   Run: build\bin\Release\EndTask10Launcher.exe
echo   Run: build\bin\Release\EndTask10Launcher.exe /unload
echo =====================================
echo.
pause
