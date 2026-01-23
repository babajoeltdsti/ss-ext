"""
Açılış Animasyonu - GG-EXT
Süslü bir şekilde belirir ve silinerek kaybolur
"""

from config import VERSION_DISPLAY

# Animasyon kareleri - her biri {line1, line2, duration_ms}
# OLED ekran ~16 karakter genişliğinde, ortalamak için boşluk ekliyoruz


def get_intro_frames():
    """Sürüm numarasıyla intro frame'lerini döndürür"""
    return [
        # Boş başlangıç
        {"line1": "", "line2": "", "duration": 100},
        # Harfler tek tek beliriyor (typing efekti) - ortalanmış
        {"line1": "      _", "line2": "", "duration": 80},
        {"line1": "     G_", "line2": "", "duration": 80},
        {"line1": "     GG_", "line2": "", "duration": 80},
        {"line1": "    GG-_", "line2": "", "duration": 80},
        {"line1": "    GG-E_", "line2": "", "duration": 80},
        {"line1": "   GG-EX_", "line2": "", "duration": 80},
        {"line1": "   GG-EXT_", "line2": "", "duration": 80},
        {"line1": "   GG-EXT", "line2": "", "duration": 150},
        # Alt satır beliriyor - sürüm numarasıyla
        {"line1": "   GG-EXT", "line2": "      _", "duration": 80},
        {"line1": "   GG-EXT", "line2": f"   {VERSION_DISPLAY}", "duration": 300},
        # Parıltı efekti (süslü çerçeve)
        {"line1": "  =-GG-EXT-=", "line2": f"   {VERSION_DISPLAY}", "duration": 150},
        {"line1": "  <-GG-EXT->", "line2": f"   {VERSION_DISPLAY}", "duration": 150},
        {"line1": "  >>GG-EXT<<", "line2": f"   {VERSION_DISPLAY}", "duration": 150},
        {"line1": "  **GG-EXT**", "line2": f"   {VERSION_DISPLAY}", "duration": 150},
        {"line1": "  >>GG-EXT<<", "line2": f"   {VERSION_DISPLAY}", "duration": 150},
        {"line1": "  =-GG-EXT-=", "line2": f"   {VERSION_DISPLAY}", "duration": 200},
        # Tam görünüm - biraz bekle
        {"line1": "   GG-EXT", "line2": f"   {VERSION_DISPLAY}", "duration": 600},
        # Yapımcı kısmı beliriyor
        {"line1": "   GG-EXT", "line2": "   Yapimci:_", "duration": 80},
        {"line1": "   GG-EXT", "line2": "  Yapimci: O_", "duration": 60},
        {"line1": "   GG-EXT", "line2": " Yapimci: OM_", "duration": 60},
        {"line1": "   GG-EXT", "line2": " Yapimci: OME_", "duration": 60},
        {"line1": "   GG-EXT", "line2": "Yapimci: OMER_", "duration": 60},
        {"line1": "   GG-EXT", "line2": "  ci: OMERB_", "duration": 60},
        {"line1": "   GG-EXT", "line2": "   : OMERBA_", "duration": 60},
        {"line1": "   GG-EXT", "line2": "   OMERBAB_", "duration": 60},
        {"line1": "   GG-EXT", "line2": "  OMERBABA_", "duration": 60},
        {"line1": "   GG-EXT", "line2": "  OMERBABAC_", "duration": 60},
        {"line1": "   GG-EXT", "line2": "  OMERBABACO", "duration": 800},
        # Parıltı efekti Yapımcı ile
        {"line1": "  *GG-EXT*", "line2": "  OMERBABACO", "duration": 150},
        {"line1": "  **GG-EXT**", "line2": "  OMERBABACO", "duration": 150},
        {"line1": "  *GG-EXT*", "line2": "  OMERBABACO", "duration": 150},
        {"line1": "   GG-EXT", "line2": "  OMERBABACO", "duration": 400},
        # Silinme efekti
        {"line1": "   GG-EXT.", "line2": "  OMERBABACO", "duration": 80},
        {"line1": "   GG-EXT..", "line2": "  OMERBABAC_", "duration": 60},
        {"line1": "   GG-EXT...", "line2": "  OMERBABA_", "duration": 60},
        {"line1": "   GG-EX_", "line2": "   OMERBAB_", "duration": 60},
        {"line1": "    GG-E_", "line2": "   OMERBA_", "duration": 60},
        {"line1": "    GG-_", "line2": "    OMERB_", "duration": 60},
        {"line1": "     GG_", "line2": "    OMER_", "duration": 60},
        {"line1": "     G_", "line2": "     OME_", "duration": 60},
        {"line1": "      _", "line2": "     OM_", "duration": 60},
        {"line1": "", "line2": "      O_", "duration": 60},
        {"line1": "", "line2": "       _", "duration": 60},
        {"line1": "", "line2": "", "duration": 100},
        # Patlama ve hazır mesajı
        {"line1": "       *", "line2": "", "duration": 80},
        {"line1": "      ***", "line2": "", "duration": 80},
        {"line1": "     *****", "line2": "", "duration": 80},
        {"line1": "   * HAZIR *", "line2": "", "duration": 300},
        {"line1": "     *****", "line2": "", "duration": 80},
        {"line1": "      ***", "line2": "", "duration": 80},
        {"line1": "       *", "line2": "", "duration": 80},
        {"line1": "", "line2": "", "duration": 100},
    ]


