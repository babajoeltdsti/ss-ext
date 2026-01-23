"""
SS-EXT Otomatik Guncelleme Modulu
GitHub'dan yeni surum kontrolu ve otomatik guncelleme
"""

import json
import os
import shutil
import time
import zipfile
from typing import Callable, Optional, Tuple

import requests

from config import VERSION, GITHUB_REPO_OWNER, GITHUB_REPO_NAME


class AutoUpdater:
    """GitHub'dan otomatik güncelleme yapan sınıf"""

    def __init__(self, oled_callback: Optional[Callable] = None):
        """
        Args:
            oled_callback: OLED ekrana mesaj göndermek için callback fonksiyonu
                          Signature: callback(line1: str, line2: str)
        """
        self.current_version = VERSION
        self.oled_callback = oled_callback
        self.github_api_url = f"https://api.github.com/repos/{GITHUB_REPO_OWNER}/{GITHUB_REPO_NAME}/releases/latest"
        self.session = requests.Session()
        self.session.headers.update({
            "Accept": "application/vnd.github.v3+json",
            "User-Agent": f"SS-EXT/{VERSION}"
        })

    def _show_oled(self, line1: str, line2: str = ""):
        """OLED ekrana mesaj göster"""
        if self.oled_callback:
            self.oled_callback(line1, line2)

    def _compare_versions(self, v1: str, v2: str) -> int:
        """
        Sürüm karşılaştırma
        Returns:
            -1: v1 < v2 (güncelleme var)
             0: v1 == v2 (güncel)
             1: v1 > v2 (daha yeni)
        """
        def parse_version(v: str) -> Tuple[int, ...]:
            # "V1.5.3" veya "1.5.3" formatını destekle
            v = v.lstrip("vV")
            parts = v.split(".")
            return tuple(int(p) for p in parts)

        try:
            v1_parts = parse_version(v1)
            v2_parts = parse_version(v2)

            # Uzunlukları eşitle
            max_len = max(len(v1_parts), len(v2_parts))
            v1_parts = v1_parts + (0,) * (max_len - len(v1_parts))
            v2_parts = v2_parts + (0,) * (max_len - len(v2_parts))

            if v1_parts < v2_parts:
                return -1
            elif v1_parts > v2_parts:
                return 1
            return 0
        except Exception:
            return 0

    def check_for_updates(self) -> Optional[dict]:
        """
        GitHub'dan yeni sürüm kontrolü yapar

        Returns:
            dict: Yeni sürüm bilgileri (tag_name, download_url, body) veya None
        """
        try:
            response = self.session.get(self.github_api_url, timeout=10)
            response.raise_for_status()
            release = response.json()

            latest_version = release.get("tag_name", "").lstrip("vV")
            
            if self._compare_versions(self.current_version, latest_version) < 0:
                # Güncelleme var!
                # zipball_url veya assets'ten zip bul
                download_url = release.get("zipball_url")
                
                # Assets'te zip varsa onu tercih et
                for asset in release.get("assets", []):
                    if asset.get("name", "").endswith(".zip"):
                        download_url = asset.get("browser_download_url")
                        break

                return {
                    "version": latest_version,
                    "download_url": download_url,
                    "release_notes": release.get("body", ""),
                    "html_url": release.get("html_url", "")
                }

            return None  # Güncel

        except requests.exceptions.RequestException as e:
            print(f"[!] Güncelleme kontrolü başarısız: {e}")
            return None
        except Exception as e:
            print(f"[!] Güncelleme hatası: {e}")
            return None

    def download_update(self, download_url: str, progress_callback: Optional[Callable] = None) -> Optional[str]:
        """
        Güncellemeyi indir

        Args:
            download_url: İndirilecek dosyanın URL'si
            progress_callback: İlerleme callback'i - Signature: callback(percent: int)

        Returns:
            str: İndirilen dosyanın yolu veya None
        """
        try:
            # Temp klasörü oluştur
            app_dir = os.path.dirname(os.path.abspath(__file__))
            temp_dir = os.path.join(app_dir, "_update_temp")
            os.makedirs(temp_dir, exist_ok=True)

            zip_path = os.path.join(temp_dir, "update.zip")

            # Streaming download
            response = self.session.get(download_url, stream=True, timeout=60)
            response.raise_for_status()

            total_size = int(response.headers.get("content-length", 0))
            downloaded = 0
            last_percent = -1

            with open(zip_path, "wb") as f:
                for chunk in response.iter_content(chunk_size=8192):
                    if chunk:
                        f.write(chunk)
                        downloaded += len(chunk)

                        if total_size > 0:
                            percent = int((downloaded / total_size) * 100)
                            if percent != last_percent:
                                last_percent = percent
                                if progress_callback:
                                    progress_callback(percent)

            # Eğer total_size bilinmiyorsa %100 olarak işaretle
            if total_size == 0 and progress_callback:
                progress_callback(100)

            return zip_path

        except Exception as e:
            print(f"[!] İndirme hatası: {e}")
            return None

    def apply_update(self, zip_path: str, progress_callback: Optional[Callable] = None) -> bool:
        """
        Güncellemeyi uygula

        Args:
            zip_path: İndirilen zip dosyasının yolu
            progress_callback: İlerleme callback'i

        Returns:
            bool: Başarılı mı?
        """
        try:
            app_dir = os.path.dirname(os.path.abspath(__file__))
            temp_dir = os.path.join(app_dir, "_update_temp")
            extract_dir = os.path.join(temp_dir, "extracted")

            # Zip'i aç
            if progress_callback:
                progress_callback(10)

            with zipfile.ZipFile(zip_path, "r") as zf:
                zf.extractall(extract_dir)

            if progress_callback:
                progress_callback(30)

            # GitHub zipball'da dosyalar bir alt klasorde olur
            # Orn: babajoeltdsti-ss-ext-abc123/
            extracted_contents = os.listdir(extract_dir)
            if len(extracted_contents) == 1 and os.path.isdir(os.path.join(extract_dir, extracted_contents[0])):
                source_dir = os.path.join(extract_dir, extracted_contents[0])
            else:
                source_dir = extract_dir

            if progress_callback:
                progress_callback(50)

            # Güncellenecek dosyalar (bazı dosyaları koruyalım)
            exclude_files = {
                "_update_temp",  # Temp klasörü
                "__pycache__",   # Python cache
                ".git",          # Git klasörü
                "venv",          # Virtual environment
            }

            # Dosyaları kopyala
            files = os.listdir(source_dir)
            total_files = len(files)

            for i, item in enumerate(files):
                if item in exclude_files:
                    continue

                src = os.path.join(source_dir, item)
                dst = os.path.join(app_dir, item)

                # Eski dosyayı/klasörü sil
                if os.path.exists(dst):
                    if os.path.isdir(dst):
                        shutil.rmtree(dst)
                    else:
                        os.remove(dst)

                # Yeni dosyayı kopyala
                if os.path.isdir(src):
                    shutil.copytree(src, dst)
                else:
                    shutil.copy2(src, dst)

                if progress_callback:
                    percent = 50 + int((i + 1) / total_files * 40)
                    progress_callback(percent)

            if progress_callback:
                progress_callback(95)

            # Temp klasörünü temizle
            try:
                shutil.rmtree(temp_dir)
            except Exception:
                pass  # Temizleme başarısız olursa da sorun değil

            if progress_callback:
                progress_callback(100)

            return True

        except Exception as e:
            print(f"[!] Güncelleme uygulama hatası: {e}")
            return False

    def update_with_oled_display(self, gamesense_client) -> bool:
        """
        Tam güncelleme akışı - OLED ekranda gösterimli

        Args:
            gamesense_client: GameSenseClient instance'ı

        Returns:
            bool: Güncelleme yapıldı mı?
        """
        print("[*] Güncelleme kontrol ediliyor...")

        # Güncelleme kontrolü
        update_info = self.check_for_updates()

        if not update_info:
            print("[OK] Uygulama güncel.")
            return False

        new_version = update_info["version"]
        print(f"[!] Yeni sürüm bulundu: V{new_version}")

        # OLED'de "Güncelleme Mevcut!" göster
        gamesense_client.send_update_message("Guncelleme", "   Mevcut!")
        time.sleep(2)

        # İndirme başlıyor
        def show_download_progress(percent: int):
            bar = self._create_progress_bar(percent)
            gamesense_client.send_update_progress("Indiriliyor", bar, percent)
            print(f"\r[↓] İndiriliyor: {percent}%", end="", flush=True)

        print()  # Yeni satır

        zip_path = self.download_update(
            update_info["download_url"],
            progress_callback=show_download_progress
        )

        if not zip_path:
            gamesense_client.send_update_message("Indirme", " Basarisiz!")
            time.sleep(2)
            return False

        print("\n[OK] İndirme tamamlandı!")

        # Kurulum başlıyor
        def show_install_progress(percent: int):
            bar = self._create_progress_bar(percent)
            gamesense_client.send_update_progress("Kuruluyor", bar, percent)
            print(f"\r[↻] Kuruluyor: {percent}%", end="", flush=True)

        success = self.apply_update(zip_path, progress_callback=show_install_progress)

        print()  # Yeni satır

        if success:
            print(f"[OK] Güncelleme tamamlandı! Yeni sürüm: V{new_version}")
            gamesense_client.send_update_message("Guncelleme", "  Tamam!")
            time.sleep(2)
            gamesense_client.send_update_message("Yeniden", " Baslatiliyor")
            time.sleep(1)
            return True
        else:
            print("[X] Güncelleme başarısız!")
            gamesense_client.send_update_message("Guncelleme", " Basarisiz!")
            time.sleep(2)
            return False

    def _create_progress_bar(self, percent: int, width: int = 8) -> str:
        """
        Progress bar oluştur

        Args:
            percent: Yüzde (0-100)
            width: Bar genişliği

        Returns:
            str: "████░░░░" formatında bar
        """
        filled = int(percent / 100 * width)
        bar = "█" * filled + "░" * (width - filled)
        return bar


def check_and_update(gamesense_client) -> bool:
    """
    Kısayol fonksiyon - güncelleme kontrol et ve varsa uygula

    Args:
        gamesense_client: GameSenseClient instance'ı

    Returns:
        bool: Güncelleme yapıldı mı? (True ise uygulama yeniden başlatılmalı)
    """
    updater = AutoUpdater()
    return updater.update_with_oled_display(gamesense_client)
