#pragma once

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "Config.hpp"
#include "GameSenseClient.hpp"
#include "I18n.hpp"
#include "security/CredentialStore.hpp"
#include "monitor/EmailMonitor.hpp"
#include "monitor/NotificationMonitor.hpp"
#include "monitor/SpotifyMonitor.hpp"
#include "monitor/TemperatureMonitor.hpp"
#include "monitor/VolumeMonitor.hpp"
#include "monitor/GameMonitor.hpp"
#include "startup/StartupManager.hpp"
#include "update/UpdateChecker.hpp"
#include "ui/TrayController.hpp"

namespace ssext {

class App {
 public:
  explicit App(HANDLE stop_event = nullptr);

  bool Initialize();
  void Run();
  void RequestStop();

 private:
  enum class OverlayType {
    None,
    Email,
    Notification,
    Volume,
    Language
  };

  void SendClock();
  bool SendSpotifyOrClock(bool allow_clock_fallback);
  void PlayStartupIntro();
  void PlayShutdownOutro();
  std::string BuildGameModeTitle(const std::string& title,
                                 const std::chrono::steady_clock::time_point& now);
  bool SendGameMode();
  void ApplyMediaSourceToggles();
  bool IsNotificationSourceEnabled(const std::string& app_name) const;
  void CheckUpdatesOnStartup();
  void CheckUpdatesPeriodically();
  void CheckEmailCredentialStatus();
  TrayActionStatus CheckForUpdatesManually();
  std::string SyncEmailCredential(const std::string& username,
                                  const std::string& password);
  bool TryApplyUpdate(const UpdateInfo& info);
  void QueueLanguageOverlay(const std::string& title, const std::string& subtitle);
  bool InitializeProfiles();
  bool CreateProfile(const std::string& profile_name);
  bool CopyProfile(const std::string& from_profile_name,
                   const std::string& to_profile_name);
  bool RenameProfile(const std::string& from_profile_name,
                     const std::string& to_profile_name);
  bool DeleteProfile(const std::string& profile_name);
  bool ExportProfile(const std::string& profile_name,
                     const std::string& destination_path);
  bool ImportProfile(const std::string& source_path,
                     const std::string& preferred_profile_name);
  bool SetProfileLocked(const std::string& profile_name, bool locked);
  bool IsProfileLocked(const std::string& profile_name) const;
  std::string BuildProfileCredentialTarget(const std::string& profile_name) const;
  bool SwitchProfile(const std::string& profile_name);
  void SaveActiveProfileConfig();
  bool SaveConfigWithRollback(const std::string& settings_path, const Config& cfg) const;
  bool TestSmtpConnectivity(const TrayEmailSettings& email_settings,
                            std::string& error_message) const;
  TrayProfileSelectionResult BuildTrayProfileSelectionResult(
      bool success,
      const std::string& error_message = {},
      const std::string& info_message = {}) const;

  std::string app_data_dir_;
  std::string profile_state_path_;
  std::string active_profile_name_ = "default";
  std::string active_profile_settings_path_;
  std::vector<std::string> profile_names_;
  HANDLE stop_event_ = nullptr;
  Config config_;
  GameSenseClient client_;
  I18n i18n_;
  CredentialStore credential_store_;
  EmailMonitor email_monitor_;
  SpotifyMonitor spotify_monitor_;
  VolumeMonitor volume_monitor_;
  NotificationMonitor notification_monitor_;
  GameMonitor game_monitor_;
  TemperatureMonitor temperature_monitor_;
  UpdateChecker update_checker_;
  TrayController tray_controller_;
  StartupManager startup_manager_;
  std::optional<std::chrono::steady_clock::time_point> game_started_at_;
  std::string last_game_process_;
  std::string game_marquee_track_key_;
  std::size_t game_marquee_offset_ = 0;
  std::chrono::steady_clock::time_point game_marquee_next_shift_at_{};
  std::chrono::steady_clock::time_point last_update_check_at_{};

  OverlayType overlay_type_ = OverlayType::None;
  std::chrono::steady_clock::time_point overlay_end_time_{};
  std::mutex language_overlay_mutex_;
  bool language_overlay_pending_ = false;
  std::string language_overlay_title_;
  std::string language_overlay_subtitle_;
  std::atomic<bool> restart_requested_{false};
  std::atomic<bool> running_;
};

}  // namespace ssext
