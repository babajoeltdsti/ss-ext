"""System tray runtime for SS-EXT."""

import os
import subprocess
import sys
import threading
from pathlib import Path
from typing import Callable

from i18n import t
from startup_manager import build_startup_command, is_startup_enabled, set_startup_enabled


def _build_icon_image():
    from PIL import Image, ImageDraw  # type: ignore[import]

    size = 64
    image = Image.new("RGB", (size, size), (18, 18, 18))
    draw = ImageDraw.Draw(image)

    draw.rounded_rectangle((6, 6, 58, 58), radius=12, fill=(0, 160, 255))
    draw.rectangle((18, 19, 46, 27), fill=(255, 255, 255))
    draw.rectangle((18, 31, 39, 39), fill=(255, 255, 255))
    draw.rectangle((18, 43, 50, 51), fill=(255, 255, 255))

    return image


def _restart_current_process() -> None:
    if getattr(sys, "frozen", False):
        subprocess.Popen([sys.executable, "--tray"])
        return

    main_script = str(Path(__file__).with_name("main.py"))
    subprocess.Popen([sys.executable, main_script, "--tray"])


def run_with_tray(start_app: Callable[[], None], stop_app: Callable[[], None], open_settings: Callable[[], None]) -> None:
    """Run app + tray icon loop."""
    import pystray  # type: ignore[import]

    app_thread = threading.Thread(target=start_app, daemon=True)
    app_thread.start()

    startup_command = build_startup_command(str(Path(__file__).with_name("main.py")))

    def on_open_settings(icon, item):
        open_settings()

    def on_toggle_startup(icon, item):
        enabled = is_startup_enabled()
        set_startup_enabled(not enabled, startup_command)
        icon.update_menu()

    def startup_checked(item):
        return is_startup_enabled()

    def on_restart(icon, item):
        stop_app()
        icon.stop()
        _restart_current_process()

    def on_exit(icon, item):
        stop_app()
        icon.stop()

    menu = pystray.Menu(
        pystray.MenuItem("Settings", on_open_settings),
        pystray.MenuItem("Start with Windows", on_toggle_startup, checked=startup_checked),
        pystray.MenuItem("Restart", on_restart),
        pystray.MenuItem("Exit", on_exit),
    )

    icon = pystray.Icon("ss-ext", _build_icon_image(), f"SS-EXT ({t('app_running')})", menu)
    icon.run()

    # Allow app thread to finish gracefully after tray exits.
    app_thread.join(timeout=5)
