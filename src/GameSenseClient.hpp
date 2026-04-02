#pragma once

#include <Windows.h>
#include <winhttp.h>

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

#include "Config.hpp"

namespace ssext {

class GameSenseClient {
 public:
  explicit GameSenseClient(Config config);
  ~GameSenseClient();
  void SetConfig(const Config& config);

  bool DiscoverServer();
  bool RegisterGame();
  bool SetupClockHandler();
  bool SetupSpotifyHandler();
  bool SetupVolumeHandler();
  bool SetupNotificationHandler();
  bool SetupGameModeHandler();
  bool SetupUpdateHandler();
  bool SetupEmailHandler();
  bool SendClockEvent(const std::string& time_text, const std::string& date_text);
  bool SendSpotifyEvent(const std::string& title, const std::string& artist,
                        const std::string& duration);
  bool SendVolumeEvent(const std::string& title, const std::string& level);
  bool SendNotificationEvent(const std::string& app, const std::string& message);
  bool SendGameModeEvent(const std::string& game, const std::string& info);
  bool SendUpdateEvent(const std::string& title, const std::string& status);
  bool SendEmailEvent(const std::string& subject, const std::string& sender);

 private:
  bool EnsureHttpConnectionLocked();
  void ResetHttpConnectionLocked();
  bool PostJson(const std::string& path, const std::string& body);
  bool PostGameEvent(const std::string& event_name, const std::string& body);
  void TrackHttpLockWaitLocked(long long wait_microseconds,
                               const std::chrono::steady_clock::time_point& now);
  static std::string EscapeJson(const std::string& text);
  static std::string ReadFile(const std::string& path);
  static bool ParseAddressFromCoreProps(const std::string& json, std::string& out_address);

  struct EventCoalesceState {
    std::string last_body;
    std::chrono::steady_clock::time_point last_sent_at{};
  };

  Config config_;
  std::string server_address_;
  std::string server_host_;
  unsigned short server_port_ = 62501;
  std::mutex http_mutex_;
  HINTERNET session_handle_ = nullptr;
  HINTERNET connect_handle_ = nullptr;
  int consecutive_post_failures_ = 0;
  std::chrono::steady_clock::time_point retry_after_{};
  std::chrono::steady_clock::time_point last_failure_log_at_{};
  std::unordered_map<std::string, EventCoalesceState> event_coalesce_cache_;
  long long http_lock_wait_total_us_ = 0;
  int http_lock_wait_samples_ = 0;
  int http_lock_contention_samples_ = 0;
  std::chrono::steady_clock::time_point next_lock_metrics_log_at_{};
};

}  // namespace ssext
