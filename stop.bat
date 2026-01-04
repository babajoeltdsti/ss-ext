@echo off
title GG-EXT - Kapatiliyor
echo ========================================
echo    GG-EXT V1.0 - Kapatiliyor
echo ========================================
echo.

:: Graceful shutdown dene (kapanış animasyonu için)
python stop_graceful.py 2>nul

:: Eğer hala çalışıyorsa zorla kapat
taskkill /f /im pythonw.exe 2>nul
echo.
echo [OK] Tamam.
timeout /t 2
