"""
Sistem Monitörü - Saat, Spotify, Volume ve Bildirimler
"""
import subprocess
import time
from datetime import datetime
from typing import Dict, Optional, Callable


class VolumeMonitor:
    """Windows ses seviyesi izleyici - pycaw 20240210+ uyumlu"""
    
    def __init__(self):
        self._last_volume: Optional[int] = None
        self._last_mute: Optional[bool] = None
        self._endpoint_volume = None
        self._init_audio()
    
    def _init_audio(self):
        """Windows Audio API'yi başlat"""
        try:
            from pycaw.pycaw import AudioUtilities
            
            # Yeni pycaw API (2024+) - AudioDevice objesi döner
            speakers = AudioUtilities.GetSpeakers()
            
            # EndpointVolume attribute'u var mı?
            if hasattr(speakers, 'EndpointVolume'):
                self._endpoint_volume = speakers.EndpointVolume
                print("[OK] Ses izleme aktif (pycaw EndpointVolume)")
                return
            
            # Eski API dene
            if hasattr(speakers, 'Activate'):
                from ctypes import cast, POINTER
                from comtypes import CLSCTX_ALL
                from pycaw.pycaw import IAudioEndpointVolume
                interface = speakers.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
                self._endpoint_volume = cast(interface, POINTER(IAudioEndpointVolume))
                print("[OK] Ses izleme aktif (pycaw Activate)")
                return
                
        except ImportError:
            print("[!] pycaw yüklü değil: pip install pycaw")
        except Exception as e:
            print(f"[!] pycaw hatası: {e}")
        
        # Fallback: winmm (master volume değil ama bir şey)
        self._endpoint_volume = "winmm"
        print("[OK] Ses izleme aktif (winmm fallback)")
    
    def get_volume(self) -> Optional[int]:
        """Mevcut ses seviyesini yüzde olarak döndürür (0-100)"""
        if self._endpoint_volume is None:
            return None
        
        if self._endpoint_volume == "winmm":
            return self._get_volume_winmm()
        
        try:
            # pycaw EndpointVolume
            level = self._endpoint_volume.GetMasterVolumeLevelScalar()
            return int(level * 100)
        except:
            return None
    
    def is_muted(self) -> Optional[bool]:
        """Sesin kapalı olup olmadığını döndürür"""
        if self._endpoint_volume is None:
            return None
        
        if self._endpoint_volume == "winmm":
            return False  # winmm mute bilgisi vermiyor
        
        try:
            return bool(self._endpoint_volume.GetMute())
        except:
            return None
    
    def _get_volume_winmm(self) -> Optional[int]:
        """Windows Mixer API ile ses seviyesi al (fallback)"""
        try:
            import ctypes
            from ctypes import wintypes
            
            winmm = ctypes.windll.winmm
            volume = wintypes.DWORD()
            result = winmm.waveOutGetVolume(0, ctypes.byref(volume))
            
            if result == 0:
                left = volume.value & 0xFFFF
                right = (volume.value >> 16) & 0xFFFF
                avg = (left + right) // 2
                return int(avg / 65535 * 100)
        except:
            pass
        return None
    
    def check_volume_change(self) -> Optional[Dict[str, any]]:
        """
        Ses değişikliği kontrolü.
        Değişiklik varsa {'volume': int, 'muted': bool} döndürür.
        Yoksa None döndürür.
        """
        current_volume = self.get_volume()
        current_mute = self.is_muted()
        
        if current_volume is None:
            return None
        
        # İlk çalıştırma
        if self._last_volume is None:
            self._last_volume = current_volume
            self._last_mute = current_mute
            return None
        
        # Değişiklik kontrolü (2% tolerans - küçük dalgalanmaları yoksay)
        volume_changed = abs(current_volume - self._last_volume) >= 2
        mute_changed = current_mute != self._last_mute
        
        if volume_changed or mute_changed:
            self._last_volume = current_volume
            self._last_mute = current_mute
            return {"volume": current_volume, "muted": current_mute}
        
        return None


