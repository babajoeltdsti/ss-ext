# GG-EXT V1.0 - SteelSeries OLED Eklentisi

SteelSeries klavyelerin OLED ekranlarında gerçek zamanlı sistem bilgilerini görüntüleyen bir GameSense eklentisidir.

## 👤 Yapımcı

**OMERBABACO**

## ✨ Özellikler

- ⏰ **Saat ve Tarih** - Gerçek zamanlı saat gösterimi
- 🎵 **Spotify Entegrasyonu** - Şu an çalan şarkı bilgisi
- 🔊 **Ses Kontrolü** - Sistem ses seviyesi göstergesi ve mute durumu
- ✉️ **Bildirim Desteği** - Windows bildirimleri

## 📋 Gereksinimler

- **İşletim Sistemi:** Windows (pycaw ve pywin32 gereksinimleri nedeniyle)
- **SteelSeries GG** uygulaması çalışıyor olmalı
- **Python 3.8+**
- Bağımlılıklar:
  - `requests>=2.28.0`
  - `psutil>=5.9.0`
  - `pywin32>=305`
  - `WMI>=1.5.1`
  - `pycaw>=20230407`
  - `comtypes>=1.2.0`

## 🔧 Nasıl Çalışır?

**EĞER ARKAPLANDA CMD EKRANI OLMADAN ÇALIŞMASINI İSTİYORSANIZ,** "start_hidden.vbs" dosyasını çalıştırın. Böylece görüntü kirliliği olmadan çalışacaktır.

SteelSeries GameSense SDK, localhost'ta bir REST API sunucusu çalıştırır. Bu eklenti:

1. `coreProps.json` dosyasından GameSense sunucu adresini okur
2. Uygulamayı GameSense'e kaydeder (`GGEXT` olarak)
3. OLED ekran için event handler'ları oluşturur
4. 200ms aralıklarla sistem bilgilerini günceller

## 🚀 Kurulum

### Otomatik Kurulum (Windows)
```batch
install.bat
```

### Manuel Kurulum
```bash
# Virtual environment oluştur (opsiyonel)
python -m venv venv
venv\Scripts\activate

# Bağımlılıkları yükle
pip install -r requirements.txt
```

## 📖 Kullanım

### Başlatma
```bash
# Normal başlatma
python main.py

# veya Windows batch dosyası ile
start.bat

# Gizli pencerede başlatma (arka planda)
start_hidden.vbs
```

### Durdurma
```bash
# Düzgün kapatma
python stop_graceful.py

# veya
stop.bat
```

## 📁 Dosya Yapısı

```
ss-ext/
├── main.py              # Ana uygulama (GGExt sınıfı)
├── gamesense_client.py  # GameSense REST API istemcisi
├── system_monitor.py    # Monitör modülleri (Spotify, Volume, Notification)
├── config.py            # Yapılandırma ayarları
├── intro_animation.py   # Başlangıç animasyonu
├── stop_graceful.py     # Düzgün kapatma scripti
├── requirements.txt     # Python bağımlılıkları
├── install.bat          # Windows kurulum scripti
├── start.bat            # Windows başlatma scripti
├── start_hidden.vbs     # Gizli pencerede başlatma
├── stop.bat             # Windows durdurma scripti
└── README.md            # Bu dosya
```

## 🎮 Desteklenen Cihazlar

- SteelSeries Apex Pro
- SteelSeries Apex Pro TKL
- SteelSeries Apex 7
- SteelSeries Apex 5
- Ve OLED ekranlı diğer SteelSeries cihazları

## ⚙️ Yapılandırma

`config.py` dosyasından aşağıdaki ayarları değiştirebilirsiniz:

| Ayar | Varsayılan | Açıklama |
|------|------------|----------|
| `GAME_NAME` | `GGEXT` | GameSense'e kaydedilen uygulama adı |
| `UPDATE_INTERVAL` | `0.2` | Güncelleme aralığı (saniye) |

## 📝 Notlar

- SteelSeries GG uygulamasının çalışıyor olması **zorunludur**
- `coreProps.json` konumları:
  - **Windows:** `%PROGRAMDATA%\SteelSeries\SteelSeries Engine 3\`
  - **macOS:** `/Library/Application Support/SteelSeries Engine 3/`
  - **Linux:** `~/.config/SteelSeries Engine 3/`

## 📄 Lisans

Bu proje açık kaynaklıdır.

---

**GG-EXT V1.0** | Yapımcı: **OMERBABACO**