# Eski değişken isimleri için uyumluluk
INTRO_TEXT_FRAMES = get_intro_frames()

# Kapanış animasyonu - yavaşça kapanıyor (ortalanmış)
OUTRO_TEXT_FRAMES = [
    # Başlangıç
    {"line1": "", "line2": "", "duration": 100},
    # Kapanıyor yazısı beliriyor - ortalanmış
    {"line1": "       _", "line2": "", "duration": 60},
    {"line1": "      K_", "line2": "", "duration": 60},
    {"line1": "      KA_", "line2": "", "duration": 60},
    {"line1": "     KAP_", "line2": "", "duration": 60},
    {"line1": "     KAPA_", "line2": "", "duration": 60},
    {"line1": "    KAPAN_", "line2": "", "duration": 60},
    {"line1": "    KAPANI_", "line2": "", "duration": 60},
    {"line1": "   KAPANIY_", "line2": "", "duration": 60},
    {"line1": "   KAPANIYO_", "line2": "", "duration": 60},
    {"line1": "  KAPANIYOR_", "line2": "", "duration": 60},
    {"line1": "  KAPANIYOR", "line2": "", "duration": 200},
    # Noktalar - ortalanmış
    {"line1": "  KAPANIYOR.", "line2": "", "duration": 300},
    {"line1": "  KAPANIYOR..", "line2": "", "duration": 300},
    {"line1": " KAPANIYOR...", "line2": "", "duration": 300},
    # GG-EXT yazısı beliriyor (veda) - ortalanmış
    {"line1": " KAPANIYOR...", "line2": "   GG-EXT", "duration": 400},
    # Silinme efekti - ortalanmış
    {"line1": "  KAPANIYOR..", "line2": "   GG-EX_", "duration": 80},
    {"line1": "  KAPANIYOR.", "line2": "    GG-E_", "duration": 80},
    {"line1": "  KAPANIYOR", "line2": "    GG-_", "duration": 80},
    {"line1": "   KAPANIYO_", "line2": "     GG_", "duration": 80},
    {"line1": "   KAPANIY_", "line2": "     G_", "duration": 80},
    {"line1": "    KAPANI_", "line2": "      _", "duration": 80},
    {"line1": "    KAPAN_", "line2": "", "duration": 80},
    {"line1": "     KAPA_", "line2": "", "duration": 80},
    {"line1": "     KAP_", "line2": "", "duration": 80},
    {"line1": "      KA_", "line2": "", "duration": 80},
    {"line1": "      K_", "line2": "", "duration": 80},
    {"line1": "       _", "line2": "", "duration": 80},
    # Son patlama - görüşürüz - ortalanmış
    {"line1": "       *", "line2": "", "duration": 80},
    {"line1": "      ***", "line2": "", "duration": 80},
    {"line1": "     *****", "line2": "", "duration": 80},
    {"line1": "  GORUSURUZ!", "line2": "", "duration": 500},
    {"line1": "     *****", "line2": "", "duration": 80},
    {"line1": "      ***", "line2": "", "duration": 80},
    {"line1": "       *", "line2": "", "duration": 80},
    {"line1": "", "line2": "", "duration": 100},
]
