"""
SteelSeries OLED Eklentisi - Yapılandırma
SADECE WINDOWS DESTEKLİ
"""

import os
import platform
import sys
# Load .env file if present (optional)
try:
    from dotenv import load_dotenv

    load_dotenv()
except Exception:
    # python-dotenv may not be installed during static analysis; that's fine
    pass

# ===== WINDOWS KONTROLÜ =====
if platform.system() != "Windows":
    print("=" * 50)
    print("  HATA: Bu uygulama sadece Windows'ta calisir!")
    print(f"  Mevcut sistem: {platform.system()}")
    print("=" * 50)
    sys.exit(1)

# ===== SÜRÜM BİLGİSİ (Tek yerden yönetim) =====
VERSION = "2.1.2"
VERSION_DISPLAY = f"V{VERSION}"

# Uygulama bilgileri
GAME_NAME = "SSEXT"
GAME_DISPLAY_NAME = f"SS-EXT {VERSION_DISPLAY}"
DEVELOPER = "OMERBABACO"

# ===== OTOMATİK GÜNCELLEME AYARLARI =====
# GitHub repo bilgileri (güncelleme kontrolü için)
GITHUB_REPO_OWNER = "babajoeltdsti"
GITHUB_REPO_NAME = "ss-ext"
# Otomatik güncelleme açık/kapalı
AUTO_UPDATE_ENABLED = True

# Güncelleme aralığı (saniye) - 0.2 saniye = anlık güncelleme
UPDATE_INTERVAL = 0.2

# ===== E-POSTA BİLDİRİM AYARLARI =====
# E-posta bilgileri yalnızca ortam değişkenlerinden veya .env dosyasından alınır.
# Güvenlik nedeniyle bu dosyaya hassas bilgiler koymayın.
EMAIL_ADDRESS = os.environ.get("SSEXT_EMAIL_ADDRESS", "")
EMAIL_PASSWORD = os.environ.get("SSEXT_EMAIL_PASSWORD", "")

# IMAP sunucu (gelen)
IMAP_SERVER = os.environ.get("SSEXT_IMAP_SERVER", "")
IMAP_PORT = int(os.environ.get("SSEXT_IMAP_PORT", "993"))
IMAP_SSL = os.environ.get("SSEXT_IMAP_SSL", "True").lower() == "true"

# SMTP sunucu (giden) - opsiyonel, bazı özellikler için kullanılabilir
SMTP_SERVER = os.environ.get("SSEXT_SMTP_SERVER", "")
SMTP_PORT = int(os.environ.get("SSEXT_SMTP_PORT", "587"))
SMTP_STARTTLS = os.environ.get("SSEXT_SMTP_STARTTLS", "True").lower() == "true"

# E-posta kontrol aralığı (saniye)
EMAIL_CHECK_INTERVAL = int(os.environ.get("SSEXT_EMAIL_CHECK_INTERVAL", "30"))
# E-posta bildirimi gösterim süresi (saniye)
EMAIL_DISPLAY_DURATION = int(os.environ.get("SSEXT_EMAIL_DISPLAY_DURATION", "10"))
# E-posta bildirimi aktif/pasif
EMAIL_NOTIFICATION_ENABLED = os.environ.get("SSEXT_EMAIL_ENABLED", "True").lower() == "true"


# coreProps.json konumu (sadece Windows)
def get_core_props_path():
    return os.path.join(
        os.environ.get("PROGRAMDATA", "C:\\ProgramData"),
        "SteelSeries",
        "SteelSeries Engine 3",
        "coreProps.json",
    )


# İkon ID'leri (SteelSeries GameSense)
# Tam liste: https://github.com/SteelSeries/gamesense-sdk/blob/main/doc/api/event-icons.md
ICONS = {
    "none": 0,
    "clock": 15,
    "music": 34,
    "email": 18,      # @ Symbol - e-posta bildirimleri için
    "message": 20,    # Talking - mesajlaşma bildirimleri için
}
