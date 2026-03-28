@echo off
cd /d "%~dp0"

if exist "dist\ss-ext.exe" (
  start "" "dist\ss-ext.exe" --settings
  exit /b 0
)

if exist "ss-ext.exe" (
  start "" "ss-ext.exe" --settings
  exit /b 0
)

python main.py --settings