class NotificationMonitor:
    """Windows bildirim izleyici - Pencere başlıklarındaki okunmamış sayısına göre"""
    
    # Bilinen uygulamalar ve pencere başlıklarındaki okunmamış sayı pattern'leri
    TRACKED_APPS = {
        'WhatsApp': {'pattern': r'\((\d+)\)', 'name': 'WhatsApp'},
        'Discord': {'pattern': r'\((\d+)\)', 'name': 'Discord'},
        'Telegram': {'pattern': r'\((\d+)\)', 'name': 'Telegram'},
        'Slack': {'pattern': r'\((\d+)\)', 'name': 'Slack'},
        'Teams': {'pattern': r'\((\d+)\)', 'name': 'Teams'},
        'Messenger': {'pattern': r'\((\d+)\)', 'name': 'Messenger'},
        'Signal': {'pattern': r'\((\d+)\)', 'name': 'Signal'},
        'Phone Link': {'pattern': r'\((\d+)\)', 'name': 'SMS'},  # Windows Phone Link for SMS
        'Your Phone': {'pattern': r'\((\d+)\)', 'name': 'SMS'},  # Eski isim
    }
    
    def __init__(self):
        self._app_unread_counts: Dict[str, int] = {}  # Her app için ayrı sayaç
        self._app_last_notified: Dict[str, float] = {}  # Her app için son bildirim zamanı
        self._last_notification_time: float = 0
        self._hook_active: bool = False
        self._pending_notification: Optional[str] = None  # Tek bir pending bildirim
        self._pending_time: float = 0
        self._init_hook()
        print("[OK] Bildirim izleme aktif")
    
    def _init_hook(self):
        """WinEvent hook başlat (arka planda)"""
        try:
            import threading
            import ctypes
            import ctypes.wintypes as wintypes
            import re
            
            self._user32 = ctypes.windll.user32
            
            # Hook thread'i başlat
            def hook_thread():
                EVENT_OBJECT_NAMECHANGE = 0x800C
                WINEVENT_OUTOFCONTEXT = 0x0000
                
                WinEventProcType = ctypes.WINFUNCTYPE(
                    None, wintypes.HANDLE, wintypes.DWORD, wintypes.HWND,
                    wintypes.LONG, wintypes.LONG, wintypes.DWORD, wintypes.DWORD
                )
                
                def callback(hWinEventHook, event, hwnd, idObject, idChild, dwEventThread, dwmsEventTime):
                    if hwnd:
                        try:
                            # Pencere class'ını al
                            class_buf = ctypes.create_unicode_buffer(256)
                            self._user32.GetClassNameW(hwnd, class_buf, 256)
                            class_name = class_buf.value
                            
                            # Toast/CoreWindow'ları tamamen yoksay - çok fazla false positive
                            if 'CoreWindow' in class_name:
                                return
                            
                            length = self._user32.GetWindowTextLengthW(hwnd)
                            buf = ctypes.create_unicode_buffer(length + 1)
                            self._user32.GetWindowTextW(hwnd, buf, length + 1)
                            title = buf.value
                            
                            if not title:
                                return
                            
                            # Bilinen uygulamaları kontrol et
                            for app_key, app_info in self.TRACKED_APPS.items():
                                if app_key in title:
                                    match = re.search(app_info['pattern'], title)
                                    app_name = app_info['name']
                                    prev_count = self._app_unread_counts.get(app_name, 0)
                                    current_time = time.time()
                                    
                                    if match:
                                        new_count = int(match.group(1))
                                        # Sadece sayı ARTINCA ve son 10 saniyede bu app için bildirim gösterilmediyse
                                        last_notified = self._app_last_notified.get(app_name, 0)
                                        if new_count > prev_count and (current_time - last_notified) > 10:
                                            self._pending_notification = app_name
                                            self._pending_time = current_time
                                            self._app_last_notified[app_name] = current_time
                                        self._app_unread_counts[app_name] = new_count
                                    else:
                                        # Parantez yok = 0 okunmamış (mesajlar okundu)
                                        self._app_unread_counts[app_name] = 0
                                    break
                                
                        except:
                            pass
                
                self._callback = WinEventProcType(callback)
                
                hook = self._user32.SetWinEventHook(
                    EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE,
                    0, self._callback, 0, 0, WINEVENT_OUTOFCONTEXT
                )
                
                if hook:
                    self._hook_active = True
                    msg = wintypes.MSG()
                    while self._hook_active:
                        if self._user32.PeekMessageW(ctypes.byref(msg), 0, 0, 0, 1):
                            self._user32.TranslateMessage(ctypes.byref(msg))
                            self._user32.DispatchMessageW(ctypes.byref(msg))
                        time.sleep(0.01)
                    self._user32.UnhookWinEvent(hook)
            
            thread = threading.Thread(target=hook_thread, daemon=True)
            thread.start()
            time.sleep(0.1)  # Hook'un kurulmasını bekle
            
        except Exception as e:
            print(f"[!] WinEvent hook hatası: {e}")
    
    def check_notification(self) -> Optional[Dict[str, str]]:
        """
        Bildirimleri kontrol et.
        Yeni bildirim varsa {'app': str, 'message': str} döndürür.
        """
        current_time = time.time()
        
        # Bekleyen bildirim var mı?
        if self._pending_notification:
            # Son 3 saniyede bildirim gösterilmediyse göster
            if current_time - self._last_notification_time > 3:
                app_name = self._pending_notification
                self._pending_notification = None
                self._last_notification_time = current_time
                return {"app": app_name, "message": "Yeni Bildirim"}
            else:
                # Cooldown süresi dolmadı, bildirimi sil
                self._pending_notification = None
        
        return None
    
    # Geriye uyumluluk için eski method adını koru
    def check_whatsapp_notification_simple(self) -> Optional[Dict[str, str]]:
        return self.check_notification()
    
    def stop(self):
        """Hook'u durdur"""
        self._hook_active = False


