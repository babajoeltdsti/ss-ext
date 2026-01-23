@echo off
chcp 65001 >nul 2>&1
title GG-EXT V1.5.3 - Kurulum

:: Windows 10+ ANSI renk destegini aktif et
reg add HKCU\Console /v VirtualTerminalLevel /t REG_DWORD /d 1 /f >nul 2>&1

cls
echo.
echo  [96m========================================[0m
echo  [93m       GG-EXT V1.5.3 - Kurulum[0m
echo  [90m       Yapimci: OMERBABACO[0m
echo  [96m========================================[0m
echo.

:: Python kontrolu
echo  [96m[1/5][0m  [97mPython kontrol ediliyor...[0m
for /f "tokens=2" %%i in ('python --version 2^>^&1') do set PYVER=%%i
if "%%PYVER%%"=="" (
    echo      [91mX Python bulunamadi![0m
    echo      [90mhttps://python.org adresinden yukleyin[0m
    pause
    exit /b 1
)
echo      [92mOK[0m - Python %PYVER% bulundu
echo.

:: Sanal ortam olusturma
echo  [96m[2/5][0m  [97mSanal ortam olusturuluyor...[0m
python -m venv venv
if errorlevel 1 (
    echo      [91mX Sanal ortam olusturulamadi![0m
    pause
    exit /b 1
)
echo      [92mOK[0m - Sanal ortam hazir
echo.

:: Aktivasyon
echo  [96m[3/5][0m  [97mSanal ortam aktif ediliyor...[0m
call venv\Scripts\activate.bat
if errorlevel 1 (
    echo      [91mX Sanal ortam aktivasyonu basarisiz![0m
    pause
    exit /b 1
)
echo      [92mOK[0m - Sanal ortam aktif
echo.

:: Pip guncelleme
echo  [96m[4/5][0m  [97mPip guncelleniyor... (Bu asama pip tarafindan yayinlanacak ilerlemeyi gosterir)[0m
python -m pip install --upgrade pip
if errorlevel 1 (
    echo      [91mX Pip guncellemesi basarisiz!%u001b[0m
    pause
    exit /b 1
)
echo      [92mOK[0m - Pip guncellendi
echo.

:: Bagimliliklari yukleme
echo [96m[5/5][0m [97mBagimliliklar yukleniyor...[0m
echo      [90m(Bu biraz zaman alabilir)[0m
echo.
pip install -r requirements.txt
if errorlevel 1 (
    echo [X] Failed to install requirements!
    pause
    exit /b 1
)
echo.
echo  [96m========================================[0m
echo  [92m       KURULUM TAMAMLANDI![0m
echo  [90m       Yapimci: OMERBABACO[0m
echo  [96m========================================[0m
echo.
echo  [97mCalistirmak icin:[0m
echo   [93mbaslat.bat[0m       [90m- Normal mod[0m
echo   [93mgizli_baslat.vbs[0m [90m- Arka plan[0m
echo.
echo  [97mDurdurmak icin:[0m
echo   [93mdurdur.bat[0m
echo.

echo.

pause
