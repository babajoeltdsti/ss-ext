"""
SS-EXT Kapanis Scripti
Calisan uygulamaya SIGTERM gonderir, boylece kapanis animasyonu calisir
"""

import os
import signal
import time


def find_and_stop():
    try:
        import psutil
    except ImportError:
        print("[!] psutil yüklü değil, direkt kapatılıyor...")
        os.system("taskkill /f /im python.exe 2>nul")
        return

    # SS-EXT calisan process'i bul
    found = False
    for proc in psutil.process_iter(["pid", "name", "cmdline"]):
        try:
            cmdline = proc.info.get("cmdline") or []
            cmdline_str = " ".join(cmdline).lower()

            # main.py calistiran python process'i bul
            if "python" in proc.info["name"].lower() and "main.py" in cmdline_str:
                print(f"[*] SS-EXT bulundu (PID: {proc.pid})")
                print("[*] Kapanis animasyonu baslatiliyor...")

                # Windows'ta CTRL_BREAK_EVENT gönder (SIGTERM yerine)
                try:
                    proc.send_signal(signal.CTRL_BREAK_EVENT)
                except Exception:
                    # Alternatif: SIGTERM
                    try:
                        proc.send_signal(signal.SIGTERM)
                    except Exception:
                        proc.terminate()

                found = True

                # Animasyonun bitmesini bekle (max 8 saniye)
                for i in range(16):
                    time.sleep(0.5)
                    if not proc.is_running():
                        print("[OK] Kapatıldı.")
                        return

                # Hala çalışıyorsa zorla kapat
                print("[!] Zorla kapatılıyor...")
                proc.kill()
                print("[OK] Kapatıldı.")
                return

        except (psutil.NoSuchProcess, psutil.AccessDenied):
            continue

    if not found:
        print("[!] SS-EXT calismiyor.")


if __name__ == "__main__":
    find_and_stop()
