#pragma once

#include <Windows.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace ssext {

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
             std::function<void(const std::string&, bool)> on_media_source_toggled,
             std::function<void(bool)> on_clock_24h_toggled,
             std::function<void(const std::string&)> on_language_changed,
             std::function<void(int)> on_marquee_speed_changed,
             std::string settings_path,
             std::string log_path,
             bool auto_update_enabled,
             bool email_enabled,
             bool auto_start_enabled,
             bool game_mode_enabled,
             bool media_spotify_enabled,
             bool media_youtube_music_enabled,
             bool media_apple_music_enabled,
             bool media_browser_enabled,
             bool clock_24h_format,
             std::string language,
             int marquee_shift_ms);
  void Stop();

 private:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  void RunLoop();
  void ShowContextMenu();
  void ShowSettingsUi();
  void HandleMenuCommand(UINT clicked);
  bool CreateWindowAndIcon();
  void RemoveIcon();

  std::function<void()> on_exit_requested_;
  std::function<void()> on_restart_requested_;
  std::function<void(bool)> on_auto_update_toggled_;
  std::function<void(bool)> on_email_toggled_;
  std::function<void(bool)> on_auto_start_toggled_;
  std::function<void(bool)> on_game_mode_toggled_;
  std::function<void(const std::string&, bool)> on_media_source_toggled_;
  std::function<void(bool)> on_clock_24h_toggled_;
  std::function<void(const std::string&)> on_language_changed_;
  std::function<void(int)> on_marquee_speed_changed_;
  std::string settings_path_;
  std::string log_path_;
  bool auto_update_enabled_ = true;
  bool email_enabled_ = false;
  bool auto_start_enabled_ = true;
  bool game_mode_enabled_ = true;
  bool media_spotify_enabled_ = true;
  bool media_youtube_music_enabled_ = true;
  bool media_apple_music_enabled_ = true;
  bool media_browser_enabled_ = true;
  bool clock_24h_format_ = false;
  std::string language_ = "tr";
  int marquee_shift_ms_ = 140;
  std::atomic<bool> running_;
  std::thread thread_;
  DWORD thread_id_;
  HWND hwnd_;
  NOTIFYICONDATAW nid_;
};

}  // namespace ssext
