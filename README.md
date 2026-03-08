# SS-EXT V2.0

SteelSeries klavyelerin OLED ekranlarında gerçek zamanlı sistem bilgilerini görüntüleyen GameSense eklentisi.

Yaptıklarım hoşunuza gidiyorsa bağış yapmaktan çekinmeyin; https://buymeacoffee.com/valentinoo
Şimdiden yapılan bütün bağışlar için teşekkür ederim! Bunlar sayesinde gelen motivasyonla yeni sürümler ve yeni özellikler sizlerle olacak.

## Özellikler

- ⏰ **Saat ve Tarih** - Gerçek zamanlı gösterim
- 🎵 **Spotify** - Çalan şarkı bilgisi ve ilerleme çubuğu
- 🔊 **Ses Kontrolü** - Ses seviyesi ve mute durumu
- 📱 **Bildirimler** - WhatsApp, Discord, Telegram vb.
- 📧 **E-posta** - IMAP ile yeni e-posta bildirimleri
- 🎮 **Oyun Modu** - Aktif oyun adı, süre ve sıcaklık gösterimi
- 🌡️ **Sıcaklık** - CPU/GPU sıcaklık izleme (oyun modunda)
- 🔄 **Otomatik Güncelleme** - GitHub'dan otomatik güncelleme

## Gereksinimler

- **Windows** (zorunlu)
- **SteelSeries GG** uygulaması çalışıyor olmalı
- **Python 3.8+**

## Kurulum

```bash
# Otomatik kurulum
kur.bat

# veya manuel
pip install -r requirements.txt
```

## Kullanım

```bash
# Normal başlatma
python main.py
baslat.bat

# Arka planda (pencere olmadan)
gizli_baslat.vbs

# Durdurma
python stop_graceful.py
durdur.bat
```

## Dosya Yapısı

```
ss-ext/
 main.py              # Ana uygulama
 gamesense_client.py  # GameSense API istemcisi
 system_monitor.py    # Monitör modülleri
 config.py            # Yapılandırma
 intro_animation.py   # Animasyonlar
 auto_updater.py      # Otomatik güncelleme
 stop_graceful.py     # Düzgün kapatma
 requirements.txt     # Bağımlılıklar
 kur.bat              # Kurulum scripti
 baslat.bat           # Başlatma scripti
 gizli_baslat.vbs     # Gizli başlatma
 durdur.bat           # Durdurma scripti
```

## Yapılandırma

`config.py` veya `.env` dosyasından ayarlar:

| Ayar | Varsayılan | Açıklama |
|------|------------|----------|
| `UPDATE_INTERVAL` | `0.2` | Güncelleme aralığı (saniye) |
| `AUTO_UPDATE_ENABLED` | `True` | Otomatik güncelleme |

### E-posta Ayarları (.env)

```env
SSEXT_EMAIL_ADDRESS=email@example.com
SSEXT_EMAIL_PASSWORD=examplepassword
SSEXT_IMAP_SERVER=mail.example.com
SSEXT_IMAP_PORT=993
SSEXT_EMAIL_ENABLED=True
```

## Desteklenen Cihazlar

- SteelSeries Apex Pro / Pro TKL
- SteelSeries Apex 7 / 5
- OLED ekranlı diğer SteelSeries cihazları

## Debug Modu

```bash
python main.py --debug
# veya
set SSEXT_DEBUG=1
python main.py
```

## Notlar

- `coreProps.json` konumu: `%PROGRAMDATA%\SteelSeries\SteelSeries Engine 3\`
- Hassas bilgileri `.env` dosyasında saklayın (gitignore'da)

---

**Yapımcı:** OMERBABACO
