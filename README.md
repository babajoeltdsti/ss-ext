# Carex-Ext

## Donate
If this project helps you, you can support development here:

`https://buymeacoffee.com/valentinoo`

Carex-Ext is a Windows tray companion for SteelSeries GameSense OLED devices.
It sends live overlays for clock, media, notifications, updates, volume, game mode, and email.

## New In Current Build
- New `About` tab in Settings with:
	- Maker info
	- Current app version
	- BuyMeACoffee link (`buymeacoffee.com/valentinoo`)
	- Manual `Check for updates` button
- Manual update check compares current version with the latest GitHub release.
	- If newer version exists: update pipeline starts
	- If already up to date: status message is shown
- Update "up-to-date" OLED subtitle now scrolls (marquee) instead of clipping.
- Language support expanded to:
	- Turkish (`tr`)
	- English (`en`)
	- German (`de`)
	- French (`fr`)
	- Spanish (`es`)
- Language expansion is applied to both:
	- Settings UI
	- OLED overlay text keys
- Email security validation is now strict:
	- IMAP SSL/TLS must be enabled
	- SMTP SSL/TLS must be enabled

## Core Features
- Native Win32 tray runtime
- Modern WebView settings panel
- Profile system (create/copy/rename/delete/import/export/lock)
- Notification filtering (WhatsApp, Discord, Instagram)
- Media source toggles (Spotify, YouTube Music, Apple Music, Browser)
- 12h/24h clock and multi-language UI
- Auto-start integration with Task Scheduler
- Auto-update and manual update check support
- Windows Credential Manager integration for email credentials

## Requirements
- Windows 10/11 (x64)
- SteelSeries GG / Engine running
- CMake 3.20+
- Visual Studio 2022 (MSVC v143)

## Build
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Run
```powershell
.\build\Release\ss-ext.exe
```

## Useful Commands
```powershell
# graceful stop
.\build\Release\ss-ext.exe --stop

# diagnostics
.\build\Release\ss-ext.exe --doctor

# set email credential (stored in Windows Credential Manager)
.\build\Release\ss-ext.exe --set-email-credential your@email.com app_password

# clear email credential
.\build\Release\ss-ext.exe --clear-email-credential
```

## Update System
- Repository target: `babajoeltdsti/ss-ext`
- Auto check is controlled by settings.
- Manual check is available in `Settings > About`.
- On release/tag workflows, CI publishes `Carex-Ext.exe` as artifact/release asset.

## Release Notes Workflow (Recommended)
1. Ensure code is on `main`.
2. Create a tag like `v1.0` (or newer).
3. Push the tag.
4. GitHub Actions builds and uploads `Carex-Ext.exe`.
5. Users can update through in-app check.

## Data And Settings Location
- App data root: `%APPDATA%\carex-ext`
- Profiles: `%APPDATA%\carex-ext\profiles`
- Logs: `%APPDATA%\carex-ext\app.log`

