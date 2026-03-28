"""
SteelSeries OLED Eklentisi - Yapılandırma
SADECE WINDOWS DESTEKLİ
"""

import os
import platform
import sys

from i18n import DEFAULT_LANGUAGE, set_language
from settings_store import load_user_settings

# .env dosyasını yükle (opsiyonel)
try:
    from dotenv import load_dotenv

    if os.path.exists(".env"):
        load_dotenv(".env")
    elif os.path.exists(".env.example"):
        load_dotenv(".env.example")
    else:
        load_dotenv()
except ImportError:
    pass  # python-dotenv yüklü değilse sessizce devam et

_USER_SETTINGS = load_user_settings()


def _read_setting(key: str, default: str) -> str:
    env_value = os.environ.get(key)
    if env_value is not None:
        return str(env_value)

    if key in _USER_SETTINGS:
        return str(_USER_SETTINGS[key])

    return default


def _read_bool(key: str, default: bool) -> bool:
    raw = _read_setting(key, "True" if default else "False")
    return str(raw).strip().lower() == "true"


def _read_int(key: str, default: int) -> int:
    raw = _read_setting(key, str(default))
    try:
        return int(str(raw).strip())
    except ValueError:
        return default


# ===== WINDOWS KONTROLÜ =====
if platform.system() != "Windows":
    print("=" * 50)
    print("  HATA: Bu uygulama sadece Windows'ta calisir!")
    print(f"  Mevcut sistem: {platform.system()}")
    print("=" * 50)
    sys.exit(1)

# ===== SÜRÜM BİLGİSİ (Tek yerden yönetim) =====
VERSION = "2.0"
VERSION_DISPLAY = f"V{VERSION}"

# Dil ayarı (en varsayılan)
LANGUAGE = _read_setting("SSEXT_LANG", DEFAULT_LANGUAGE).strip().lower() or DEFAULT_LANGUAGE
if LANGUAGE not in {"en", "tr"}:
    LANGUAGE = DEFAULT_LANGUAGE
set_language(LANGUAGE)

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
EMAIL_ADDRESS = _read_setting("SSEXT_EMAIL_ADDRESS", "")
EMAIL_PASSWORD = _read_setting("SSEXT_EMAIL_PASSWORD", "")

# IMAP sunucu (gelen)
IMAP_SERVER = _read_setting("SSEXT_IMAP_SERVER", "")
IMAP_PORT = _read_int("SSEXT_IMAP_PORT", 993)
IMAP_SSL = _read_bool("SSEXT_IMAP_SSL", True)

# SMTP sunucu (giden) - opsiyonel, bazı özellikler için kullanılabilir
SMTP_SERVER = _read_setting("SSEXT_SMTP_SERVER", "")
SMTP_PORT = _read_int("SSEXT_SMTP_PORT", 587)
SMTP_STARTTLS = _read_bool("SSEXT_SMTP_STARTTLS", True)

# E-posta kontrol aralığı (saniye)
EMAIL_CHECK_INTERVAL = _read_int("SSEXT_EMAIL_CHECK_INTERVAL", 10)
# E-posta bildirimi gösterim süresi (saniye)
EMAIL_DISPLAY_DURATION = _read_int("SSEXT_EMAIL_DISPLAY_DURATION", 10)
# E-posta bildirimi aktif/pasif
EMAIL_NOTIFICATION_ENABLED = _read_bool("SSEXT_EMAIL_ENABLED", True)


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
    "music": 40,
    "email": 18,      # @ Symbol - e-posta bildirimleri için
    "message": 20,    # Talking - mesajlaşma bildirimleri için
    "game": 38,       # Controller - oyun modu için
}

