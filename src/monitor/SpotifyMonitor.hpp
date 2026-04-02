#pragma once

#include <chrono>
#include <mutex>
#include <string>

#include <winrt/Windows.Media.Control.h>
#include <winrt/base.h>

namespace ssext {

struct SpotifyState {
  bool active = false;
  bool changed = false;
  std::string title;
  std::string display_title;
  std::string artist;
  int position_seconds = -1;
  int duration_seconds = -1;
  std::string duration_text;
  int source_confidence = 0;
};

class SpotifyMonitor {
 public:
  ~SpotifyMonitor();

  void SetSourceToggles(bool spotify_enabled,
                        bool youtube_music_enabled,
                        bool apple_music_enabled,
                        bool browser_enabled);
  void SetMarqueeShiftIntervalMs(int value_ms);
  void SetTimelineRefreshIntervalMs(int value_ms);
  SpotifyState Poll();

 private:
  bool EnsureManagerReady();
  void RefreshSessionSelection(const std::chrono::steady_clock::time_point& now);
  void RefreshActiveSessionMediaProperties();
  void RefreshActiveSessionTimeline(const std::chrono::steady_clock::time_point& now);
  void ResetMediaCacheLocked();
  void DetachSessionEventsLocked();
  void AttachSessionEventsLocked();
  static std::string FormatDuration(int position_seconds, int duration_seconds);
  std::string BuildMarqueeTitle(const std::string& title,
                                const std::chrono::steady_clock::time_point& now);

  std::mutex state_mutex_;
  bool manager_ready_ = false;
  std::chrono::steady_clock::time_point next_manager_retry_at_{};
  std::chrono::steady_clock::time_point last_session_scan_at_{};
  bool session_list_dirty_ = true;
  bool media_properties_dirty_ = true;
  bool timeline_dirty_ = true;
  bool saw_paused_spotify_session_ = false;
  std::string active_source_lower_;
  int active_source_confidence_ = 0;
  std::string active_title_;
  std::string active_artist_;
  std::string active_album_;
  bool active_playback_ = false;
  winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager session_manager_{nullptr};
  winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession active_session_{nullptr};
  winrt::event_token manager_sessions_changed_token_{};
  winrt::event_token manager_current_changed_token_{};
  winrt::event_token session_playback_changed_token_{};
  winrt::event_token session_media_changed_token_{};
  winrt::event_token session_timeline_changed_token_{};
  bool manager_events_attached_ = false;
  bool session_events_attached_ = false;

  std::string last_track_key_;
  std::string last_timeline_track_key_;
  std::string marquee_track_key_;
  std::size_t marquee_offset_ = 0;
  std::chrono::steady_clock::time_point next_marquee_shift_at_{};
  std::string duration_fallback_track_key_;
  std::chrono::steady_clock::time_point duration_fallback_started_at_{};
  std::string display_position_track_key_;
  long long last_display_position_milliseconds_ = -1;
  std::chrono::steady_clock::time_point last_display_position_sampled_at_{};
  int marquee_shift_interval_ms_ = 140;
  int timeline_refresh_interval_ms_ = 700;
  bool spotify_enabled_ = true;
  bool youtube_music_enabled_ = true;
  bool apple_music_enabled_ = true;
  bool browser_enabled_ = true;
  long long cached_position_milliseconds_ = -1;
  int cached_duration_seconds_ = -1;
  std::chrono::steady_clock::time_point last_timeline_refresh_{};
  std::chrono::steady_clock::time_point last_timeline_sampled_at_{};
  long long drift_accumulated_ms_ = 0;
  long long drift_max_abs_ms_ = 0;
  int drift_samples_ = 0;
  std::chrono::steady_clock::time_point next_drift_log_at_{};
};

}  // namespace ssext
