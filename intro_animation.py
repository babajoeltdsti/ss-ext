"""
Açılış Animasyonu - GG-EXT
Süslü bir şekilde belirir ve silinerek kaybolur
"""

# Animasyon kareleri - her biri {line1, line2, duration_ms}
# OLED ekran ~16 karakter genişliğinde, ortalamak için boşluk ekliyoruz
INTRO_TEXT_FRAMES = [
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
    
    # Alt satır beliriyor - ortalanmış
    {"line1": "   GG-EXT", "line2": "      _", "duration": 80},
    {"line1": "   GG-EXT", "line2": "     V_", "duration": 80},
    {"line1": "   GG-EXT", "line2": "    V 1_", "duration": 80},
    {"line1": "   GG-EXT", "line2": "    V 1._", "duration": 80},
    {"line1": "   GG-EXT", "line2": "    V 1.0", "duration": 300},
    
    # Parıltı efekti (süslü çerçeve) - ortalanmış
    {"line1": "  =-GG-EXT-=", "line2": "    V 1.0", "duration": 150},
    {"line1": "  <-GG-EXT->", "line2": "    V 1.0", "duration": 150},
    {"line1": "  >>GG-EXT<<", "line2": "    V 1.0", "duration": 150},
    {"line1": "  **GG-EXT**", "line2": "    V 1.0", "duration": 150},
    {"line1": "  >>GG-EXT<<", "line2": "    V 1.0", "duration": 150},
    {"line1": "  =-GG-EXT-=", "line2": "    V 1.0", "duration": 200},
    
    # Tam görünüm - biraz bekle
    {"line1": "   GG-EXT", "line2": "    V 1.0", "duration": 800},
    
    # Silinme efekti - ortalanmış
    {"line1": "   GG-EXT.", "line2": "    V 1.0", "duration": 100},
    {"line1": "   GG-EXT..", "line2": "    V 1.0", "duration": 100},
    {"line1": "   GG-EXT...", "line2": "    V 1.0", "duration": 100},
    {"line1": "   GG-EX_", "line2": "    V 2._", "duration": 60},
    {"line1": "    GG-E_", "line2": "     V 2_", "duration": 60},
    {"line1": "    GG-_", "line2": "      V_", "duration": 60},
    {"line1": "     GG_", "line2": "      _", "duration": 60},
    {"line1": "     G_", "line2": "", "duration": 60},
    {"line1": "      _", "line2": "", "duration": 60},
    {"line1": "", "line2": "", "duration": 100},
    
    # Patlama ve hazır mesajı - ortalanmış
    {"line1": "       *", "line2": "", "duration": 80},
    {"line1": "      ***", "line2": "", "duration": 80},
    {"line1": "     *****", "line2": "", "duration": 80},
    {"line1": "   * HAZIR *", "line2": "", "duration": 300},
    {"line1": "     *****", "line2": "", "duration": 80},
    {"line1": "      ***", "line2": "", "duration": 80},
    {"line1": "       *", "line2": "", "duration": 80},
    {"line1": "", "line2": "", "duration": 100},
]

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
    
    # Son patlama - görüşürz - ortalanmış
    {"line1": "       *", "line2": "", "duration": 80},
    {"line1": "      ***", "line2": "", "duration": 80},
    {"line1": "     *****", "line2": "", "duration": 80},
    {"line1": "  GORUSURUZ!", "line2": "", "duration": 500},
    {"line1": "     *****", "line2": "", "duration": 80},
    {"line1": "      ***", "line2": "", "duration": 80},
    {"line1": "       *", "line2": "", "duration": 80},
    {"line1": "", "line2": "", "duration": 100},
]
