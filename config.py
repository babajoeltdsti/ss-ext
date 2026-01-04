"""
SteelSeries OLED Eklentisi - Yapılandırma
"""
import os
import platform

# Uygulama bilgileri
GAME_NAME = "GGEXT"
GAME_DISPLAY_NAME = "GG-EXT V1.0"
DEVELOPER = "OMERBABACO"

# Güncelleme aralığı (saniye) - daha hızlı güncelleme
UPDATE_INTERVAL = 0.2

# coreProps.json konumu
def get_core_props_path():
    system = platform.system()
    
    if system == "Windows":
        return os.path.join(
            os.environ.get("PROGRAMDATA", "C:\\ProgramData"),
            "SteelSeries", "SteelSeries Engine 3", "coreProps.json"
        )
    elif system == "Darwin":
        return "/Library/Application Support/SteelSeries Engine 3/coreProps.json"
    else:
        return os.path.expanduser("~/.config/SteelSeries Engine 3/coreProps.json")

# İkon ID'leri (SteelSeries GameSense)
# Resmi liste: https://github.com/SteelSeries/gamesense-sdk/blob/master/doc/api/json-handlers-screen.md
ICONS = {
    "none": 0,
    "clock": 15,   # Saat ikonu (doğrulandı)
    "music": 34,    # Müzik için özel ikon yok, none kullan
}
