# SS-EXT V2.0

English is the default language for documentation and runtime messages. Turkish is available below.

If this project helps you, you can support it: https://buymeacoffee.com/valentinoo

## Language

- English (default)
- Turkish: see [Turkce](#turkce)

## Features

- Clock and Date on SteelSeries OLED
- Spotify track metadata and timeline display
- Volume and mute status overlays
- Windows messaging notifications (WhatsApp, Discord, Telegram, etc.)
- IMAP email notifications
- Game Mode with session duration and CPU/GPU temperatures
- Automatic update checks from GitHub releases
- System tray control (Settings, Restart, Exit)
- Optional Start with Windows toggle from tray

## Requirements

- Windows only
- SteelSeries GG must be installed and running

End users do not need Python or build tools.

## Install

```bash
# End-user: use published EXE package
# Place ss-ext.exe (or dist\ss-ext.exe) in folder and run baslat.bat
```

## Run

```bash
# start (EXE-first)
baslat.bat

# open settings UI
settings.bat

# graceful stop
durdur.bat
```

## Configuration

Settings are loaded from environment variables (`.env` is supported).

| Key | Default | Description |
|---|---|---|
| `SSEXT_LANG` | `en` | UI/message language: `en` or `tr` |
| `SSEXT_EMAIL_ENABLED` | `True` | Enables IMAP email monitoring |
| `SSEXT_EMAIL_CHECK_INTERVAL` | `10` | Email polling interval (seconds) |
| `SSEXT_EMAIL_DISPLAY_DURATION` | `10` | OLED overlay duration for email |

Email settings example:

```env
SSEXT_LANG=en
SSEXT_EMAIL_ADDRESS=email@example.com
SSEXT_EMAIL_PASSWORD=examplepassword
SSEXT_IMAP_SERVER=mail.example.com
SSEXT_IMAP_PORT=993
SSEXT_IMAP_SSL=True
SSEXT_EMAIL_ENABLED=True
```

You can also configure these values from the built-in settings window:

```bash
# on Windows
settings.bat
```

Tray mode can open settings anytime from the tray menu.

## Update Support

SS-EXT checks the latest GitHub release automatically at startup.

- Python source mode: downloads release zip and updates project files in place.
- EXE mode (PyInstaller/onefile): downloads the release EXE asset, stages replacement, and relaunches after exit.

Release recommendation for broad user rollout:

1. Publish a tagged GitHub release (`vX.Y.Z`).
2. Upload a Windows EXE asset (example: `ss-ext-win-x64.exe`).
3. Keep semantic version tags so clients can compare correctly.

## Build EXE (Windows)

This section is for maintainer/release workflow only. End users should not build.

Example command:

```bash
pyinstaller --onefile --windowed main.py
```

Or use the included helper script:

```bash
build_exe.bat
```

Output location:

- Main output: `dist\\ss-ext.exe`
- Optional copy for launcher compatibility: project root `ss-ext.exe`

For production, include hidden imports for `winrt`, `pycaw`, `comtypes`, and `pywin32` modules if needed.

## Release Workflow (Maintainer)

1. Build EXE via `build_exe.bat`.
2. Validate startup/tray/settings/update behavior on clean Windows machine.
3. Publish GitHub release tag (`vX.Y.Z`) and upload EXE asset.
4. Send/publish this EXE package to SteelSeries distribution channel.

## Supported Devices

- SteelSeries Apex Pro / Pro TKL
- SteelSeries Apex 7 / 5
- Other SteelSeries OLED-capable devices

## Notes

- `coreProps.json` is resolved from `%PROGRAMDATA%\SteelSeries\SteelSeries Engine 3\`.
- Store sensitive credentials in environment variables; do not hardcode secrets.

---

## Turkce

SS-EXT, SteelSeries OLED ekraninda saat, Spotify, ses, bildirim ve e-posta bilgilerini gosteren bir GameSense eklentisidir.

### Ozellikler

- Saat ve tarih gosterimi
- Spotify sarki bilgisi ve sure
- Ses/mute overlay
- WhatsApp/Discord/Telegram vb. bildirimler
- IMAP e-posta bildirimi
- Oyun modu (oyun suresi + CPU/GPU sicaklik)
- GitHub release tabanli otomatik guncelleme
- Sistem tepsisi kontrol menusu (Ayarlar, Yeniden Baslat, Cikis)
- Tepsiden Windows baslangicinda calistirma ac/kapat

### Dil Modu

- Varsayilan: Ingilizce (`SSEXT_LANG=en`)
- Turkce: `SSEXT_LANG=tr`

### Guncelleme Notu

- Python kaynak surumunde zip ile dosyalar yerinde guncellenir.
- EXE surumunde release EXE dosyasi indirilir, uygulama kapandiktan sonra yer degistirilir ve yeniden acilir.

### Kurulum/Kullanim

```bash
# Son kullanici: yayinlanan EXE paketini kullanir
baslat.bat
gizli_baslat.vbs
settings.bat
durdur.bat
```

---

Author: OMERBABACO
