#pragma once

#include <Windows.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace ssext {

struct TrayEmailSettings {
  std::string imap_server;
  int imap_port = 993;
  bool imap_ssl = true;
  std::string smtp_server;
  int smtp_port = 587;
  bool smtp_ssl = true;
  std::string username;
  int poll_interval_seconds = 10;
};

struct TrayActionStatus {
  bool success = true;
  std::string message;
};

struct TrayProfileSelectionResult {
  bool success = false;
  std::string error_message;
  std::string info_message;
  std::string settings_path;
  bool auto_update_enabled = true;
  bool email_enabled = false;
  bool auto_start_enabled = true;
  bool game_mode_enabled = true;
  bool wp_notifications_enabled = true;
  bool notification_whatsapp_enabled = true;
  bool notification_discord_enabled = true;
  bool notification_instagram_enabled = true;
  bool media_spotify_enabled = true;
  bool media_youtube_music_enabled = true;
  bool media_apple_music_enabled = true;
  bool media_browser_enabled = true;
  bool clock_24h_format = false;
  std::string language = "tr";
  int marquee_shift_ms = 140;
  TrayEmailSettings email_settings;
  std::vector<std::string> profile_names;
  std::string active_profile_name = "default";
  bool active_profile_locked = false;
};

class TrayController {
 public:
  TrayController();
  ~TrayController();

  bool Start(std::function<void()> on_exit_requested,
             std::function<void()> on_restart_requested,
             std::function<void(bool)> on_auto_update_toggled,
             std::function<void(bool)> on_email_toggled,
             std::function<void(bool)> on_auto_start_toggled,
             std::function<void(bool)> on_game_mode_toggled,
             std::function<void(bool)> on_wp_notifications_toggled,
             std::function<void(const std::string&, bool)> on_notification_source_toggled,
             std::function<void(const std::string&, bool)> on_media_source_toggled,
             std::function<void(bool)> on_clock_24h_toggled,
             std::function<void(const std::string&)> on_language_changed,
             std::function<void(int)> on_marquee_speed_changed,
             std::function<TrayActionStatus()> on_manual_update_requested,
             std::function<void(const TrayEmailSettings&)> on_email_settings_changed,
             std::function<std::string(const std::string&, const std::string&)>
               on_email_credential_saved,
             std::function<std::string(const TrayEmailSettings&)> on_email_smtp_test,
             std::function<TrayProfileSelectionResult(const std::string&)> on_profile_selected,
             std::function<TrayProfileSelectionResult(const std::string&)> on_profile_created,
             std::function<TrayProfileSelectionResult(const std::string&, const std::string&)>
               on_profile_copied,
             std::function<TrayProfileSelectionResult(const std::string&, const std::string&)>
               on_profile_renamed,
             std::function<TrayProfileSelectionResult(const std::string&)> on_profile_deleted,
             std::function<TrayProfileSelectionResult(const std::string&, const std::string&)>
               on_profile_exported,
             std::function<TrayProfileSelectionResult(const std::string&, const std::string&)>
               on_profile_imported,
             std::function<TrayProfileSelectionResult(const std::string&, bool)>
               on_profile_lock_toggled,
             std::string settings_path,
             std::string log_path,
             bool auto_update_enabled,
             bool email_enabled,
             bool auto_start_enabled,
             bool game_mode_enabled,
             bool wp_notifications_enabled,
             bool notification_whatsapp_enabled,
             bool notification_discord_enabled,
             bool notification_instagram_enabled,
             bool media_spotify_enabled,
             bool media_youtube_music_enabled,
             bool media_apple_music_enabled,
             bool media_browser_enabled,
             bool clock_24h_format,
             std::string language,
             std::string app_version,
             std::string developer,
             int marquee_shift_ms,
             TrayEmailSettings email_settings,
             std::vector<std::string> profile_names,
             std::string active_profile_name,
             bool active_profile_locked);
  void Stop();

