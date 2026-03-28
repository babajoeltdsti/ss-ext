"""
SS-EXT Otomatik Guncelleme Modulu
GitHub'dan yeni surum kontrolu ve otomatik guncelleme
"""

import os
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile
from typing import Callable, Optional, Tuple

import requests

from config import VERSION, GITHUB_REPO_OWNER, GITHUB_REPO_NAME
from i18n import t


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

    @staticmethod
    def _is_frozen() -> bool:
        return bool(getattr(sys, "frozen", False))

    @staticmethod
    def _running_executable() -> str:
        if AutoUpdater._is_frozen():
            return os.path.abspath(sys.executable)
        return os.path.abspath(__file__)

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
                assets = []

                # Assets'te zip varsa onu tercih et
                for asset in release.get("assets", []):
                    name = asset.get("name", "")
                    url = asset.get("browser_download_url")
                    if name and url:
                        assets.append({"name": name, "download_url": url})
                    if asset.get("name", "").endswith(".zip"):
                        download_url = asset.get("browser_download_url")
                        break

                return {
                    "version": latest_version,
                    "download_url": download_url,
                    "assets": assets,
                    "release_notes": release.get("body", ""),
                    "html_url": release.get("html_url", "")
                }

            return None  # Güncel

        except requests.exceptions.RequestException as e:
            print(f"[!] {t('updater_check_failed', error=e)}")
            return None
        except Exception as e:
            print(f"[!] {t('updater_check_failed', error=e)}")
            return None

    def download_update(
        self,
        download_url: str,
        progress_callback: Optional[Callable] = None,
        target_name: str = "update.zip",
        target_dir: Optional[str] = None,
    ) -> Optional[str]:
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
            app_dir = os.path.dirname(self._running_executable())
            temp_dir = target_dir or os.path.join(app_dir, "_update_temp")
            os.makedirs(temp_dir, exist_ok=True)

            zip_path = os.path.join(temp_dir, target_name)

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
            print(f"[!] {t('updater_download_failed')} ({e})")
            return None

    def _find_exe_asset(self, assets: list) -> Optional[dict]:
        if not assets:
            return None

        preferred_keywords = ("win", "windows", "x64")
        for asset in assets:
            name = asset.get("name", "").lower()
            if name.endswith(".exe") and any(k in name for k in preferred_keywords):
                return asset

        for asset in assets:
            name = asset.get("name", "").lower()
            if name.endswith(".exe"):
                return asset

        return None

    def _stage_exe_replacement(self, new_exe_path: str) -> bool:
        """Create a detached updater .bat to replace running EXE after process exits."""
        try:
            current_exe = self._running_executable()
            pid = os.getpid()
            script_path = os.path.join(tempfile.gettempdir(), f"ssext_apply_update_{pid}.bat")

            script_lines = [
                "@echo off",
                "setlocal",
                ":wait_loop",
                f'tasklist /FI "PID eq {pid}" | find "{pid}" >nul',
                "if %ERRORLEVEL%==0 (",
                "  timeout /t 1 /nobreak >nul",
                "  goto wait_loop",
                ")",
                f'copy /Y "{new_exe_path}" "{current_exe}" >nul',
                f'start "" "{current_exe}" --updated',
                "endlocal",
            ]

            with open(script_path, "w", encoding="utf-8") as f:
                f.write("\r\n".join(script_lines) + "\r\n")

            create_no_window = getattr(subprocess, "CREATE_NO_WINDOW", 0)
            subprocess.Popen(
                f'start "" "{script_path}"',
                shell=True,
                creationflags=create_no_window,
            )
            return True
        except Exception as e:
            print(f"[!] {t('updater_staging_failed', error=e)}")
            return False

    def _update_frozen_executable(self, update_info: dict, progress_callback: Optional[Callable] = None) -> bool:
        assets = update_info.get("assets", [])
        exe_asset = self._find_exe_asset(assets)

        if not exe_asset:
            print(f"[!] {t('updater_exe_asset_missing')}")
            return False

        target_dir = tempfile.mkdtemp(prefix="ssext_update_")
        downloaded_exe = self.download_update(
            exe_asset["download_url"],
            progress_callback=progress_callback,
            target_name=exe_asset["name"],
            target_dir=target_dir,
        )

        if not downloaded_exe:
            return False

        return self._stage_exe_replacement(downloaded_exe)

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
            print(f"[!] {t('updater_flow_failed', error=e)}")
            return False

    def update_with_oled_display(self, gamesense_client) -> bool:
        """
        Tam güncelleme akışı - OLED ekranda gösterimli

        Args:
            gamesense_client: GameSenseClient instance'ı

        Returns:
            bool: Güncelleme yapıldı mı?
        """
        print(f"[*] {t('updater_checking')}")

        # Güncelleme kontrolü
        update_info = self.check_for_updates()

        if not update_info:
            print(f"[OK] {t('updater_uptodate')}")
            return False

        new_version = update_info["version"]
        print(f"[!] {t('updater_found', version=new_version)}")

        # OLED'de "Güncelleme Mevcut!" göster
        gamesense_client.send_update_message(t("update_oled_available_title"), t("update_oled_available_status"))
        time.sleep(2)

        # İndirme başlıyor
        def show_download_progress(percent: int):
            bar = self._create_progress_bar(percent)
            gamesense_client.send_update_progress(t("update_oled_download_title"), bar, percent)
            print(f"\r[↓] {t('update_oled_download_title')}: {percent}%", end="", flush=True)

        print()  # Yeni satır

        if self._is_frozen():
            success = self._update_frozen_executable(update_info, progress_callback=show_download_progress)
            print()
            if success:
                print(f"[OK] {t('updater_install_done', version=new_version)}")
                gamesense_client.send_update_message(t("update_oled_done_title"), t("update_oled_done_status"))
                time.sleep(1)
                gamesense_client.send_update_message(t("updater_restart_msg"), "")
                time.sleep(1)
                return True

            print(f"[X] {t('updater_install_failed')}")
            gamesense_client.send_update_message(t("update_oled_failed_title"), t("update_oled_failed_status"))
            time.sleep(2)
            return False

        zip_path = self.download_update(update_info["download_url"], progress_callback=show_download_progress)

        if not zip_path:
            gamesense_client.send_update_message(
                t("update_oled_download_failed_title"),
                t("update_oled_download_failed_status"),
            )
            time.sleep(2)
            return False

        print(f"\n[OK] {t('updater_download_done')}")

        # Kurulum başlıyor
        def show_install_progress(percent: int):
            bar = self._create_progress_bar(percent)
            gamesense_client.send_update_progress(t("update_oled_install_title"), bar, percent)
            print(f"\r[↻] {t('update_oled_install_title')}: {percent}%", end="", flush=True)

        success = self.apply_update(zip_path, progress_callback=show_install_progress)

        print()  # Yeni satır

        if success:
            print(f"[OK] {t('updater_install_done', version=new_version)}")
            gamesense_client.send_update_message(t("update_oled_done_title"), t("update_oled_done_status"))
            time.sleep(2)
            gamesense_client.send_update_message(t("updater_restart_msg"), "")
            time.sleep(1)
            return True
        else:
            print(f"[X] {t('updater_install_failed')}")
            gamesense_client.send_update_message(t("update_oled_failed_title"), t("update_oled_failed_status"))
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
