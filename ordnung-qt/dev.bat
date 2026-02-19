@echo off
setlocal

:: ── Set this to your Qt MSVC install path ──────────────────────────────────
:: Example: C:\Qt\6.7.2\msvc2022_64
set QT_PATH=C:\Qt\6.7.2\msvc2022_64

:: ───────────────────────────────────────────────────────────────────────────

if not exist "%QT_PATH%" (
    echo ERROR: Qt not found at %QT_PATH%
    echo Edit QT_PATH in this file to match your Qt install location.
    pause
    exit /b 1
)

cmake -B build -G "Visual Studio 17 2022" -A x64 ^
      -DCMAKE_PREFIX_PATH="%QT_PATH%" ^
      -DCMAKE_BUILD_TYPE=Debug

cmake --build build --config Debug

if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

build\Debug\Ordnung.exe
