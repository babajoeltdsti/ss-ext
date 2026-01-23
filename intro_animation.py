"""
Acilis Animasyonu - SS-EXT
Suslu bir sekilde belirir ve silinerek kaybolur
"""

from config import VERSION_DISPLAY

# Animasyon kareleri - her biri {line1, line2, duration_ms}
# OLED ekran ~16 karakter genişliğinde, ortalamak için boşluk ekliyoruz


def get_intro_frames():
    """Surum numarasiyla intro frame'lerini dondurur"""
    return [
        # Bos baslangic
        {"line1": "", "line2": "", "duration": 100},
        # Harfler tek tek beliriyor (typing efekti) - ortalanmis
        {"line1": "      _", "line2": "", "duration": 80},
        {"line1": "     S_", "line2": "", "duration": 80},
        {"line1": "     SS_", "line2": "", "duration": 80},
        {"line1": "    SS-_", "line2": "", "duration": 80},
        {"line1": "    SS-E_", "line2": "", "duration": 80},
        {"line1": "   SS-EX_", "line2": "", "duration": 80},
        {"line1": "   SS-EXT_", "line2": "", "duration": 80},
        {"line1": "   SS-EXT", "line2": "", "duration": 150},
        # Alt satir beliriyor - surum numarasiyla
        {"line1": "   SS-EXT", "line2": "      _", "duration": 80},
        {"line1": "   SS-EXT", "line2": f"   {VERSION_DISPLAY}", "duration": 300},
        # Parilti efekti (suslu cerceve)
        {"line1": "  =-SS-EXT-=", "line2": f"   {VERSION_DISPLAY}", "duration": 150},
        {"line1": "  <-SS-EXT->", "line2": f"   {VERSION_DISPLAY}", "duration": 150},
        {"line1": "  >>SS-EXT<<", "line2": f"   {VERSION_DISPLAY}", "duration": 150},
        {"line1": "  **SS-EXT**", "line2": f"   {VERSION_DISPLAY}", "duration": 150},
        {"line1": "  >>SS-EXT<<", "line2": f"   {VERSION_DISPLAY}", "duration": 150},
        {"line1": "  =-SS-EXT-=", "line2": f"   {VERSION_DISPLAY}", "duration": 200},
        # Tam gorunum - biraz bekle
        {"line1": "   SS-EXT", "line2": f"   {VERSION_DISPLAY}", "duration": 600},
        # Yapimci kismi beliriyor
        {"line1": "   SS-EXT", "line2": "   Yapimci:_", "duration": 80},
        {"line1": "   SS-EXT", "line2": "  Yapimci: O_", "duration": 60},
        {"line1": "   SS-EXT", "line2": " Yapimci: OM_", "duration": 60},
        {"line1": "   SS-EXT", "line2": " Yapimci: OME_", "duration": 60},
        {"line1": "   SS-EXT", "line2": "Yapimci: OMER_", "duration": 60},
        {"line1": "   SS-EXT", "line2": "  ci: OMERB_", "duration": 60},
        {"line1": "   SS-EXT", "line2": "   : OMERBA_", "duration": 60},
        {"line1": "   SS-EXT", "line2": "   OMERBAB_", "duration": 60},
        {"line1": "   SS-EXT", "line2": "  OMERBABA_", "duration": 60},
        {"line1": "   SS-EXT", "line2": "  OMERBABAC_", "duration": 60},
        {"line1": "   SS-EXT", "line2": "  OMERBABACO", "duration": 800},
        # Parilti efekti Yapimci ile
        {"line1": "  *SS-EXT*", "line2": "  OMERBABACO", "duration": 150},
        {"line1": "  **SS-EXT**", "line2": "  OMERBABACO", "duration": 150},
        {"line1": "  *SS-EXT*", "line2": "  OMERBABACO", "duration": 150},
        {"line1": "   SS-EXT", "line2": "  OMERBABACO", "duration": 400},
        # Silinme efekti
        {"line1": "   SS-EXT.", "line2": "  OMERBABACO", "duration": 80},
        {"line1": "   SS-EXT..", "line2": "  OMERBABAC_", "duration": 60},
        {"line1": "   SS-EXT...", "line2": "  OMERBABA_", "duration": 60},
        {"line1": "   SS-EX_", "line2": "   OMERBAB_", "duration": 60},
        {"line1": "    SS-E_", "line2": "   OMERBA_", "duration": 60},
        {"line1": "    SS-_", "line2": "    OMERB_", "duration": 60},
        {"line1": "     SS_", "line2": "    OMER_", "duration": 60},
        {"line1": "     S_", "line2": "     OME_", "duration": 60},
        {"line1": "      _", "line2": "     OM_", "duration": 60},
        {"line1": "", "line2": "      O_", "duration": 60},
        {"line1": "", "line2": "       _", "duration": 60},
        {"line1": "", "line2": "", "duration": 100},
        # Patlama ve hazir mesaji
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

# Kapanis animasyonu - yavasca kapaniyor (ortalanmis)
OUTRO_TEXT_FRAMES = [
    # Baslangic
    {"line1": "", "line2": "", "duration": 100},
    # Kapaniyor yazisi beliriyor - ortalanmis
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
    # Noktalar - ortalanmis
    {"line1": "  KAPANIYOR.", "line2": "", "duration": 300},
    {"line1": "  KAPANIYOR..", "line2": "", "duration": 300},
    {"line1": " KAPANIYOR...", "line2": "", "duration": 300},
    # SS-EXT yazisi beliriyor (veda) - ortalanmis
    {"line1": " KAPANIYOR...", "line2": "   SS-EXT", "duration": 400},
    # Silinme efekti - ortalanmis
    {"line1": "  KAPANIYOR..", "line2": "   SS-EX_", "duration": 80},
    {"line1": "  KAPANIYOR.", "line2": "    SS-E_", "duration": 80},
    {"line1": "  KAPANIYOR", "line2": "    SS-_", "duration": 80},
    {"line1": "   KAPANIYO_", "line2": "     SS_", "duration": 80},
    {"line1": "   KAPANIY_", "line2": "     S_", "duration": 80},
    {"line1": "    KAPANI_", "line2": "      _", "duration": 80},
    {"line1": "    KAPAN_", "line2": "", "duration": 80},
    {"line1": "     KAPA_", "line2": "", "duration": 80},
    {"line1": "     KAP_", "line2": "", "duration": 80},
    {"line1": "      KA_", "line2": "", "duration": 80},
    {"line1": "      K_", "line2": "", "duration": 80},
    {"line1": "       _", "line2": "", "duration": 80},
    # Son patlama - gorusuruz - ortalanmis
    {"line1": "       *", "line2": "", "duration": 80},
    {"line1": "      ***", "line2": "", "duration": 80},
    {"line1": "     *****", "line2": "", "duration": 80},
    {"line1": "  GORUSURUZ!", "line2": "", "duration": 500},
    {"line1": "     *****", "line2": "", "duration": 80},
    {"line1": "      ***", "line2": "", "duration": 80},
    {"line1": "       *", "line2": "", "duration": 80},
    {"line1": "", "line2": "", "duration": 100},
]
