@echo off
chcp 65001 >nul 2>&1
title GG-EXT V1.5.3 - Durdur

:: Windows 10+ ANSI renk destegini aktif et
reg add HKCU\Console /v VirtualTerminalLevel /t REG_DWORD /d 1 /f >nul 2>&1

cls
echo.
echo  [96m========================================[0m
echo  [93m       GG-EXT V1.5.3 - Durduruluyor[0m
echo  [90m       Yapimci: OMERBABACO[0m
echo  [96m========================================[0m
echo.

:: Virtual env check
if not exist "venv\Scripts\activate.bat" (
    echo [X] Virtual env not found!
    pause
    exit /b 1
)

echo  [93m[*][0m  [97mSanal ortam aktif ediliyor...[0m
call venv\Scripts\activate.bat
if errorlevel 1 (
    echo      [91mX Sanal ortam aktivasyonu basarisiz![0m
    pause
    exit /b 1
)
echo  [92m[OK][0m  [97mSanal ortam aktif[0m
echo.

echo [*] Stopping GG-EXT...
python stop_graceful.py

echo.
echo  [96m========================================[0m
echo  [92m       GG-EXT DURDURULDU![0m
echo  [90m       Yapimci: OMERBABACO[0m
echo  [96m========================================[0m
echo.

timeout /t 3 >nul
