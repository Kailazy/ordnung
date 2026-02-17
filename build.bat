@echo off
title Building Eyebags Terminal
cd /d "%~dp0"

echo ============================================================
echo  Building Eyebags Terminal standalone app
echo ============================================================
echo.

echo [1/3] Checking dependencies...
pip install -r requirements.txt --quiet
if errorlevel 1 (
    echo ERROR: Failed to install dependencies
    pause
    exit /b 1
)

echo [2/3] Running PyInstaller...
pyinstaller eyebags.spec --clean --noconfirm
if errorlevel 1 (
    echo ERROR: PyInstaller build failed
    pause
    exit /b 1
)

echo [3/3] Build complete!
echo.
echo ============================================================
echo  Output: dist\Eyebags Terminal\Eyebags Terminal.exe
echo ============================================================
echo.
echo You can copy the entire "dist\Eyebags Terminal" folder
echo to any location and run "Eyebags Terminal.exe" directly.
echo.
echo NOTE: FFmpeg must be installed on the target machine
echo       for audio conversion to work.
echo ============================================================
pause
