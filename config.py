"""
SteelSeries OLED Eklentisi - Yapılandırma
SADECE WINDOWS DESTEKLİ
"""

import os
import platform
import sys

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

# Uygulama bilgileri
GAME_NAME = "GGEXT"
GAME_DISPLAY_NAME = f"GG-EXT {VERSION_DISPLAY}"
DEVELOPER = "OMERBABACO"

# ===== OTOMATİK GÜNCELLEME AYARLARI =====
# GitHub repo bilgileri (güncelleme kontrolü için)
GITHUB_REPO_OWNER = "OMERBABACO"
GITHUB_REPO_NAME = "GG-EXT"
# Otomatik güncelleme açık/kapalı
AUTO_UPDATE_ENABLED = True

# Güncelleme aralığı (saniye) - 0.2 saniye = anlık güncelleme
UPDATE_INTERVAL = 0.2


# coreProps.json konumu (sadece Windows)
def get_core_props_path():
    return os.path.join(
        os.environ.get("PROGRAMDATA", "C:\\ProgramData"),
        "SteelSeries",
        "SteelSeries Engine 3",
        "coreProps.json",
    )


# İkon ID'leri (SteelSeries GameSense)
ICONS = {
    "none": 0,
    "clock": 15,
    "music": 34,
}
