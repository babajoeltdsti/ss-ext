"""
GG-EXT Kapanış Scripti
Çalışan uygulamaya SIGTERM gönderir, böylece kapanış animasyonu çalışır
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

    # GG-EXT çalışan process'i bul
    found = False
    for proc in psutil.process_iter(["pid", "name", "cmdline"]):
        try:
            cmdline = proc.info.get("cmdline") or []
            cmdline_str = " ".join(cmdline).lower()

            # main.py çalıştıran python process'i bul
            if "python" in proc.info["name"].lower() and "main.py" in cmdline_str:
                print(f"[*] GG-EXT bulundu (PID: {proc.pid})")
                print("[*] Kapanış animasyonu başlatılıyor...")

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
        print("[!] GG-EXT çalışmıyor.")


if __name__ == "__main__":
    find_and_stop()
