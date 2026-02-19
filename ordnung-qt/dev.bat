@echo off
setlocal

:: ── Config ─────────────────────────────────────────────────────────────────
set QT_PATH=C:\Qt\6.10.2\mingw_64
set MINGW_PATH=C:\Qt\Tools\mingw1310_64
set NINJA_PATH=C:\Qt\Tools\Ninja

:: Source: reads directly from WSL — no copy needed.
:: If builds are slow, see README for the junction point alternative.
set SOURCE=\\wsl$\Ubuntu\home\kailazy\projects\ordnung\ordnung-qt

:: Windows-native build output (separate from the Linux build dir in WSL)
set BUILD=C:\ordnung-build

:: ───────────────────────────────────────────────────────────────────────────

if not exist "%QT_PATH%" (
    echo ERROR: Qt not found at %QT_PATH%
    echo Edit QT_PATH in this file to match your Qt install location.
    pause
    exit /b 1
)

set PATH=%MINGW_PATH%\bin;%NINJA_PATH%;%PATH%

cmake -B "%BUILD%" -S "%SOURCE%" ^
      -G "Ninja" ^
      -DCMAKE_BUILD_TYPE=Debug ^
      -DCMAKE_PREFIX_PATH="%QT_PATH%"

cmake --build "%BUILD%" -j %NUMBER_OF_PROCESSORS%

if errorlevel 1 (
    echo.
    echo Build failed.
    pause
    exit /b 1
)

"%QT_PATH%\bin\windeployqt.exe" --no-translations "%BUILD%\Ordnung.exe"

"%BUILD%\Ordnung.exe"
