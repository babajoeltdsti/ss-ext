"""Simple i18n helpers for SS-EXT."""

import os
from typing import Dict

DEFAULT_LANGUAGE = "en"
SUPPORTED_LANGUAGES = {"en", "tr"}


_MESSAGES: Dict[str, Dict[str, str]] = {
    "en": {
        "app_title": "SS-EXT {version} - SteelSeries OLED",
        "app_running": "Running... (Ctrl+C to stop)",
        "app_closing": "Closing...",
        "app_closed": "Closed.",
        "app_shutdown": "Shutting down...",
        "app_start_failed": "Startup failed!",
        "app_ensure_gg": "Make sure SteelSeries GG is running.",
        "log_new_email": "New Email: {sender} - {subject}",
        "log_new_notification": "{app}: {message}",
        "log_volume": "Volume: {status}",
        "log_game_started": "Game started: {game}",
        "log_game_ended": "Game ended: {game} ({duration})",
        "log_clock_mode": "Clock mode",
        "volume_muted": "MUTED",
        "volume_title": "VOLUME",
        "volume_off": "SOUND OFF",
        "notification_new": "New Message",
        "email_no_subject": "No Subject",
        "email_unknown_sender": "Unknown",
        "email_password_missing": "Email password is not set (SSEXT_EMAIL_PASSWORD)",
        "email_disabled": "Email notifications disabled",
        "email_ready": "Email monitoring ready: {address}",
        "email_monitoring_active": "Email monitoring active",
        "email_settings_load_failed": "Email settings could not be loaded: {error}",
        "email_init_failed": "Email initialization failed: {error}",
        "email_check_failed": "Email check failed: {error}",
        "email_last_uid_failed": "Could not load last email UID: {error}",
        "imap_error": "IMAP error: {error}",
        "server_not_found": "SteelSeries Engine not found!",
        "server_path": "Location: {path}",
        "server_ok": "SteelSeries Engine: {url}",
        "register_ok": "Application: {name}",
        "handlers_ready": "Handlers configured",
        "updater_checking": "Checking for updates...",
        "updater_uptodate": "App is up to date.",
        "updater_found": "New version found: V{version}",
        "updater_download_failed": "Download failed!",
        "updater_download_done": "Download completed!",
        "updater_install_failed": "Update failed!",
        "updater_install_done": "Update completed! New version: V{version}",
        "updater_restart_required": "Update completed! Please restart the app.",
        "updater_exit_soon": "Exiting in 3 seconds...",
        "updater_restart_msg": "Restarting",
        "updater_check_failed": "Update check failed: {error}",
        "updater_flow_failed": "Update process failed: {error}",
        "updater_staging_failed": "EXE staging failed: {error}",
        "updater_exe_asset_missing": "No EXE asset found in release.",
        "update_oled_available_title": "Update",
        "update_oled_available_status": "Available",
        "update_oled_download_title": "Downloading",
        "update_oled_install_title": "Installing",
        "update_oled_done_title": "Update",
        "update_oled_done_status": "Completed",
        "update_oled_failed_title": "Update",
        "update_oled_failed_status": "Failed",
        "update_oled_download_failed_title": "Download",
        "update_oled_download_failed_status": "Failed",
    },
    "tr": {
        "app_title": "SS-EXT {version} - SteelSeries OLED",
        "app_running": "Calisiyor... (Ctrl+C ile kapat)",
        "app_closing": "Kapaniyor...",
        "app_closed": "Kapatildi.",
        "app_shutdown": "Kapatiliyor...",
        "app_start_failed": "Baslatilamadi!",
        "app_ensure_gg": "SteelSeries GG calistiginden emin ol.",
        "log_new_email": "Yeni E-posta: {sender} - {subject}",
        "log_new_notification": "{app}: {message}",
        "log_volume": "Ses: {status}",
        "log_game_started": "Oyun basladi: {game}",
        "log_game_ended": "Oyun bitti: {game} ({duration})",
        "log_clock_mode": "Saat modu",
        "volume_muted": "SESSIZ",
        "volume_title": "SES SEVIYESI",
        "volume_off": "SES KAPALI",
        "notification_new": "Yeni Mesaj",
        "email_no_subject": "Konu Yok",
        "email_unknown_sender": "Bilinmeyen",
        "email_password_missing": "E-posta sifresi ayarlanmamis (SSEXT_EMAIL_PASSWORD)",
        "email_disabled": "E-posta bildirimleri devre disi",
        "email_ready": "E-posta izleme hazir: {address}",
        "email_monitoring_active": "E-posta izleme aktif",
        "email_settings_load_failed": "E-posta ayarlari yuklenemedi: {error}",
        "email_init_failed": "E-posta baslatilamadi: {error}",
        "email_check_failed": "E-posta kontrol hatasi: {error}",
        "email_last_uid_failed": "Son e-posta UID alinamadi: {error}",
        "imap_error": "IMAP hatasi: {error}",
        "server_not_found": "SteelSeries Engine bulunamadi!",
        "server_path": "Konum: {path}",
        "server_ok": "SteelSeries Engine: {url}",
        "register_ok": "Uygulama: {name}",
        "handlers_ready": "Handler'lar ayarlandi",
        "updater_checking": "Guncelleme kontrol ediliyor...",
        "updater_uptodate": "Uygulama guncel.",
        "updater_found": "Yeni surum bulundu: V{version}",
        "updater_download_failed": "Indirme basarisiz!",
        "updater_download_done": "Indirme tamamlandi!",
        "updater_install_failed": "Guncelleme basarisiz!",
        "updater_install_done": "Guncelleme tamamlandi! Yeni surum: V{version}",
        "updater_restart_required": "Guncelleme tamamlandi! Lutfen uygulamayi yeniden baslatin.",
        "updater_exit_soon": "3 saniye icinde cikiliyor...",
        "updater_restart_msg": "Yeniden Baslat",
        "updater_check_failed": "Guncelleme kontrolu basarisiz: {error}",
        "updater_flow_failed": "Guncelleme akisi basarisiz: {error}",
        "updater_staging_failed": "EXE guncelleme hazirlama basarisiz: {error}",
        "updater_exe_asset_missing": "Release icinde EXE asset bulunamadi.",
        "update_oled_available_title": "Guncelleme",
        "update_oled_available_status": "Mevcut",
        "update_oled_download_title": "Indiriliyor",
        "update_oled_install_title": "Kuruluyor",
        "update_oled_done_title": "Guncelleme",
        "update_oled_done_status": "Tamam",
        "update_oled_failed_title": "Guncelleme",
        "update_oled_failed_status": "Basarisiz",
        "update_oled_download_failed_title": "Indirme",
        "update_oled_download_failed_status": "Basarisiz",
    },
}


def normalize_language(lang: str) -> str:
    candidate = (lang or "").strip().lower()
    if candidate in SUPPORTED_LANGUAGES:
        return candidate
    return DEFAULT_LANGUAGE


_CURRENT_LANGUAGE = normalize_language(os.getenv("SSEXT_LANG", DEFAULT_LANGUAGE))


def set_language(lang: str) -> str:
    """Set process language and keep env in sync."""
    global _CURRENT_LANGUAGE
    _CURRENT_LANGUAGE = normalize_language(lang)
    os.environ["SSEXT_LANG"] = _CURRENT_LANGUAGE
    return _CURRENT_LANGUAGE


def get_language() -> str:
    return _CURRENT_LANGUAGE


def t(key: str, **kwargs) -> str:
    lang_table = _MESSAGES.get(_CURRENT_LANGUAGE, _MESSAGES[DEFAULT_LANGUAGE])
    template = lang_table.get(key) or _MESSAGES[DEFAULT_LANGUAGE].get(key) or key
    if kwargs:
        return template.format(**kwargs)
    return template
