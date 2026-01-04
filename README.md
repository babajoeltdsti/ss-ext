# SteelSeries OLED Ekran Eklentisi

Bu proje, SteelSeries klavyelerin OLED ekranlarında sistem bilgilerini ve özel mesajları görüntüleyen bir eklentidir.

## Özellikler

- 🖥️ CPU kullanımı
- 🧠 RAM kullanımı  
- ⏰ Saat ve tarih
- 🎵 Spotify şu an çalan şarkı (opsiyonel)
- 📊 Sistem bilgileri
- ✉️ Özel mesaj gönderme

## Gereksinimler

1. **SteelSeries GG** uygulaması çalışıyor olmalı
2. Python 3.8+
3. Gerekli paketler: `pip install -r requirements.txt`

## Nasıl Çalışır?

SteelSeries GameSense SDK, localhost'ta bir REST API sunucusu çalıştırır. Bu eklenti:

1. `coreProps.json` dosyasından GameSense sunucu adresini okur
2. Uygulamayı GameSense'e kaydeder
3. OLED ekran için event handler'ları bağlar
4. Periyodik olarak sistem bilgilerini gönderir

## Kurulum

```bash
# Virtual environment oluştur (opsiyonel)
python -m venv venv
source venv/bin/activate  # Linux/Mac
# veya
venv\Scripts\activate  # Windows

# Bağımlılıkları yükle
pip install -r requirements.txt
```

## Kullanım

```bash
# Ana uygulamayı çalıştır
python main.py

# Sadece özel mesaj göster
python main.py --message "Merhaba Dünya!"

# Sistem monitörünü başlat
python main.py --monitor
```

## Dosya Yapısı

```
ss-ext/
├── main.py              # Ana uygulama
├── gamesense_client.py  # GameSense API istemcisi
├── system_monitor.py    # Sistem bilgisi toplayan modül
├── config.py            # Yapılandırma ayarları
├── requirements.txt     # Python bağımlılıkları
└── README.md           # Bu dosya
```

## Desteklenen Cihazlar

- SteelSeries Apex Pro
- SteelSeries Apex Pro TKL
- SteelSeries Apex 7
- SteelSeries Apex 5
- Ve OLED ekranlı diğer SteelSeries cihazları

## Notlar

- SteelSeries GG uygulamasının çalışıyor olması gerekir
- Linux'ta `coreProps.json` konumu: `~/.config/SteelSeries Engine 3/`
- Windows'ta: `%PROGRAMDATA%/SteelSeries/SteelSeries Engine 3/`
- macOS'ta: `/Library/Application Support/SteelSeries Engine 3/`
