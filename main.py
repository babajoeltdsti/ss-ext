#!/usr/bin/env python3
"""
SS-EXT - SteelSeries OLED Eklentisi
"""
import argparse
import os
import signal
import sys
import time

from gamesense_client import GameSenseClient
from config import UPDATE_INTERVAL, VERSION_DISPLAY, AUTO_UPDATE_ENABLED, EMAIL_DISPLAY_DURATION, ICONS
from auto_updater import check_and_update
from system_monitor import (
    EmailMonitor,
    NotificationMonitor,
    SpotifyMonitor,
    SystemMonitor,
    VolumeMonitor,
)

# ANSI renk kodlari (konsol ciktisi icin)
C = "\u001b[96m"  # cyan
Y = "\u001b[93m"  # yellow
G = "\u001b[92m"  # green
R = "\u001b[91m"  # red
W = "\u001b[97m"  # white
D = "\u001b[90m"  # dim
RESET = "\u001b[0m"

# Debug: set SSEXT_DEBUG=1 in environment to enable per-loop progress logs
DEBUG_PROGRESS = os.getenv("SSEXT_DEBUG", "0") == "1"

# Allow --debug flag for Windows users who can't easily set env vars
parser = argparse.ArgumentParser(add_help=False)
parser.add_argument("--debug", action="store_true")
args, _ = parser.parse_known_args()
if args.debug:
    DEBUG_PROGRESS = True


