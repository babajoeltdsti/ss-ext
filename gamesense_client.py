"""
SteelSeries GameSense API temcisi
"""

import json
import os
import time
from typing import Any, Dict, Optional

import requests

from config import DEVELOPER, GAME_DISPLAY_NAME, GAME_NAME, ICONS, get_core_props_path
from intro_animation import INTRO_TEXT_FRAMES


class GameSenseClient:
    def __init__(self):
        self.base_url: Optional[str] = None
        self.session = requests.Session()
        self.session.headers.update({"Content-Type": "application/json"})

    def discover_server(self) -> bool:
        """SteelSeries Engine'i bulur"""
        path = get_core_props_path()

        if not os.path.exists(path):
            print("[X] SteelSeries Engine bulunamadi!")
            print(f"    Konum: {path}")
            return False

        try:
            with open(path, "r") as f:
                props = json.load(f)
                if props.get("address"):
                    self.base_url = f"http://{props['address']}"
                    print(f"[OK] SteelSeries Engine: {self.base_url}")
                    return True
        except Exception as e:
            print(f"[X] Hata: {e}")
        return False

    def _post(self, endpoint: str, data: Dict[str, Any]) -> Optional[Dict]:
        if not self.base_url:
            return None
        try:
            r = self.session.post(f"{self.base_url}/{endpoint}", json=data, timeout=2)
            r.raise_for_status()
            return r.json() if r.text else {}
        except Exception:
            return None

    def register_game(self) -> bool:
        data = {
            "game": GAME_NAME,
            "game_display_name": GAME_DISPLAY_NAME,
            "developer": DEVELOPER,
            "deinitialize_timer_length_ms": 60000,
        }
        if self._post("game_metadata", data) is not None:
            print(f"[OK] Uygulama: {GAME_DISPLAY_NAME}")
            return True
        return False

    def register_event(self, event_name: str) -> bool:
        """Event'i value_optional ile kaydeder"""
        data = {"game": GAME_NAME, "event": event_name, "value_optional": True}
        return self._post("register_game_event", data) is not None

    def setup_handlers(self) -> bool:
        """Tüm handler'ları ayarlar"""
        handlers = [
            (
                "INTRO",
                ICONS["none"],
                [
                    {"has-text": True, "context-frame-key": "line1", "bold": True},
                    {"has-text": True, "context-frame-key": "line2"},
                ],
            ),
            (
                "CLOCK",
                ICONS["clock"],
                [
                    {"has-text": True, "context-frame-key": "time", "bold": True},
                    {"has-text": True, "context-frame-key": "date"},
                ],
            ),
            (
                "SPOTIFY",
                ICONS["music"],
                [
                    {"has-text": True, "context-frame-key": "title", "bold": True},
                    {"has-text": True, "context-frame-key": "artist"},
                ],
            ),
            # Spotify süre gösterimli handler (3 satır simülasyonu - 2 satırda)
            # Satır 1: Şarkı adı
            # Satır 2: Sanatçı | Süre
            (
                "SPOTIFY_TIME",
                ICONS["music"],
                [
                    {"has-text": True, "context-frame-key": "title", "bold": True},
                    {"has-text": True, "context-frame-key": "info"},
                ],
            ),
            (
                "VOLUME",
                ICONS["none"],
                [
                    {"has-text": True, "context-frame-key": "title", "bold": True},
                    {"has-text": True, "context-frame-key": "level"},
                ],
            ),
            (
                "NOTIFICATION",
                ICONS["message"],
                [
                    {"has-text": True, "context-frame-key": "app", "bold": True},
                    {"has-text": True, "context-frame-key": "message"},
                ],
            ),
            # E-posta bildirimi handler'ı
            (
                "EMAIL_NOTIFICATION",
                ICONS["email"],
                [
                    {"has-text": True, "context-frame-key": "subject", "bold": True},
                    {"has-text": True, "context-frame-key": "sender"},
                ],
            ),
            # Güncelleme mesajı handler'ı
            (
                "UPDATE",
                ICONS["none"],
                [
                    {"has-text": True, "context-frame-key": "title", "bold": True},
                    {"has-text": True, "context-frame-key": "status"},
                ],
            ),
            # Güncelleme progress bar handler'ı
            (
                "UPDATE_PROGRESS",
                ICONS["none"],
                [
                    {"has-text": True, "context-frame-key": "title", "bold": True},
                    {"has-text": True, "context-frame-key": "progress"},
                ],
            ),
        ]

        for event, icon, lines in handlers:
            # Önce event'i kaydet (value_optional ile)
            self.register_event(event)

            # Sonra handler'ı bağla
            data = {
                "game": GAME_NAME,
                "event": event,
                "handlers": [
                    {
                        "device-type": "screened",
                        "zone": "one",
                        "mode": "screen",
                        "datas": [{"icon-id": icon, "lines": lines}],
                    }
                ],
            }
            if self._post("bind_game_event", data) is None:
                return False

        print("[OK] Handler'lar ayarlandi")
        return True

    def send_event(self, event: str, frame: Dict[str, Any]) -> bool:
        """Event gönder - her seferinde değer değişsin diye counter kullan"""
        data = {
            "game": GAME_NAME,
            "event": event,
            "data": {
                "value": int(time.time() * 1000) % 100,  # Her zaman farklı değer
                "frame": frame,
            },
        }
        return self._post("game_event", data) is not None

    def play_intro(self):
        """SS-EXT Intro"""
        from config import VERSION_DISPLAY

        print("\n" + "=" * 35)
        print(f"       S S - E X T   {VERSION_DISPLAY}")
        print("=" * 35 + "\n")

        for frame in INTRO_TEXT_FRAMES:
            self.send_event("INTRO", {"line1": frame["line1"], "line2": frame["line2"]})
            time.sleep(frame["duration"] / 1000.0)

    def play_outro(self):
        """SS-EXT Kapanis Animasyonu"""
        from intro_animation import OUTRO_TEXT_FRAMES

        for frame in OUTRO_TEXT_FRAMES:
            self.send_event("INTRO", {"line1": frame["line1"], "line2": frame["line2"]})
            time.sleep(frame["duration"] / 1000.0)

    def send_clock(self, time_str: str, date_str: str) -> bool:
        return self.send_event("CLOCK", {"time": time_str, "date": date_str})

    def send_spotify(self, title: str, artist: str) -> bool:
        """Spotify şarkı bilgisi (süresiz)"""
        return self.send_event("SPOTIFY", {"title": title, "artist": artist})

    def _scroll_text(self, text: str, max_len: int = 16, offset: int = 0) -> str:
        """Kayan yazı için belirli bir offset ile 16 karakterlik görüntü döndürür."""
        if not text:
            return "".center(max_len)

        if len(text) <= max_len:
            return text.center(max_len)

        scroll_text = text + "    "
        text_len = len(scroll_text)
        start = offset % text_len
        display = ""
        for i in range(max_len):
            idx = (start + i) % text_len
            display += scroll_text[idx]
        return display

    def send_spotify_with_time(
        self,
        title: str,
        artist: str,
        current_time: str,
        total_time: str,
        combined_scroll: int = 0,
    ) -> bool:
        """
        Spotify şarkı bilgisi + Süre gösterimi
        OLED:
          Satır 1: "ŞARKI - SANATÇI" (kaydırmalı olabilir)
          Satır 2: "1:23/3:45" (sadece süre)
        """
        max_line_len = 13
        time_display = f"{current_time}/{total_time}"

        combined = f"{title} - {artist}"
        # Eğer combined uzunluğu tam olarak ekrana sığıyorsa bile kayan yazı olsun;
        # sadece daha kısa metinleri ortala.
        if len(combined) < max_line_len:
            title_line = combined.center(max_line_len)
        else:
            scroll_text = combined + "    "
            text_len = len(scroll_text)
            start = combined_scroll % text_len
            title_line = ""
            for i in range(max_line_len):
                idx = (start + i) % text_len
                title_line += scroll_text[idx]

        return self.send_event(
            "SPOTIFY_TIME", {"title": title_line, "info": time_display}
        )

    def send_volume(self, volume: int, muted: bool = False) -> bool:
        """Ses seviyesini ekranda göster"""
        if muted:
            title = "    SESSIZ".center(16)
            level = "   SES KAPALI".center(16)
        else:
            title = "SES SEVIYESI".center(16)
            # Progress bar oluştur
            bar_len = 8
            filled = int(volume / 100 * bar_len)
            bar = "█" * filled + "░" * (bar_len - filled)
            level = f"{bar} %{volume}".center(16)
        return self.send_event("VOLUME", {"title": title, "level": level})

    def send_notification(self, app: str, message: str = "Bildirim", scroll_offset: int = 0) -> bool:
        """Bildirim göster (mesajlaşma uygulamaları için). Uzun mesajlar kayan yazı olur."""
        # Başlık (app) için 10 karakter sınırı; uzun ise kayan yazı kullan
        title_width = 10
        app_display = app if len(app) <= title_width else app[:title_width]
        app_display = app_display.center(title_width)

        # Mesaj satırı - kısa mesajları soldan hizala (boşluk olmasın), uzun mesajları kaydır
        if not message:
            message_display = "".ljust(16)
        elif len(message) <= 16:
            message_display = message.ljust(16)
        else:
            message_display = self._scroll_text(message, 16, scroll_offset)

        # Compose final 16-char lines: place title (10 chars) at line start, pad to 16
        app_line = app_display.ljust(16)[:16]
        message_line = message_display.ljust(16)[:16]

        return self.send_event(
            "NOTIFICATION", {"app": app_line, "message": message_line}
        )

    def send_email_notification(self, subject: str, sender: str, scroll_offset: int = 0) -> bool:
        """E-posta bildirimi göster. Uzun konu ve gönderenler kayan yazı olarak gösterilir."""
        # Subject ve sender için 10 karakter sınırı; uzun ise kayan yazı göster
        title_width = 10
        subject_display = self._scroll_text(subject, title_width, scroll_offset)
        sender_display = self._scroll_text(sender, title_width, scroll_offset)

        # Place subject/sender at line start and pad to 16 chars (no leading gap)
        subject_line = subject_display.ljust(16)[:16]
        sender_line = sender_display.ljust(16)[:16]

        return self.send_event(
            "EMAIL_NOTIFICATION", {"subject": subject_line, "sender": sender_line}
        )

    def send_update_message(self, title: str, status: str) -> bool:
        """Güncelleme mesajı göster (örn: 'Güncelleme Mevcut!')"""
        # Metni ortala (16 karakter ekran genişliği)
        title_centered = title.center(16)
        status_centered = status.center(16)
        return self.send_event(
            "UPDATE", {"title": title_centered, "status": status_centered}
        )

    def send_update_progress(self, title: str, progress_bar: str, percent: int) -> bool:
        """
        Güncelleme progress bar göster
        Args:
            title: Başlık (örn: 'Indiriliyor', 'Kuruluyor')
            progress_bar: Progress bar string'i (örn: '████░░░░')
            percent: Yüzde değeri
        """
        # Format: "████░░░░ %75" - ortalanmış
        title_centered = title.center(16)
        progress_display = f"{progress_bar} %{percent}".center(16)
        return self.send_event(
            "UPDATE_PROGRESS", {"title": title_centered, "progress": progress_display}
        )

    def heartbeat(self):
        self._post("game_heartbeat", {"game": GAME_NAME})
