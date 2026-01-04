@echo off
echo ========================================
echo    GG-EXT V1.0 - Kurulum
echo ========================================
echo.

python --version >nul 2>&1
if errorlevel 1 (
    echo [X] Python bulunamadi!
    echo     https://python.org adresinden yukle
    pause
    exit /b 1
)

echo [OK] Python bulundu
echo.
echo Virtual environment olusturuluyor...
python -m venv venv

echo Bagimliliklar yukleniyor...
call venv\Scripts\activate.bat
pip install -r requirements.txt

echo.
echo ========================================
echo    Kurulum tamamlandi!
echo ========================================
echo.
echo Calistir: start.bat veya start_hidden.vbs
echo.
pause
