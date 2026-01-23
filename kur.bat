@echo off
chcp 65001 >nul 2>&1
title SS-EXT V2.1 - Kurulum

:: Windows 10+ ANSI renk destegini aktif et
reg add HKCU\Console /v VirtualTerminalLevel /t REG_DWORD /d 1 /f >nul 2>&1

cls
echo.
echo  [96m========================================[0m
echo  [93m       SS-EXT V2.1 - Kurulum[0m
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

:: E-posta yapılandırması iste
set /p USER_EMAIL_CONFIG="E-posta yapılandırması yapmak ister misiniz? (E/h): "
if /i "%USER_EMAIL_CONFIG%"=="E" (
    echo.
    set /p SSEXT_EMAIL_ADDRESS_INPUT="E-posta adresi (örnek: info@ornek.com): "
    set /p SSEXT_EMAIL_PASSWORD_INPUT="E-posta şifresi (gizli): "
    set /p SSEXT_IMAP_SERVER_INPUT="IMAP sunucu (gelen) (örnek: mail.ornek.com): "
    set /p SSEXT_IMAP_PORT_INPUT="IMAP port (varsayılan 993): "
    if "%SSEXT_IMAP_PORT_INPUT%"=="" set SSEXT_IMAP_PORT_INPUT=993
    set /p SSEXT_IMAP_SSL_INPUT="IMAP SSL? (True/False) (varsayılan True): "
    if "%SSEXT_IMAP_SSL_INPUT%"=="" set SSEXT_IMAP_SSL_INPUT=True
    set /p SSEXT_SMTP_SERVER_INPUT="SMTP sunucu (giden) (örnek: mail.ornek.com) (boş bırakılabilir): "
    set /p SSEXT_SMTP_PORT_INPUT="SMTP port (varsayılan 587): "
    if "%SSEXT_SMTP_PORT_INPUT%"=="" set SSEXT_SMTP_PORT_INPUT=587
    set /p SSEXT_SMTP_STARTTLS_INPUT="SMTP STARTTLS? (True/False) (varsayılan True): "
    if "%SSEXT_SMTP_STARTTLS_INPUT%"=="" set SSEXT_SMTP_STARTTLS_INPUT=True

    echo Ayarlar .env dosyasina yaziliyor (.env savunmasi: .gitignore icinde olmalidir)...
    >.env echo # SS-EXT configuration file
    >>.env echo SSEXT_EMAIL_ADDRESS=%SSEXT_EMAIL_ADDRESS_INPUT%
    >>.env echo SSEXT_EMAIL_PASSWORD=%SSEXT_EMAIL_PASSWORD_INPUT%
    >>.env echo SSEXT_IMAP_SERVER=%SSEXT_IMAP_SERVER_INPUT%
    >>.env echo SSEXT_IMAP_PORT=%SSEXT_IMAP_PORT_INPUT%
    >>.env echo SSEXT_IMAP_SSL=%SSEXT_IMAP_SSL_INPUT%
    if not "%SSEXT_SMTP_SERVER_INPUT%"=="" (
        >>.env echo SSEXT_SMTP_SERVER=%SSEXT_SMTP_SERVER_INPUT%
        >>.env echo SSEXT_SMTP_PORT=%SSEXT_SMTP_PORT_INPUT%
        >>.env echo SSEXT_SMTP_STARTTLS=%SSEXT_SMTP_STARTTLS_INPUT%
    )

    echo .env dosyasi olusturuldu ve yerel olarak kaydedildi.
)

echo.

pause
