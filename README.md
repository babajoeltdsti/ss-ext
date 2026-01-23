# SS-EXT V2.0 - SteelSeries OLED Eklentisi

SteelSeries klavyelerin OLED ekranlarında gerçek zamanlı sistem bilgilerini görüntüleyen bir GameSense eklentisidir.

## 🧾 Changelog (v2.0)

- **Version display:** Uygulama sürümü `V2.0` olarak güncellendi.
- **Spotify progress smoothing:** Predict+EMA tabanlı smoothing eklendi; küçük geri jitter'lar yoksayılıyor, tutarlı küçük geri düşüşler için onay sayısı, büyük backward seek'ler anında kabul ediliyor. (parametreler: small_back_ms, required confirmations, per-tick cap)
- **Spotify title handling:** PowerShell decoding ve Unicode normalization iyileştirildi; UI için Spotify başlık genişliği 13 olarak ayarlandı (daha erken kaydırma).
- **VolumeMonitor:** WinMM fallback ve `check_volume_change` desteği eklendi; pycaw ile uyum geliştirildi.
- **Env handling:** `.env` yoksa otomatik olarak `.env.example` okunur; bu sayede örnek konfigürasyonlarla çalıştırmak kolaylaştı.
- **Debugging:** `--debug` ile çalıştırıldığında `SSEXT_DEBUG=1` olarak ayarlanır ve `get_progress_info` gibi fonksiyonlardan ayrıntılı `[DBG]` logları alınır

## 👤 Yapımcı

**OMERBABACO**

## ✨ Özellikler

- ⏰ **Saat ve Tarih** - Gerçek zamanlı saat gösterimi
- 🎵 **Spotify Entegrasyonu** - Şu an çalan şarkı bilgisi
- � **Şarkı Progress Bar** - Şarkının ilerleme durumu (süre göstergesi ile)
- 🔊 **Ses Kontrolü** - Sistem ses seviyesi göstergesi ve mute durumu
- ✉️ **Bildirim Desteği** - Windows bildirimleri
- � **E-posta İzleme** - IMAP üzerinden yeni e-posta bildirimleri (konfigürasyon SSEXT_EMAIL_*)
- �🔄 **Otomatik Güncelleme** - GitHub'dan otomatik güncelleme kontrolü ve kurulumu

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
  - `winrt-Windows.Media.Control>=2.0.0` (Şarkı progress bar için)
  - `winrt-Windows.Foundation>=2.0.0`

## 🔧 Nasıl Çalışır?

**EĞER ARKAPLANDA CMD EKRANI OLMADAN ÇALIŞMASINI İSTİYORSANIZ,** "gizli_baslat.vbs" dosyasını çalıştırın. Böylece görüntü kirliliği olmadan çalışacaktır.

SteelSeries GameSense SDK, localhost'ta bir REST API sunucusu çalıştırır. Bu eklenti:

1. `coreProps.json` dosyasından GameSense sunucu adresini okur
2. Uygulamayı GameSense'e kaydeder (`SSEXT` olarak)
3. OLED ekran için event handler'ları oluşturur
4. 200ms aralıklarla sistem bilgilerini günceller

## 🚀 Kurulum

### Otomatik Kurulum (Windows)
```batch
kur.bat
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
baslat.bat

# Gizli pencerede başlatma (arka planda)
gizli_baslat.vbs
```

### Durdurma
```bash
# Düzgün kapatma
python stop_graceful.py

# veya
durdur.bat
```

## 📁 Dosya Yapısı

```
ss-ext/
├── main.py              # Ana uygulama (GGExt sınıfı)
├── gamesense_client.py  # GameSense REST API istemcisi
├── system_monitor.py    # Monitör modülleri (Spotify, Volume, Notification)
├── config.py            # Yapılandırma ayarları
├── intro_animation.py   # Başlangıç animasyonu
├── auto_updater.py      # Otomatik güncelleme modülü
├── stop_graceful.py     # Düzgün kapatma scripti
├── requirements.txt     # Python bağımlılıkları
├── kur.bat              # Windows kurulum scripti
├── baslat.bat           # Windows başlatma scripti
├── gizli_baslat.vbs     # Gizli pencerede başlatma
├── durdur.bat           # Windows durdurma scripti
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
| `GAME_NAME` | `SSEXT` | GameSense'e kaydedilen uygulama adı |
| `UPDATE_INTERVAL` | `0.2` | Güncelleme aralığı (saniye) |
| `AUTO_UPDATE_ENABLED` | `True` | Otomatik güncelleme açık/kapalı |
| `GITHUB_REPO_OWNER` | `babajoeltdsti` | GitHub kullanıcı adı |
| `GITHUB_REPO_NAME` | `ss-ext` | GitHub repo adı |

## 📝 Notlar

- SteelSeries GG uygulamasının çalışıyor olması **zorunludur**
- `coreProps.json` konumları:
  - **Windows:** `%PROGRAMDATA%\SteelSeries\SteelSeries Engine 3\`
  - **macOS:** `/Library/Application Support/SteelSeries Engine 3/`
  - **Linux:** `~/.config/SteelSeries Engine 3/`

## 📄 Lisans

Bu proje açık kaynaklıdır.

---

**SS-EXT V2.0** | Yapimci: **OMERBABACO**

---

Not: E-posta şifreleri ve sunucu bilgileri gibi hassas verileri repoya koymayın. Kurulum sırasında `kur.bat` bu bilgileri sizden isteyip Windows kullanıcı ortam değişkeni (`setx`) olarak kaydeder. Daha güvenli kullanım için GitHub'da `Secrets` veya dış bir gizli depo (ör. Vault) kullanın.
 
 Yeni davranış: `kur.bat` şimdi hassas bilgileri doğrudan `.env` dosyasına yazar (lokal). `.env` dosyası `.gitignore` içinde olduğundan repoya itilmeyecektir.
 
 Eğer geçmişte hassas bilgi commitlediyseniz, `scripts/clean_history.sh` scriptini kullanarak geçmişi temizleyebilirsiniz (BFG veya `git-filter-repo` gerektirir). Scripti çalıştırmadan önce README'deki uyarıları okuyun.
