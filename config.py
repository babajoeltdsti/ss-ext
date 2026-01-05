"""
SteelSeries OLED Eklentisi - Yapılandırma
SADECE WINDOWS DESTEKLİ
"""
import os
import sys
import platform

# ===== WINDOWS KONTROLÜ =====
if platform.system() != "Windows":
    print("=" * 50)
    print("  HATA: Bu uygulama sadece Windows'ta calisir!")
    print(f"  Mevcut sistem: {platform.system()}")
    print("=" * 50)
    sys.exit(1)

# ===== SÜRÜM BİLGİSİ (Tek yerden yönetim) =====
VERSION = "1.5.3"
VERSION_DISPLAY = f"V{VERSION}"

# Uygulama bilgileri
GAME_NAME = "GGEXT"
GAME_DISPLAY_NAME = f"GG-EXT {VERSION_DISPLAY}"
DEVELOPER = "OMERBABACO"

# Güncelleme aralığı (saniye) - 0.2 saniye = anlık güncelleme
UPDATE_INTERVAL = 0.2

# coreProps.json konumu (sadece Windows)
def get_core_props_path():
    return os.path.join(
        os.environ.get("PROGRAMDATA", "C:\\ProgramData"),
        "SteelSeries", "SteelSeries Engine 3", "coreProps.json"
    )

# İkon ID'leri (SteelSeries GameSense)
ICONS = {
    "none": 0,
    "clock": 15,
    "music": 34,
}
