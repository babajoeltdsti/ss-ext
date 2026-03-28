"""Windows startup registration helpers."""

import os
import sys
from pathlib import Path

APP_REG_NAME = "SS-EXT"


def _on_windows() -> bool:
    return os.name == "nt"


def build_startup_command(main_script_path: str) -> str:
    """Build a command that starts SS-EXT in tray mode."""
    if getattr(sys, "frozen", False):
        exe_path = Path(sys.executable)
        return f'"{exe_path}" --tray'

    python_path = Path(sys.executable)
    pythonw = python_path.with_name("pythonw.exe")
    runtime = pythonw if pythonw.exists() else python_path
    script_path = Path(main_script_path).resolve()
    return f'"{runtime}" "{script_path}" --tray'


def is_startup_enabled() -> bool:
    if not _on_windows():
        return False

    try:
        import winreg  # type: ignore

        with winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            r"Software\Microsoft\Windows\CurrentVersion\Run",
            0,
            winreg.KEY_READ,
        ) as key:
            value, _ = winreg.QueryValueEx(key, APP_REG_NAME)
            return bool(value)
    except Exception:
        return False


def set_startup_enabled(enabled: bool, command: str) -> bool:
    if not _on_windows():
        return False

    try:
        import winreg  # type: ignore

        with winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            r"Software\Microsoft\Windows\CurrentVersion\Run",
            0,
            winreg.KEY_SET_VALUE,
        ) as key:
            if enabled:
                winreg.SetValueEx(key, APP_REG_NAME, 0, winreg.REG_SZ, command)
            else:
                try:
                    winreg.DeleteValue(key, APP_REG_NAME)
                except FileNotFoundError:
                    pass
        return True
    except Exception:
        return False
