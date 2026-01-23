@echo off
setlocal enableextensions enabledelayedexpansion
chcp 65001 >nul 2>&1
title SS-EXT V2.2.0 - Kurulum

cls
echo ========================================
echo   SS-EXT V2.2.0 - Kurulum
echo   Yapimci: OMERBABACO
echo ========================================
echo.

rem 1) Python kontrolu
echo [1/5] Python kontrol ediliyor...
set "PYVER="
for /f "delims=" %%A in ('python --version 2^>^&1') do set "PYVER=%%A"
if "%PYVER%"=="" (
    echo Python bulunamadi!
    echo Lutfen https://python.org adresinden Python 3.x yukleyin ve PATH'e ekleyin.
    pause
    exit /b 1
)
echo OK - %PYVER%
echo.

rem 2) Sanal ortam olusturma
echo [2/5] Sanal ortam olusturuluyor...
python -m venv venv
if errorlevel 1 (
    echo Sanal ortam olusturulamadi!
    pause
    exit /b 1
)
echo OK - Sanal ortam hazir
echo.

rem 3) Aktivasyon
echo [3/5] Sanal ortam aktif ediliyor...
call "%CD%\venv\Scripts\activate.bat"
if errorlevel 1 (
    echo Sanal ortam aktivasyonu basarisiz!
    echo Aktivasyon icin su komutu deneyin: venv\Scripts\activate.bat
    pause
    exit /b 1
)
echo OK - Sanal ortam aktif
echo.

rem 4) Pip guncelleme
echo [4/5] Pip guncelleniyor...
python -m pip install --upgrade pip
if errorlevel 1 (
    echo Pip guncellemesi basarisiz!
    pause
    exit /b 1
)
echo OK - Pip guncellendi
echo.

rem 5) Bagimliliklari yukleme
echo [5/5] Bagimliliklar yukleniyor...
python -m pip install -r requirements.txt
if errorlevel 1 (
    echo Bagimlilikler yuklenemedi. requirements.txt dosyasini kontrol edin.
    pause
    exit /b 1
)
echo.
echo ========================================
echo        KURULUM TAMAMLANDI!
echo        Yapimci: OMERBABACO
echo ========================================
echo.
echo Calistirmak icin:
echo   baslat.bat       - Normal mod
echo   gizli_baslat.vbs - Arka plan
echo.
echo Durdurmak icin:
echo   durdur.bat
echo.
echo.

rem .env yonetimi: kullanici manuel olusturacak
if exist ".env" (
    echo .env bulundu. Devam ediliyor...
) else (
    echo .env bulunamadi.
    echo Lutfen [.env.example] dosyasini kopyalayip doldurun ve sonra kur.bat'i yeniden calistirin.
)

echo.
pause
