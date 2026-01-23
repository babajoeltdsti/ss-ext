@echo off
chcp 65001 >nul 2>&1
title SS-EXT V2.0 - Baslat

:: Windows 10+ ANSI renk destegini aktif et
reg add HKCU\Console /v VirtualTerminalLevel /t REG_DWORD /d 1 /f >nul 2>&1

cls
echo.
echo  [96m========================================[0m
echo  [93m       SS-EXT V2.0 - Baslatiliyor[0m
echo  [90m       Yapimci: OMERBABACO[0m
echo  [96m========================================[0m
echo.

:: Venv check
echo  [96m[1/2][0m  [97mSanal ortam kontrol ediliyor...[0m
if not exist "venv\Scripts\activate.bat" (
    echo      [91mX Sanal ortam bulunamadi![0m
    pause
    exit /b 1
)
echo      [92mOK[0m - Sanal ortam bulundu

echo.
:: Aktivasyon
echo  [96m[2/2][0m  [97mSanal ortam aktif ediliyor...[0m
call venv\Scripts\activate.bat
if errorlevel 1 (
    echo      [91mX Sanal ortam aktivasyonu basarisiz![0m
    pause
    exit /b 1
)
echo      [92mOK[0m - Sanal ortam aktif

echo.
echo  [93m[*][0m  [97mSS-EXT baslatiliyor...[0m
python main.py %*

echo.
echo  [92m[OK][0m  [97mSS-EXT kapandi.[0m
echo.
pause