class GGExt:
    def __init__(self):
        self.client = GameSenseClient()
        self.monitor = SystemMonitor()
        self.spotify = SpotifyMonitor()
        self.volume = VolumeMonitor()
        self.notifications = NotificationMonitor()
        self.email_monitor = EmailMonitor()
        self.running = False

        # Overlay durumları (geçici gösterimler için)
        self._overlay_active = False
        self._overlay_end_time = 0
        self._overlay_type = None

        # Signal handlers
        signal.signal(signal.SIGINT, self._stop)
        signal.signal(signal.SIGTERM, self._stop)
        # Windows için CTRL_BREAK_EVENT
        try:
            signal.signal(signal.SIGBREAK, self._stop)
        except AttributeError:
            pass  # Linux'ta SIGBREAK yok

    def _stop(self, *args):
        print("\n[!] Kapatiliyor...")
        self.running = False

    def start(self) -> bool:
        print(C + "=" * 40 + RESET)
        print(f"{Y}    SS-EXT {VERSION_DISPLAY} - SteelSeries OLED{RESET}")
        print(C + "=" * 40 + "\n")

        if not self.client.discover_server():
            return False
        if not self.client.register_game():
            return False
        if not self.client.setup_handlers():
            return False
        
        # Otomatik güncelleme kontrolü
        if AUTO_UPDATE_ENABLED:
            try:
                needs_restart = check_and_update(self.client)
                if needs_restart:
                    print(f"\n{Y}[!]{RESET} {W}Güncelleme tamamlandı! Lütfen uygulamayı yeniden başlatın.{RESET}")
                    print(f"{G}[*]{RESET} {W}3 saniye içinde çıkılıyor...{RESET}")
                    time.sleep(3)
                    sys.exit(0)
            except Exception as e:
                print(f"{D}[!] Güncelleme kontrolü başarısız: {e}{RESET}")
        
        # E-posta izlemeyi başlat
        if self.email_monitor.is_enabled():
            self.email_monitor.start()
        
        return True

    def run(self):
        # Intro
        self.client.play_intro()

        print(f"{Y}[*]{RESET} {W}Çalışıyor... (Ctrl+C ile kapat)\n{RESET}")

        self.running = True
        hb_counter = 0
        last_track = None
        combined_scroll = 0

        # Overlay süreleri (saniye)
        VOLUME_OVERLAY_DURATION = 2.0
        NOTIFICATION_OVERLAY_DURATION = 3.0
        EMAIL_OVERLAY_DURATION = float(EMAIL_DISPLAY_DURATION)

        while self.running:
            try:
                current_time = time.time()

                # --- Öncelik 0: E-posta Bildirimleri ---
                if self.email_monitor.is_enabled():
                    email_notif = self.email_monitor.get_pending_email()
                    if email_notif:
                        print(
                            f"{Y}[📧]{RESET} {W}Yeni E-posta: {email_notif['sender']} - {email_notif['subject']}{RESET}"
                        )

                        # Başlık/gönderen için 10 karakter sınırı (scroll hedefi)
                        title_width = 10

                        # Scroll gereksinimi için her iki metnin kaydırma uzunluğunu hesapla
                        def scroll_length(s: str) -> int:
                            if not s:
                                return 1
                            if len(s) <= title_width:
                                return 1
                            return len(s + "    ")

                        subj_len = scroll_length(email_notif.get("subject", ""))
                        sender_len = scroll_length(email_notif.get("sender", ""))

                        # En az steps: görüntü süresi / update interval
                        base_steps = max(1, int(EMAIL_OVERLAY_DURATION / UPDATE_INTERVAL))
                        # Tam döngü göstermek için gereken adım sayısı
                        needed_steps = max(subj_len, sender_len, base_steps)

                        for i in range(needed_steps):
                            self.client.send_email_notification(
                                email_notif["subject"], email_notif["sender"], scroll_offset=i
                            )
                            time.sleep(UPDATE_INTERVAL)

                        # Overlay sonrası kısa bekleme
                        self._overlay_active = True
                        self._overlay_end_time = current_time + 0.01
                        self._overlay_type = "email"
                        continue

                # --- Öncelik 1: Bildirimler ---
                notification = self.notifications.check_whatsapp_notification_simple()
                if notification:
                    print(
                        f"{Y}[📱]{RESET} {W}{notification['app']}: {notification.get('message', 'Yeni Bildirim')}{RESET}"
                    )
                    # Kayan yazı ile göster (mesaj uzun ise kaydır)
                    steps = max(1, int(NOTIFICATION_OVERLAY_DURATION / UPDATE_INTERVAL))
                    for i in range(steps):
                        self.client.send_notification(
                            notification["app"], notification.get("message", "Yeni Bildirim"), scroll_offset=i
                        )
                        time.sleep(UPDATE_INTERVAL)
                    self._overlay_active = True
                    self._overlay_end_time = current_time + 0.01
                    self._overlay_type = "notification"
                    continue

                # --- Öncelik 2: Ses değişikliği ---
                volume_change = self.volume.check_volume_change()
                if volume_change:
                    vol = volume_change["volume"]
                    muted = volume_change["muted"]
                    status = "MUTED" if muted else f"{vol}%"
                    print(f"{C}[🔊]{RESET} {W}Ses: {status}{RESET}")
                    self.client.send_volume(vol, muted)
                    self._overlay_active = True
                    self._overlay_end_time = current_time + VOLUME_OVERLAY_DURATION
                    self._overlay_type = "volume"
                    time.sleep(UPDATE_INTERVAL)
                    continue

                # --- Overlay aktifse bekle ---
                if self._overlay_active and current_time < self._overlay_end_time:
                    time.sleep(UPDATE_INTERVAL)
                    continue

                # Overlay bitti
                if self._overlay_active:
                    self._overlay_active = False
                    self._overlay_type = None

                # --- Normal mod: Spotify veya Saat ---
                track = self.spotify.get_current_track()

                if track:
                    # Spotify çalıyor - şarkı var
                    if last_track != track.get("title"):
                        print(
                            f"{G}[♫]{RESET} {W}{track['artist']} - {track['title']}{RESET}"
                        )
                        last_track = track.get("title")
                        self.spotify.reset_scroll()
                        combined_scroll = 0

                    # Progress/süre bilgisini al
                    progress_info = self.spotify.get_progress_info()

                    if DEBUG_PROGRESS:
                        print(
                            f"{D}[DBG]{RESET} {W}get_progress_info returned: {progress_info}{RESET}"
                        )

                    if progress_info and progress_info.get("duration_ms", 0) > 0:
                        # Süre gösterimli mod
                        # Üst satır: "Şarkı - Sanatçı" (kaydırmalı gerekirse)
                        ok = self.client.send_spotify_with_time(
                            title=track["title"],
                            artist=track["artist"],
                            current_time=progress_info["progress_str"],
                            total_time=progress_info["duration_str"],
                            combined_scroll=combined_scroll,
                        )
                        if DEBUG_PROGRESS:
                            est_flag = progress_info.get("estimated", False)
                            print(
                                f"{D}[DBG]{RESET} {W}progress={progress_info['progress_str']} ({progress_info['progress_ms']}ms) duration={progress_info['duration_str']} estimated={est_flag} send_ok={ok} scroll={combined_scroll}{RESET}"
                            )
                        combined_scroll += 1
                    else:
                        # Süre bilgisi yok, normal mod
                        title_display = self.spotify.get_scrolling_text(track["title"])
                        artist_display = self.spotify.get_scrolling_text(
                            track["artist"]
                        )
                        self.client.send_spotify(title_display, artist_display)

                    self.spotify.update_scroll()

                else:
                    # Şarkı yok - saat modu
                    if last_track is not None:
                        print(f"{D}[◷]{RESET} {W}Saat modu{RESET}")
                        last_track = None
                        self.spotify.reset_scroll()
                        combined_scroll = 0

                    d = self.monitor.get_clock_display()
                    self.client.send_clock(d["time"], d["date"])

                # Heartbeat - her 5 saniyede
                hb_counter += 1
                if hb_counter >= 10:
                    self.client.heartbeat()
                    hb_counter = 0

                time.sleep(UPDATE_INTERVAL)

            except Exception as e:
                print(f"{R}[!]{RESET} {W}Hata: {e}{RESET}")
                time.sleep(1)

        # Kapanış animasyonu
        print(f"\n{Y}[*]{RESET} {W}Kapaniyor...{RESET}")
        
        # E-posta izlemeyi durdur
        if self.email_monitor.is_enabled():
            self.email_monitor.stop()
        
        self.client.play_outro()
        print(f"{G}[OK]{RESET} {W}Kapatildi.{RESET}")


def main():
    app = GGExt()

    if not app.start():
        print(f"\n{R}[X]{RESET} Baslatilamadi!")
        print(f"    {D}SteelSeries GG calistiginden emin ol.{RESET}")
        input("\nEnter'a bas...")
        sys.exit(1)

    app.run()


if __name__ == "__main__":
    main()