class SystemMonitor:
    """Saat bilgisi"""
    
    def get_clock_display(self) -> Dict[str, str]:
        """OLED için saat (AM/PM) + tarih (AY/GUN/YIL)"""
        now = datetime.now()
        return {
            "time": now.strftime("%I:%M:%S %p"),
            "date": now.strftime("%m/%d/%Y")
        }


class SpotifyMonitor:
    """Sadece Spotify'dan çalan şarkı bilgisi + Progress Bar"""
    
    def __init__(self):
        self._scroll_pos = 0
        self._last_track: str = ""
        self._session_manager = None
        self._last_progress_ms: int = 0
        self._last_duration_ms: int = 0
        self._last_update_time: float = 0
        self._is_playing: bool = False
        self._init_media_session()
    
    def _init_media_session(self):
        """Windows Media Session API'yi başlat (progress için)"""
        try:
            # winrt paketi kullanarak GlobalSystemMediaTransportControlsSessionManager
            from winrt.windows.media.control import GlobalSystemMediaTransportControlsSessionManager
            import asyncio
            
            async def get_manager():
                return await GlobalSystemMediaTransportControlsSessionManager.request_async()
            
            # Event loop oluştur veya mevcut olanı kullan
            try:
                loop = asyncio.get_event_loop()
            except RuntimeError:
                loop = asyncio.new_event_loop()
                asyncio.set_event_loop(loop)
            
            self._session_manager = loop.run_until_complete(get_manager())
            print("[OK] Media Session API aktif (Progress Bar destegi)")
        except ImportError as e:
            print(f"[!] winrt paketi yüklenemedi: {e}")
            print("    Progress bar devre disi, sadece sarki bilgisi gosterilecek")
            self._session_manager = None
        except Exception as e:
            print(f"[!] Media Session hatasi: {e}")
            self._session_manager = None
    
    def get_progress_info(self) -> Optional[Dict[str, any]]:
        """
        Şarkı progress bilgisini döndürür.
        Returns: {
            'progress_ms': int,  # Mevcut pozisyon (milisaniye)
            'duration_ms': int,  # Toplam süre (milisaniye) 
            'progress_percent': float,  # 0-100 arası yüzde
            'is_playing': bool
        }
        
        Bu fonksiyon, Media Session'dan düzenli güncelleme alamıyorsa cache'deki
        son değerleri kullanarak oynatılıyorsa tahmini bir ilerleme hesaplar.
        """
        try_estimate = False
        # Eğer session manager yoksa direkt olarak tahmin dene
        if self._session_manager is None:
            try_estimate = True

        try:
            if not try_estimate:
                session = self._session_manager.get_current_session()
                if session is None:
                    try_estimate = True
                else:
                    # Sadece Spotify session'ı mı kontrol et
                    app_id = session.source_app_user_model_id
                    if app_id and 'spotify' not in app_id.lower():
                        try_estimate = True
                    else:
                        # Timeline bilgisini al
                        timeline = session.get_timeline_properties()
                        playback_info = session.get_playback_info()

                        # Position ve Duration (timedelta objesi -> milisaniye)
                        progress_ms = int(timeline.position.total_seconds() * 1000)
                        duration_ms = int(timeline.end_time.total_seconds() * 1000)

                        # Playback durumu
                        from winrt.windows.media.control import GlobalSystemMediaTransportControlsSessionPlaybackStatus
                        is_playing = playback_info.playback_status == GlobalSystemMediaTransportControlsSessionPlaybackStatus.PLAYING

                        # Geçerli süre kontrolü
                        if duration_ms <= 0:
                            return None

                        now = time.time()
                        min_advance_ms = 400  # küçük artışları yoksay (ms)

                        if self._last_progress_ms is None or progress_ms > self._last_progress_ms + min_advance_ms:
                            # Gerçek ilerleme kabul edilsin
                            progress_percent = min(100.0, (progress_ms / duration_ms) * 100) if duration_ms > 0 else 0
                            self._last_progress_ms = progress_ms
                            self._last_duration_ms = duration_ms
                            self._is_playing = is_playing
                            self._last_update_time = now

                            return {
                                'progress_ms': progress_ms,
                                'duration_ms': duration_ms,
                                'progress_percent': progress_percent,
                                'is_playing': is_playing,
                                'progress_str': self._format_time(progress_ms),
                                'duration_str': self._format_time(duration_ms),
                                'estimated': False
                            }
                        else:
                            # API güncellemesi gecikiyorsa bizim cache ve geçen süre ile tahmin hesapla
                            elapsed = now - getattr(self, '_last_update_time', now)
                            est_progress = min(duration_ms, self._last_progress_ms + int(elapsed * 1000))
                            progress_percent = min(100.0, (est_progress / duration_ms) * 100) if duration_ms > 0 else 0

                            # Cache'i tahmini değere göre güncelle
                            self._last_progress_ms = est_progress
                            self._last_duration_ms = duration_ms
                            self._is_playing = is_playing
                            self._last_update_time = now

                            return {
                                'progress_ms': est_progress,
                                'duration_ms': duration_ms,
                                'progress_percent': progress_percent,
                                'is_playing': is_playing,
                                'progress_str': self._format_time(est_progress),
                                'duration_str': self._format_time(duration_ms),
                                'estimated': True
                            }
        except Exception:
            # Eğer gerçek sorgu başarısız olursa, aşağıda cache üzerinden tahmin denenecek
            try_estimate = True

        # Tahmini güncelleme: son cache ve geçen süreye göre ilerleme hesapla
        # Daha agresif: eğer elimizde geçerli bir duration varsa ve son güncelleme
        # çok uzun geçmiş değilse veya son bilgilere göre oynatılıyorsa tahmin yap.
        if self._last_duration_ms > 0 and (
            self._is_playing or (time.time() - getattr(self, '_last_update_time', 0)) < 30
        ):
            # Eğer _last_update_time hiç set edilmediyse tahmin yapamayız
            if getattr(self, '_last_update_time', 0) == 0:
                return None

            elapsed = time.time() - self._last_update_time
            est_progress = min(self._last_duration_ms, self._last_progress_ms + int(elapsed * 1000))
            progress_percent = min(100.0, (est_progress / self._last_duration_ms) * 100)

            # Cache'i güncelle (bir sonraki döngüde tahmin devam etsin)
            self._last_progress_ms = est_progress
            self._last_update_time = time.time()

            return {
                'progress_ms': est_progress,
                'duration_ms': self._last_duration_ms,
                'progress_percent': progress_percent,
                'is_playing': True,
                'progress_str': self._format_time(est_progress),
                'duration_str': self._format_time(self._last_duration_ms),
                'estimated': True
            }

        return None
    
    def _format_time(self, ms: int) -> str:
        """Milisaniyeyi MM:SS formatına çevir"""
        total_seconds = ms // 1000
        minutes = total_seconds // 60
        seconds = total_seconds % 60
        return f"{minutes}:{seconds:02d}"
    
    def get_current_track(self) -> Optional[Dict[str, str]]:
        """Spotify'dan çalan şarkı bilgisini döndürür"""
        title = self._get_spotify_window_title()
        
        # Boş veya None
        if not title:
            return None
        
        title = title.strip()
        
        # Bunlar çalmıyor demek
        if not title or title in ["Spotify", "Spotify Free", "Spotify Premium"]:
            return None
        
        # Şarkı değişti mi?
        if title != self._last_track:
            self._last_track = title
            self.reset_scroll()
            # Yeni şarkı tespit edildiğinde hemen tahmini süre göstergesini başlat
            # Böylece Media Session geç yanıt verirse bile süre her döngüde artar
            try:
                self._last_progress_ms = 0
                # Eğer önceki duration bilinmiyorsa bırak (0) - tahmin yine ilerler
                self._last_update_time = time.time()
                self._is_playing = True
            except Exception:
                pass
        
        # "Sanatçı - Şarkı" formatını parse et
        if " - " in title:
            parts = title.split(" - ", 1)
            artist = parts[0].strip()
            song = parts[1].strip()
            
            return {
                "artist": artist,
                "title": song
            }
        
        return None
    
    def get_scrolling_text(self, text: str, max_len: int = 16) -> str:
        """Uzun metni kayan yazı yapar"""
        if len(text) <= max_len:
            return text
        
        # Kayan yazı: metin + boşluk + metin şeklinde döngü
        scroll_text = text + "    "
        text_len = len(scroll_text)
        start = self._scroll_pos % text_len
        
        # Döngüsel görünüm için
        display = ""
        for i in range(max_len):
            idx = (start + i) % text_len
            display += scroll_text[idx]
        
        return display
    
    def update_scroll(self):
        """Scroll pozisyonunu 1 ilerlet"""
        self._scroll_pos += 1
    
    def reset_scroll(self):
        """Scroll sıfırla"""
        self._scroll_pos = 0
    
    def _get_spotify_window_title(self) -> Optional[str]:
        """Spotify.exe pencere başlığını alır - sadece Windows"""
        
        # Önce win32gui dene (daha hızlı)
        try:
            import win32gui
            import win32process
            import psutil
            
            result_title = None
            
            def enum_callback(hwnd, _):
                nonlocal result_title
                if not win32gui.IsWindowVisible(hwnd):
                    return True
                try:
                    _, pid = win32process.GetWindowThreadProcessId(hwnd)
                    try:
                        proc = psutil.Process(pid)
                        if proc.name().lower() == "spotify.exe":
                            title = win32gui.GetWindowText(hwnd)
                            if title and title.strip():
                                result_title = title
                                return False  # Durdur
                    except:
                        pass
                except:
                    pass
                return True
            
            win32gui.EnumWindows(enum_callback, None)
            
            if result_title:
                return result_title
        except ImportError:
            pass
        except Exception:
            pass
        
        # Yedek: PowerShell
        try:
            cmd = 'powershell -command "(Get-Process Spotify -ErrorAction SilentlyContinue | Where-Object {$_.MainWindowTitle}).MainWindowTitle"'
            result = subprocess.run(cmd, capture_output=True, text=True, shell=True, timeout=1)
            title = result.stdout.strip()
            if title:
                return title
        except:
            pass
        
        return None
