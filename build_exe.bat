@echo off
setlocal

REM Build SS-EXT as a single hidden-window executable.
cd /d "%~dp0"

where pyinstaller >nul 2>nul
if errorlevel 1 (
    echo [!] PyInstaller not found. Installing...
    python -m pip install pyinstaller
)

echo [*] Building ss-ext.exe ...
pyinstaller ^
  --noconfirm ^
  --clean ^
  --onefile ^
  --windowed ^
  --name ss-ext ^
  --hidden-import pycaw.pycaw ^
  --hidden-import comtypes ^
  --hidden-import winrt.windows.media.control ^
  --hidden-import winrt.windows.foundation ^
  --hidden-import win32gui ^
  --hidden-import win32process ^
  main.py

if errorlevel 1 (
    echo [X] Build failed.
    exit /b 1
)

echo [OK] Build complete: dist\ss-ext.exe
exit /b 0
