#pragma once

#include <chrono>
#include <string>

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
};

class SpotifyMonitor {
 public:
  void SetSourceToggles(bool spotify_enabled,
                        bool youtube_music_enabled,
                        bool apple_music_enabled,
                        bool browser_enabled);
  void SetMarqueeShiftIntervalMs(int value_ms);
  void SetTimelineRefreshIntervalMs(int value_ms);
  SpotifyState Poll();

 private:
    bool TryReadTimeline(long long& out_position_ms, long long& out_duration_ms);
  static std::string FormatDuration(int position_seconds, int duration_seconds);
  std::string BuildMarqueeTitle(const std::string& title,
                                const std::chrono::steady_clock::time_point& now);

  std::string last_track_key_;
  std::string last_timeline_track_key_;
  std::string marquee_track_key_;
  std::size_t marquee_offset_ = 0;
  std::chrono::steady_clock::time_point next_marquee_shift_at_{};
  std::string duration_fallback_track_key_;
  std::chrono::steady_clock::time_point duration_fallback_started_at_{};
  std::string display_position_track_key_;
  int last_display_position_seconds_ = -1;
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
};

}  // namespace ssext
