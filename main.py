#!/usr/bin/env python3
"""
GG-EXT - SteelSeries OLED Eklentisi
"""
import signal
import sys
import time

from gamesense_client import GameSenseClient
from system_monitor import SystemMonitor, SpotifyMonitor, VolumeMonitor, NotificationMonitor
from config import UPDATE_INTERVAL


class GGExt:
    def __init__(self):
        self.client = GameSenseClient()
        self.monitor = SystemMonitor()
        self.spotify = SpotifyMonitor()
        self.volume = VolumeMonitor()
        self.notifications = NotificationMonitor()
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
        print("=" * 40)
        print("    GG-EXT V1.0 - SteelSeries OLED")
        print("=" * 40 + "\n")
        
        if not self.client.discover_server():
            return False
        if not self.client.register_game():
            return False
        if not self.client.setup_handlers():
            return False
        return True
    
    def run(self):
        # Intro
        self.client.play_intro()
        
        print("[*] Calisiyor... (Ctrl+C ile kapat)\n")
        
        self.running = True
        hb_counter = 0
        last_track = None
        
        # Overlay süreleri (saniye)
        VOLUME_OVERLAY_DURATION = 2.0
        NOTIFICATION_OVERLAY_DURATION = 3.0
        
        while self.running:
            try:
                current_time = time.time()
                
                # --- Öncelik 1: Bildirimler ---
                notification = self.notifications.check_whatsapp_notification_simple()
                if notification:
                    print(f"[📱] {notification['app']}: {notification.get('message', 'Yeni Bildirim')}")
                    self.client.send_notification(notification['app'], notification.get('message', 'Yeni Bildirim'))
                    self._overlay_active = True
                    self._overlay_end_time = current_time + NOTIFICATION_OVERLAY_DURATION
                    self._overlay_type = "notification"
                    time.sleep(UPDATE_INTERVAL)
                    continue
                
                # --- Öncelik 2: Ses değişikliği ---
                volume_change = self.volume.check_volume_change()
                if volume_change:
                    vol = volume_change['volume']
                    muted = volume_change['muted']
                    status = "MUTED" if muted else f"{vol}%"
                    print(f"[🔊] Ses: {status}")
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
                    if last_track != track.get('title'):
                        print(f"[♫] {track['artist']} - {track['title']}")
                        last_track = track.get('title')
                        self.spotify.reset_scroll()
                    
                    # Kayan yazı uygula
                    title_display = self.spotify.get_scrolling_text(track['title'])
                    artist_display = self.spotify.get_scrolling_text(track['artist'])
                    
                    self.client.send_spotify(title_display, artist_display)
                    self.spotify.update_scroll()
                    
                else:
                    # Şarkı yok - saat modu
                    if last_track is not None:
                        print(f"[◷] Saat modu")
                        last_track = None
                        self.spotify.reset_scroll()
                    
                    d = self.monitor.get_clock_display()
                    self.client.send_clock(d['time'], d['date'])
                
                # Heartbeat - her 5 saniyede
                hb_counter += 1
                if hb_counter >= 10:
                    self.client.heartbeat()
                    hb_counter = 0
                
                time.sleep(UPDATE_INTERVAL)
                
            except Exception as e:
                print(f"[!] Hata: {e}")
                time.sleep(1)
        
        # Kapanış animasyonu
        print("\n[*] Kapaniyor...")
        self.client.play_outro()
        print("[OK] Kapatildi.")


def main():
    app = GGExt()
    
    if not app.start():
        print("\n[X] Baslatilamadi!")
        print("    SteelSeries GG calistiginden emin ol.")
        input("\nEnter'a bas...")
        sys.exit(1)
    
    app.run()


if __name__ == "__main__":
    main()
