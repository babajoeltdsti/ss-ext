#pragma once

#include <string>

namespace ssext {

struct Config {
  std::string app_id = "SSEXT";
  std::string app_name = "Carex-Ext";
  std::string language = "tr";
  std::string developer = "OMERBABACO";
  std::string app_version = "1.0";
  std::string update_repo_owner = "babajoeltdsti";
  std::string update_repo_name = "ss-ext-cpp";
  bool auto_update_check = false;
  bool auto_update_apply = true;
  bool auto_update_require_checksum = true;
  int auto_update_check_interval_minutes = 60;
  bool auto_start_task_enabled = true;
  std::string auto_start_task_name = "SS-EXT Cpp";
  bool game_mode_enabled = true;
  bool media_spotify_enabled = true;
  bool media_youtube_music_enabled = true;
  bool media_apple_music_enabled = true;
  bool media_browser_enabled = true;
  bool email_enabled = false;
  int email_check_interval_seconds = 10;
  std::string email_imap_server = "";
  int email_imap_port = 993;
  bool email_imap_ssl = true;
  std::string email_username = "";
  std::string email_credential_target = "SSEXT_CPP_IMAP";
  std::string email_test_subject = "";
  std::string email_test_sender = "";
  int deinitialize_timer_ms = 60000;
  int loop_interval_ms = 200;
  int clock_send_interval_ms = 1000;
  bool clock_24h_format = false;
  int spotify_poll_interval_ms = 100;
  int spotify_marquee_shift_ms = 140;
  int spotify_timeline_refresh_ms = 300;

  static Config Load(const std::string& app_data_dir);
  static void SaveTemplateIfMissing(const std::string& app_data_dir);
  static bool Save(const std::string& app_data_dir, const Config& cfg);
};

}  // namespace ssext
