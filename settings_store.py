"""Persistent user settings for SS-EXT."""

import json
import os
from pathlib import Path
from typing import Any, Dict

APP_DIR_NAME = "SSEXT"
SETTINGS_FILE_NAME = "settings.json"


def get_settings_dir() -> Path:
    appdata = os.getenv("APPDATA")
    if appdata:
        return Path(appdata) / APP_DIR_NAME
    return Path.home() / f".{APP_DIR_NAME.lower()}"


def get_settings_path() -> Path:
    return get_settings_dir() / SETTINGS_FILE_NAME


def _ensure_dir() -> Path:
    settings_dir = get_settings_dir()
    settings_dir.mkdir(parents=True, exist_ok=True)
    return settings_dir


def _hide_on_windows(path: Path) -> None:
    if os.name != "nt":
        return
    try:
        import ctypes

        FILE_ATTRIBUTE_HIDDEN = 0x02
        ctypes.windll.kernel32.SetFileAttributesW(str(path), FILE_ATTRIBUTE_HIDDEN)
    except Exception:
        # Hiding is best-effort.
        pass


def load_user_settings() -> Dict[str, Any]:
    path = get_settings_path()
    if not path.exists():
        return {}

    try:
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
            if isinstance(data, dict):
                return data
    except Exception:
        return {}

    return {}


def save_user_settings(data: Dict[str, Any]) -> Path:
    _ensure_dir()
    path = get_settings_path()
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    _hide_on_windows(path)
    _hide_on_windows(path.parent)
    return path
