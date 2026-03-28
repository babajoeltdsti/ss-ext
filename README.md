# Support This Project
Buy me a coffee: https://buymeacoffee.com/valentinoo

# SS-EXT (Windows)

SS-EXT is a Windows desktop companion that sends real-time events to SteelSeries GameSense devices.

## Highlights
- Native Windows tray app
- Real-time Spotify title + progress display
- Clock/date mode with smooth second alignment
- Volume, notification, game mode, and email overlays
- EN/TR language support
- Single-instance runtime with graceful external stop command

## Requirements
- Windows 10/11 (x64)
- SteelSeries GG / Engine installed and running
- Visual Studio 2022 Build Tools (for local compile)
- CMake 3.20+

## Build
```powershell
cd cpp
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Run
```powershell
.\build\Release\ss-ext.exe
```

## Download EXE (GitHub)
- Go to Actions and open the latest successful `build-windows` run.
- Download artifact: `carex-ext-windows-x64`.
- Inside artifact, run `Carex-Ext.exe`.

For permanent public download links:
- Create and push a tag like `v1.0.0`.
- Workflow will automatically attach `Carex-Ext.exe` under GitHub Releases.

## Stop
```powershell
.\build\Release\ss-ext.exe --stop
```

## Health Check
```powershell
.\build\Release\ss-ext.exe --doctor
```

## User-Editable Settings
Settings file is stored in `%APPDATA%\ss-ext-cpp\settings.ini`.

Editable examples:
- `language`
- `auto_update_check`
- `auto_start_task_enabled`
- `email_*` settings
- Spotify UI timing settings (`spotify_poll_interval_ms`, `spotify_marquee_shift_ms`, `spotify_timeline_refresh_ms`)

Protected (non-editable) metadata is enforced in code:
- App identity
- Developer name
- App version
- Update repository identity

## Credentials
Use Windows Credential Manager integration via commands:
```powershell
.\build\Release\ss-ext.exe --set-email-credential your@email.com app_password
.\build\Release\ss-ext.exe --clear-email-credential
```

## Roadmap: Potential Features
- Custom per-zone OLED layouts for supported devices
- User-defined overlay priority rules
- Per-app notification filters and quiet hours
- Rich media integrations (YouTube Music, Apple Music, browser players)
- Advanced game profiles with per-title templates
- GPU temperature/load telemetry integration
- CPU package power and fan curve overlays
- Multi-account email monitoring with profile switching
- OAuth-based email setup wizard
- In-app settings window (instead of file-only editing)
- Live preview panel for OLED output
- Backup/restore settings profiles
- Export/import theme packs
- Plugin system for third-party data sources
- Weather/calendar overlays with configurable schedule
- Discord/Rich Presence based status overlays
- Optional cloud sync for settings
- Signed update channel + beta/stable tracks
- Crash report packaging with one-click consent upload
- Built-in diagnostics dashboard with latency graphs

---

# SS-EXT (Türkçe)

SS-EXT, SteelSeries GameSense cihazlarına gerçek zamanlı veri gönderen Windows masaüstü yardımcı uygulamasıdır.

## Öne Çıkanlar
- Yerel Windows tray uygulaması
- Gerçek zamanlı Spotify şarkı adı + süre/progress gösterimi
- Saniyeye hizalı saat/tarih görünümü
- Ses seviyesi, bildirim, oyun modu ve e-posta overlay desteği
- EN/TR dil desteği
- Tek instance çalışma ve dışarıdan güvenli kapatma komutu

## Gereksinimler
- Windows 10/11 (x64)
- SteelSeries GG / Engine kurulu ve açık
- Visual Studio 2022 Build Tools (yerel derleme için)
- CMake 3.20+

## Derleme
```powershell
cd cpp
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Çalıştırma
```powershell
.\build\Release\ss-ext.exe
```

## Durdurma
```powershell
.\build\Release\ss-ext.exe --stop
```

## Sağlık Kontrolü
```powershell
.\build\Release\ss-ext.exe --doctor
```

## Kullanıcının Düzenleyebileceği Ayarlar
Ayar dosyası: `%APPDATA%\ss-ext-cpp\settings.ini`

Örnek düzenlenebilir alanlar:
- `language`
- `auto_update_check`
- `auto_start_task_enabled`
- `email_*` ayarları
- Spotify zamanlama ayarları (`spotify_poll_interval_ms`, `spotify_marquee_shift_ms`, `spotify_timeline_refresh_ms`)

Kullanıcıya kapalı (kod içinde sabitlenen) metadata:
- Uygulama kimliği
- Geliştirici adı
- Uygulama sürümü
- Güncelleme repository kimliği

## Kimlik Bilgileri
Windows Credential Manager entegrasyonu için komutlar:
```powershell
.\build\Release\ss-ext.exe --set-email-credential your@email.com app_password
.\build\Release\ss-ext.exe --clear-email-credential
```

## Gelecekte Eklenebilecek Özellikler
- Desteklenen cihazlar için zone bazlı özel OLED yerleşimleri
- Kullanıcı tanımlı overlay öncelik kuralları
- Uygulama bazlı bildirim filtreleri ve sessiz saatler
- Zengin medya entegrasyonları (YouTube Music, Apple Music, tarayıcı oynatıcıları)
- Oyun başlığına özel profil/şablon sistemi
- GPU sıcaklık/yük telemetrisi
- CPU paket gücü ve fan eğrisi overlay’i
- Çoklu e-posta hesabı ve profil geçişi
- OAuth tabanlı e-posta kurulum sihirbazı
- Dosya yerine uygulama içi ayar penceresi
- OLED canlı önizleme paneli
- Ayar profilini yedekleme/geri yükleme
- Tema paketlerini dışa/içe aktarma
- Üçüncü taraf veri kaynakları için plugin sistemi
- Hava durumu/takvim overlay’leri (zamanlanabilir)
- Discord/Rich Presence tabanlı durum gösterimi
- Opsiyonel bulut ayar senkronizasyonu
- İmzalı güncelleme kanalı + beta/stable ayrımı
- Tek tık onaylı çökme raporu gönderimi
- Gecikme grafikleri içeren tanılama paneli