 private:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  void RunLoop();
  void ShowContextMenu();
  void ShowSettingsUi();
  bool InitializeSettingsWebView();
  std::string BuildWebViewStatePayload() const;
  void SyncWebViewState();
  void HandleWebViewMessage(const std::string& message);
  void SyncSettingsWindowState();
  void ApplyProfileResult(const TrayProfileSelectionResult& result,
                          const std::string& fallback_profile_name);
  void SetActiveTab(int tab_index);
  void SetStatusMessage(const std::string& message, bool is_error);
  void SetEmailControlsEnabled(bool enabled);
  void HandleMenuCommand(UINT clicked);
  bool CreateWindowAndIcon();
  void RemoveIcon();

  std::function<void()> on_exit_requested_;
  std::function<void()> on_restart_requested_;
  std::function<void(bool)> on_auto_update_toggled_;
  std::function<void(bool)> on_email_toggled_;
  std::function<void(bool)> on_auto_start_toggled_;
  std::function<void(bool)> on_game_mode_toggled_;
  std::function<void(bool)> on_wp_notifications_toggled_;
  std::function<void(const std::string&, bool)> on_notification_source_toggled_;
  std::function<void(const std::string&, bool)> on_media_source_toggled_;
  std::function<void(bool)> on_clock_24h_toggled_;
  std::function<void(const std::string&)> on_language_changed_;
  std::function<void(int)> on_marquee_speed_changed_;
  std::function<TrayActionStatus()> on_manual_update_requested_;
  std::function<void(const TrayEmailSettings&)> on_email_settings_changed_;
  std::function<std::string(const std::string&, const std::string&)>
      on_email_credential_saved_;
  std::function<std::string(const TrayEmailSettings&)> on_email_smtp_test_;
  std::function<TrayProfileSelectionResult(const std::string&)> on_profile_selected_;
  std::function<TrayProfileSelectionResult(const std::string&)> on_profile_created_;
    std::function<TrayProfileSelectionResult(const std::string&, const std::string&)>
      on_profile_copied_;
  std::function<TrayProfileSelectionResult(const std::string&, const std::string&)>
      on_profile_renamed_;
  std::function<TrayProfileSelectionResult(const std::string&)> on_profile_deleted_;
    std::function<TrayProfileSelectionResult(const std::string&, const std::string&)>
      on_profile_exported_;
    std::function<TrayProfileSelectionResult(const std::string&, const std::string&)>
      on_profile_imported_;
    std::function<TrayProfileSelectionResult(const std::string&, bool)> on_profile_lock_toggled_;
  std::string settings_path_;
  std::string log_path_;
  bool auto_update_enabled_ = true;
  bool email_enabled_ = false;
  bool auto_start_enabled_ = true;
  bool game_mode_enabled_ = true;
  bool wp_notifications_enabled_ = true;
  bool notification_whatsapp_enabled_ = true;
  bool notification_discord_enabled_ = true;
  bool notification_instagram_enabled_ = true;
  bool media_spotify_enabled_ = true;
  bool media_youtube_music_enabled_ = true;
  bool media_apple_music_enabled_ = true;
  bool media_browser_enabled_ = true;
  bool clock_24h_format_ = false;
  std::string language_ = "tr";
  std::string app_version_ = "1.0";
  std::string developer_ = "OMERBABACO";
  int marquee_shift_ms_ = 140;
  TrayEmailSettings email_settings_{};
  std::vector<std::string> profile_names_;
  std::string active_profile_name_ = "default";
  bool active_profile_locked_ = false;
  int active_tab_index_ = 0;
  bool settings_use_webview_ = true;
  bool settings_webview_ready_ = false;
  std::string status_message_;
  bool status_message_is_error_ = false;
  std::atomic<bool> running_;
  std::thread thread_;
  DWORD thread_id_;
  HWND hwnd_;
  HWND settings_hwnd_;
  HICON tray_icon_;
  HFONT settings_font_;
  HFONT settings_title_font_;
  HBRUSH settings_background_brush_;
  NOTIFYICONDATAW nid_;
};

}  // namespace ssext
