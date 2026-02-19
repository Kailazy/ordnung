@echo off
setlocal

:: ── Config ─────────────────────────────────────────────────────────────────
:: Qt MSVC install path — update version number to match your install
set QT_PATH=C:\Qt\6.7.2\msvc2022_64

:: Source: reads directly from WSL — no copy needed.
:: If builds are slow, see README for the junction point alternative.
set SOURCE=\\wsl$\Ubuntu\home\kailazy\projects\ordnung\ordnung-qt

:: Windows-native build output (separate from the Linux build dir in WSL)
set BUILD=C:\projects\ordnung-build

:: ───────────────────────────────────────────────────────────────────────────

if not exist "%QT_PATH%" (
    echo ERROR: Qt not found at %QT_PATH%
    echo Edit QT_PATH in this file to match your Qt install location.
    pause
    exit /b 1
)

cmake -B "%BUILD%" -S "%SOURCE%" ^
      -G "Visual Studio 17 2022" -A x64 ^
      -DCMAKE_PREFIX_PATH="%QT_PATH%"

cmake --build "%BUILD%" --config Debug -j %NUMBER_OF_PROCESSORS%

if errorlevel 1 (
    echo.
    echo Build failed.
    pause
    exit /b 1
)

"%BUILD%\Debug\Ordnung.exe"
