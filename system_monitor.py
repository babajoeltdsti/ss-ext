"""
Sistem Monitörü - Saat, Spotify, Volume, Bildirimler ve E-posta
"""

import imaplib
import email
from email.header import decode_header
import subprocess
import threading
import time
from datetime import datetime
from typing import Dict, List, Optional


class EmailMonitor:
    """IMAP e-posta izleyici - Yeni e-posta bildirimleri için"""

    def __init__(self):
        self._last_uid: Optional[str] = None
        self._pending_emails: List[Dict[str, str]] = []
        self._lock = threading.Lock()
        self._running = False
        self._thread: Optional[threading.Thread] = None
        self._initialized = False
        self._init_email()

    def _init_email(self):
        """E-posta ayarlarını yükle"""
        try:
            from config import (
                EMAIL_ADDRESS,
                EMAIL_PASSWORD,
                IMAP_SERVER,
                IMAP_PORT,
                IMAP_SSL,
                EMAIL_CHECK_INTERVAL,
                EMAIL_NOTIFICATION_ENABLED,
            )

            self._email_address = EMAIL_ADDRESS
            self._email_password = EMAIL_PASSWORD
            self._imap_server = IMAP_SERVER
            self._imap_port = IMAP_PORT
            self._use_ssl = IMAP_SSL
            self._check_interval = EMAIL_CHECK_INTERVAL
            self._enabled = EMAIL_NOTIFICATION_ENABLED

            if not self._email_password:
                print("[!] E-posta sifresi ayarlanmamis (SSEXT_EMAIL_PASSWORD)")
                print("    E-posta bildirimleri devre disi")
                self._enabled = False
                return

            if self._enabled:
                self._initialized = True
                print(f"[OK] E-posta izleme hazir: {self._email_address}")
            else:
                print("[!] E-posta bildirimleri devre disi")

        except ImportError as e:
            print(f"[!] E-posta ayarlari yuklenemedi: {e}")
            self._enabled = False
        except Exception as e:
            print(f"[!] E-posta baslatilamadi: {e}")
            self._enabled = False

    def start(self):
        """E-posta izleme thread'ini başlat"""
        if not self._initialized or not self._enabled:
            return

        self._running = True
        self._thread = threading.Thread(target=self._check_loop, daemon=True)
        self._thread.start()
        print("[OK] E-posta izleme aktif")

    def stop(self):
        """E-posta izleme thread'ini durdur"""
        self._running = False
        if self._thread:
            self._thread.join(timeout=5)

    def _check_loop(self):
        """Arka planda e-posta kontrol döngüsü"""
        # İlk başta mevcut son e-postayı kaydet (bildirim gösterme)
        self._update_last_uid()

        while self._running:
            try:
                self._check_new_emails()
            except Exception as e:
                print(f"[!] E-posta kontrol hatasi: {e}")

            # Belirtilen aralıkta bekle
            for _ in range(self._check_interval):
                if not self._running:
                    break
                time.sleep(1)

    def _update_last_uid(self):
        """Mevcut son e-posta UID'sini güncelle"""
        try:
            if self._use_ssl:
                mail = imaplib.IMAP4_SSL(self._imap_server, self._imap_port)
            else:
                mail = imaplib.IMAP4(self._imap_server, self._imap_port)

            mail.login(self._email_address, self._email_password)
            mail.select("INBOX", readonly=True)

            # Son e-postayı al
            status, messages = mail.search(None, "ALL")
            if status == "OK" and messages[0]:
                mail_ids = messages[0].split()
                if mail_ids:
                    self._last_uid = mail_ids[-1].decode()

            mail.logout()
        except Exception as e:
            print(f"[!] Son e-posta UID alinamadi: {e}")

    def _decode_mime_header(self, header: str) -> str:
        """MIME kodlu başlığı decode et"""
        if not header:
            return ""
        
        decoded_parts = decode_header(header)
        result = []
        for part, encoding in decoded_parts:
            if isinstance(part, bytes):
                try:
                    result.append(part.decode(encoding or 'utf-8', errors='replace'))
                except Exception:
                    result.append(part.decode('utf-8', errors='replace'))
            else:
                result.append(part)
        return ''.join(result)

    def _check_new_emails(self):
        """Yeni e-postaları kontrol et"""
        try:
            if self._use_ssl:
                mail = imaplib.IMAP4_SSL(self._imap_server, self._imap_port)
            else:
                mail = imaplib.IMAP4(self._imap_server, self._imap_port)

            mail.login(self._email_address, self._email_password)
            mail.select("INBOX", readonly=True)

            # Okunmamış e-postaları ara
            status, messages = mail.search(None, "UNSEEN")
            if status != "OK":
                mail.logout()
                return

            mail_ids = messages[0].split()
            if not mail_ids:
                mail.logout()
                return

            # Son bilinen UID'den sonraki e-postaları işle
            for mail_id in mail_ids:
                uid = mail_id.decode()
                
                # Daha önce işlenmişse atla
                if self._last_uid and int(uid) <= int(self._last_uid):
                    continue

                # E-posta detaylarını al
                status, msg_data = mail.fetch(mail_id, "(RFC822.HEADER)")
                if status != "OK":
                    continue

                for response_part in msg_data:
                    if isinstance(response_part, tuple):
                        msg = email.message_from_bytes(response_part[1])
                        
                        # Konu ve göndereni al
                        subject = self._decode_mime_header(msg.get("Subject", ""))
                        from_header = self._decode_mime_header(msg.get("From", ""))
                        
                        # Gönderen adını parse et
                        if "<" in from_header:
                            sender = from_header.split("<")[0].strip().strip('"')
                        else:
                            sender = from_header.split("@")[0] if "@" in from_header else from_header

                        # Bildirimi kuyruğa ekle
                        with self._lock:
                            self._pending_emails.append({
                                "subject": subject or "Konu Yok",
                                "sender": sender or "Bilinmeyen"
                            })

                        self._last_uid = uid

            mail.logout()

        except imaplib.IMAP4.error as e:
            print(f"[!] IMAP hatasi: {e}")
        except Exception as e:
            print(f"[!] E-posta kontrol hatasi: {e}")

    def get_pending_email(self) -> Optional[Dict[str, str]]:
        """Bekleyen e-posta bildirimini al (varsa)"""
        with self._lock:
            if self._pending_emails:
                return self._pending_emails.pop(0)
        return None

    def is_enabled(self) -> bool:
        """E-posta bildirimleri aktif mi?"""
        return self._enabled and self._initialized


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
            if hasattr(speakers, "EndpointVolume"):
                self._endpoint_volume = speakers.EndpointVolume
                print("[OK] Ses izleme aktif (pycaw EndpointVolume)")
                return

            # Eski API dene
            if hasattr(speakers, "Activate"):
                from ctypes import POINTER, cast

                from comtypes import CLSCTX_ALL
                from pycaw.pycaw import IAudioEndpointVolume

                interface = speakers.Activate(
                    IAudioEndpointVolume._iid_, CLSCTX_ALL, None
                )
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
        except Exception:
            pass

        return None
        # Yedek: PowerShell (güçlü decoding ve Unicode normalizasyonu uyguluyoruz)
        try:
            cmd = 'powershell -NoProfile -Command "(Get-Process Spotify -ErrorAction SilentlyContinue | Where-Object {$_.MainWindowTitle}).MainWindowTitle"'
            result = subprocess.run(cmd, capture_output=True, shell=True, timeout=1)
            raw = result.stdout
            title = ""
            if raw:
                if isinstance(raw, (bytes, bytearray)):
                    # Denenecek encoding'ler sırasıyla
                    for enc in ("utf-8", "utf-16-le", "utf-16", "cp1254", "cp1252", "latin-1"):
                        try:
                            title = raw.decode(enc, errors="strict").strip()
                            if title:
                                break
                        except Exception:
                            continue
                    if not title:
                        title = raw.decode("utf-8", errors="replace").strip()
                else:
                    title = str(raw).strip()

            if title:
                # Temizlik ve normalize
                title = title.replace("\x00", "").replace("\r", " ").replace("\n", " ").strip()
                title = unicodedata.normalize("NFC", title)
                if title:
                    return title
        except Exception:
            pass

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
        "WhatsApp": {"pattern": r"\((\d+)\)", "name": "WhatsApp"},
        "Discord": {"pattern": r"\((\d+)\)", "name": "Discord"},
        "Telegram": {"pattern": r"\((\d+)\)", "name": "Telegram"},
        "Slack": {"pattern": r"\((\d+)\)", "name": "Slack"},
        "Teams": {"pattern": r"\((\d+)\)", "name": "Teams"},
        "Messenger": {"pattern": r"\((\d+)\)", "name": "Messenger"},
        "Signal": {"pattern": r"\((\d+)\)", "name": "Signal"},
        "Phone Link": {
            "pattern": r"\((\d+)\)",
            "name": "SMS",
        },  # Windows Phone Link for SMS
        "Your Phone": {"pattern": r"\((\d+)\)", "name": "SMS"},  # Eski isim
    }

    def __init__(self):
        self._app_unread_counts: Dict[str, int] = {}  # Her app için ayrı sayaç
        self._app_last_notified: Dict[str, float] = (
            {}
        )  # Her app için son bildirim zamanı
        self._last_notification_time: float = 0
        self._hook_active: bool = False
        self._pending_notification: Optional[str] = None  # Tek bir pending bildirim
        self._pending_time: float = 0
        self._init_hook()
        print("[OK] Bildirim izleme aktif")

    def _init_hook(self):
        """WinEvent hook başlat (arka planda)"""
        try:
            import ctypes
            import ctypes.wintypes as wintypes
            import re
            import threading

            self._user32 = ctypes.windll.user32

            # Hook thread'i başlat
            def hook_thread():
                EVENT_OBJECT_NAMECHANGE = 0x800C
                WINEVENT_OUTOFCONTEXT = 0x0000

                WinEventProcType = ctypes.WINFUNCTYPE(
                    None,
                    wintypes.HANDLE,
                    wintypes.DWORD,
                    wintypes.HWND,
                    wintypes.LONG,
                    wintypes.LONG,
                    wintypes.DWORD,
                    wintypes.DWORD,
                )

                def callback(
                    hWinEventHook,
                    event,
                    hwnd,
                    idObject,
                    idChild,
                    dwEventThread,
                    dwmsEventTime,
                ):
                    if hwnd:
                        try:
                            # Pencere class'ını al
                            class_buf = ctypes.create_unicode_buffer(256)
                            self._user32.GetClassNameW(hwnd, class_buf, 256)
                            class_name = class_buf.value

                            # Toast/CoreWindow'ları tamamen yoksay - çok fazla false positive
                            if "CoreWindow" in class_name:
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
                                    match = re.search(app_info["pattern"], title)
                                    app_name = app_info["name"]
                                    prev_count = self._app_unread_counts.get(
                                        app_name, 0
                                    )
                                    current_time = time.time()

                                    if match:
                                        new_count = int(match.group(1))
                                        # Sadece sayı ARTINCA ve son 10 saniyede bu app için bildirim gösterilmediyse
                                        last_notified = self._app_last_notified.get(
                                            app_name, 0
                                        )
                                        if (
                                            new_count > prev_count
                                            and (current_time - last_notified) > 10
                                        ):
                                            self._pending_notification = app_name
                                            self._pending_time = current_time
                                            self._app_last_notified[app_name] = (
                                                current_time
                                            )
                                        self._app_unread_counts[app_name] = new_count
                                    else:
                                        # Parantez yok = 0 okunmamış (mesajlar okundu)
                                        self._app_unread_counts[app_name] = 0
                                    break

                        except Exception:
                            pass

                self._callback = WinEventProcType(callback)

                hook = self._user32.SetWinEventHook(
                    EVENT_OBJECT_NAMECHANGE,
                    EVENT_OBJECT_NAMECHANGE,
                    0,
                    self._callback,
                    0,
                    0,
                    WINEVENT_OUTOFCONTEXT,
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
                return {"app": app_name, "message": "Yeni Mesaj"}
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
        return {"time": now.strftime("%I:%M:%S %p"), "date": now.strftime("%m/%d/%Y")}


class SpotifyMonitor:
    """Sadece Spotify'dan çalan şarkı bilgisi + Progress Bar"""

    def __init__(self):
        self._scroll_pos = 0
        self._last_track: str = ""
        self._session_manager = None
        self._last_progress_ms: Optional[int] = None
        self._last_duration_ms: int = 0
        self._last_update_time: float = 0
        self._is_playing: bool = False
        self._init_media_session()

    def _init_media_session(self):
        """Windows Media Session API'yi başlat (progress için)"""
        try:
            # winrt paketi kullanarak GlobalSystemMediaTransportControlsSessionManager
            import asyncio

            from winrt.windows.media.control import (
                GlobalSystemMediaTransportControlsSessionManager,
            )

            async def get_manager():
                return (
                    await GlobalSystemMediaTransportControlsSessionManager.request_async()
                )

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
                    if app_id and "spotify" not in app_id.lower():
                        try_estimate = True
                    else:
                        # Timeline bilgisini al
                        timeline = session.get_timeline_properties()
                        playback_info = session.get_playback_info()

                        # Position ve Duration (timedelta objesi -> milisaniye)
                        progress_ms = int(timeline.position.total_seconds() * 1000)
                        duration_ms = int(timeline.end_time.total_seconds() * 1000)

                        # Playback durumu
                        from winrt.windows.media.control import (
                            GlobalSystemMediaTransportControlsSessionPlaybackStatus,
                        )

                        is_playing = (
                            playback_info.playback_status
                            == GlobalSystemMediaTransportControlsSessionPlaybackStatus.PLAYING
                        )

                        # Geçerli süre kontrolü
                        if duration_ms <= 0:
                            return None

                        # Zaman ve bazı eşikler
                        now = time.time()
                        elapsed_ms = int((now - getattr(self, "_last_update_time", now)) * 1000)
                        # küçük dalgalanmaları yoksayacak, beklenen artıştan daha az geri
                        small_back_ms = 1500
                        # büyük seek'leri kabul etmek için eşik
                        big_forward_ms = 5000

                        # Başlangıçta smoothed yoksa kur
                        if not hasattr(self, "_smoothed_progress_ms") or self._smoothed_progress_ms is None:
                            self._smoothed_progress_ms = float(progress_ms)

                        # Tahmin: önceki smooth + geçen süre (yalnızca oynuyorsa)
                        predicted = self._smoothed_progress_ms + (elapsed_ms if getattr(self, "_is_playing", False) else 0)

                        raw = float(progress_ms)

                        # Eğer küçük bir geri sıçrama ise ve oynatılıyorsa yok say
                        if is_playing and raw < predicted and (predicted - raw) < small_back_ms:
                            raw = predicted

                        # Eğer çok büyük ileri sıçrama varsa (seek), kabul et
                        if raw > predicted + big_forward_ms:
                            smoothed = raw
                        else:
                            # EMA birleştirme: alpha daha yakın takip etsin (duruma göre değiştirilebilir)
                            alpha = 0.6
                            smoothed = predicted * (1.0 - alpha) + raw * alpha

                        # Clamp ve güncelle cache
                        smoothed = max(0.0, min(float(duration_ms), smoothed))
                        self._smoothed_progress_ms = smoothed
                        self._last_progress_ms = int(smoothed)
                        self._last_duration_ms = duration_ms
                        self._is_playing = is_playing
                        self._last_update_time = now

                        # Gösterim için saniye bazlı histerezis (küçük dalgalanmaları engelle)
                        disp_sec = int(self._smoothed_progress_ms) // 1000
                        prev_disp = getattr(self, "_last_display_sec", None)
                        if prev_disp is not None and disp_sec < prev_disp and (prev_disp - disp_sec) < 2:
                            disp_sec = prev_disp
                        self._last_display_sec = disp_sec
                        progress_str = f"{disp_sec//60}:{disp_sec%60:02d}"

                        progress_percent = (
                            min(100.0, (self._smoothed_progress_ms / duration_ms) * 100)
                            if duration_ms > 0
                            else 0
                        )

                        return {
                            "progress_ms": int(self._smoothed_progress_ms),
                            "duration_ms": duration_ms,
                            "progress_percent": progress_percent,
                            "is_playing": is_playing,
                            "progress_str": progress_str,
                            "duration_str": self._format_time(duration_ms),
                            "estimated": False,
                        }
        except Exception:
            # Eğer gerçek sorgu başarısız olursa, aşağıda cache üzerinden tahmin denenecek
            try_estimate = True

        # Tahmini güncelleme: son cache ve geçen süreye göre ilerleme hesapla
        # Daha agresif: eğer elimizde geçerli bir duration varsa ve son güncelleme
        # çok uzun geçmiş değilse veya son bilgilere göre oynatılıyorsa tahmin yap.
        if self._last_duration_ms > 0 and (
            self._is_playing
            or (time.time() - getattr(self, "_last_update_time", 0)) < 30
        ):
            # Eğer _last_update_time hiç set edilmediyse tahmin yapamayız
            if getattr(self, "_last_update_time", 0) == 0:
                return None

            elapsed = time.time() - self._last_update_time
            est_progress = min(
                self._last_duration_ms, self._last_progress_ms + int(elapsed * 1000)
            )
            progress_percent = min(100.0, (est_progress / self._last_duration_ms) * 100)

            # Cache'i güncelle (bir sonraki döngüde tahmin devam etsin)
            self._last_progress_ms = est_progress
            self._last_update_time = time.time()

            disp_sec = est_progress // 1000
            prev_disp = getattr(self, "_last_display_sec", None)
            if prev_disp is not None and disp_sec < prev_disp and (prev_disp - disp_sec) < 2:
                disp_sec = prev_disp
            self._last_display_sec = disp_sec
            progress_str = f"{disp_sec//60}:{disp_sec%60:02d}"

            return {
                "progress_ms": est_progress,
                "duration_ms": self._last_duration_ms,
                "progress_percent": progress_percent,
                "is_playing": True,
                "progress_str": progress_str,
                "duration_str": self._format_time(self._last_duration_ms),
                "estimated": True,
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
        # Temizlik ve normalize et; bazı durumlarda pencereden alınan başlık
        # bozuk karakterler içerebiliyor, normalize ederek tutarlı hale getirelim.
        try:
            title = title.replace("\x00", "").replace("\r", " ").replace("\n", " ").strip()
            title = unicodedata.normalize("NFC", title)
        except Exception:
            pass

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
                # Yeni şarkı tespitinde önceki cache'i sıfırla, böylece
                # bir sonraki gerçek API okuması doğru değeri set eder.
                self._last_progress_ms = None
                self._last_duration_ms = 0
                self._last_update_time = 0
                self._is_playing = True
                # gösterim saniye geçmişini de sıfırla
                self._last_display_sec = None
            except Exception:
                pass

        # "Sanatçı - Şarkı" formatını parse et
        if " - " in title:
            parts = title.split(" - ", 1)
            artist = parts[0].strip()
            song = parts[1].strip()

            return {"artist": artist, "title": song}

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
            import psutil
            import win32gui
            import win32process

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
                    except Exception:
                        pass
                except Exception:
                    pass
                return True

            win32gui.EnumWindows(enum_callback, None)

            if result_title:
                # Temizle ve Unicode normalize et (garbled/encoding problemlerini azaltmak için)
                try:
                    result_title = result_title.replace("\x00", "").replace("\r", " ").replace("\n", " ").strip()
                    result_title = unicodedata.normalize("NFC", result_title)
                except Exception:
                    pass
                return result_title
        except ImportError:
            pass
        except Exception:
            pass

        # Yedek: PowerShell
        try:
            cmd = 'powershell -command "(Get-Process Spotify -ErrorAction SilentlyContinue | Where-Object {$_.MainWindowTitle}).MainWindowTitle"'
            result = subprocess.run(
                cmd, capture_output=True, text=True, shell=True, timeout=1
            )
            title = result.stdout.strip()
            if title:
                return title
        except Exception:
            pass

        return None
