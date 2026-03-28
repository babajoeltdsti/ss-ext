#include "monitor/SpotifyMonitor.hpp"

#include <Windows.h>
#include <objbase.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/base.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace ssext {

namespace {

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::vector<std::string> SplitUtf8Codepoints(const std::string& text) {
  std::vector<std::string> out;
  out.reserve(text.size());

  std::size_t i = 0;
  while (i < text.size()) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    std::size_t len = 1;
    if ((c & 0x80) == 0x00) {
      len = 1;
    } else if ((c & 0xE0) == 0xC0) {
      len = 2;
    } else if ((c & 0xF0) == 0xE0) {
      len = 3;
    } else if ((c & 0xF8) == 0xF0) {
      len = 4;
    }

    if (i + len > text.size()) {
      len = 1;
    }

    out.emplace_back(text.substr(i, len));
    i += len;
  }

  return out;
}

bool IsSpotifyWindow(HWND hwnd) {
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid == 0) {
    return false;
  }

  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (process == nullptr) {
    return false;
  }

  char path[MAX_PATH] = {0};
  DWORD size = MAX_PATH;
  bool spotify = false;
  if (QueryFullProcessImageNameA(process, 0, path, &size) != 0) {
    std::string exe_path(path, size);
    std::string lower = ToLower(exe_path);
    spotify = lower.rfind("spotify.exe") != std::string::npos;
  }

  CloseHandle(process);
  return spotify;
}

struct SearchContext {
  std::string title;
};

bool ContainsAny(const std::string& haystack, const std::initializer_list<const char*>& needles) {
  for (const char* needle : needles) {
    if (haystack.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool NormalizeTimelineMs(long long raw_position_ms,
                        long long raw_start_ms,
                        long long raw_end_ms,
                        long long raw_min_seek_ms,
                        long long raw_max_seek_ms,
                        long long& out_position_ms,
                        long long& out_duration_ms) {
  const long long dur_from_start = raw_end_ms - raw_start_ms;
  const long long dur_from_seek = raw_max_seek_ms - raw_min_seek_ms;
  const long long dur_from_end = raw_end_ms;
  const long long dur_from_max_seek = raw_max_seek_ms;

  const long long pos_from_start = raw_position_ms - raw_start_ms;
  const long long pos_from_seek = raw_position_ms - raw_min_seek_ms;
  const long long pos_from_zero = raw_position_ms;

  struct Candidate {
    long long position;
    long long duration;
  };

  auto is_valid = [](const Candidate& c) {
    constexpr long long kToleranceMs = 15000;
    return c.duration > 0 && c.position >= 0 && c.position <= c.duration + kToleranceMs;
  };

    // Farkli kaynaklarda seek araligi gercek medya suresinden sapabildigi icin
    // once start/end tabanli adaylar tercih edilir.
  const Candidate preferred[] = {
      {pos_from_start, dur_from_start},
      {pos_from_zero, dur_from_end},
      {pos_from_seek, dur_from_seek},
      {pos_from_zero, dur_from_max_seek},
  };

  for (const auto& c : preferred) {
    if (is_valid(c)) {
      out_position_ms = c.position;
      out_duration_ms = c.duration;
      return true;
    }
  }

  // Son care: pozisyonu sifira clamp'leyip en makul pozitif sureyi sec.
  long long fallback_duration = -1;
  const long long duration_candidates[] = {dur_from_seek, dur_from_start, dur_from_end, dur_from_max_seek};
  for (const long long d : duration_candidates) {
    if (d > 0 && (fallback_duration < 0 || d < fallback_duration)) {
      fallback_duration = d;
    }
  }

  if (fallback_duration <= 0) {
    return false;
  }

  out_duration_ms = fallback_duration;
  out_position_ms = (std::max)(0LL, raw_position_ms);
  out_position_ms = (std::min)(out_position_ms, out_duration_ms);
  return true;
}

void MergeTimelineSample(const std::string& track_key,
                        long long sampled_position_ms,
                        long long sampled_duration_ms,
                        const std::chrono::steady_clock::time_point& now,
                        long long& cached_position_milliseconds,
                        int& cached_duration_seconds,
                        std::string& last_timeline_track_key,
                        std::chrono::steady_clock::time_point& last_timeline_refresh,
                        std::chrono::steady_clock::time_point& last_timeline_sampled_at) {
  long long predicted_position_ms = sampled_position_ms;
  if (cached_position_milliseconds >= 0 &&
      last_timeline_sampled_at != std::chrono::steady_clock::time_point{}) {
    const auto elapsed_since_last_sample_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_timeline_sampled_at)
            .count();
    predicted_position_ms =
        cached_position_milliseconds + (elapsed_since_last_sample_ms > 0 ? elapsed_since_last_sample_ms : 0);
  }

  // Coarse update veren playerlarda akici sayac icin daha toleransli geri-seek siniri kullan.
  const long long diff_ms = sampled_position_ms - predicted_position_ms;
  constexpr long long kForwardSeekThresholdMs = 1500;
  constexpr long long kBackwardSeekThresholdMs = -8000;

  if (diff_ms >= kForwardSeekThresholdMs || diff_ms <= kBackwardSeekThresholdMs) {
    cached_position_milliseconds = sampled_position_ms;
  } else {
    cached_position_milliseconds = (std::max)(predicted_position_ms, sampled_position_ms);
  }

  if (sampled_duration_ms > 0) {
    const int sampled_duration_seconds = static_cast<int>(sampled_duration_ms / 1000LL);
    if (cached_duration_seconds > 0 && track_key == last_timeline_track_key) {
      // Bazi browser oturumlarinda ara sira sisirilmis duration gelebiliyor; ayni parcada min'i koru.
      cached_duration_seconds = (std::min)(cached_duration_seconds, sampled_duration_seconds);
    } else {
      cached_duration_seconds = sampled_duration_seconds;
    }

    const long long clamped_duration_ms = static_cast<long long>(cached_duration_seconds) * 1000LL;
    cached_position_milliseconds = (std::min)(cached_position_milliseconds, clamped_duration_ms);
  }

  last_timeline_track_key = track_key;
  last_timeline_refresh = now;
  last_timeline_sampled_at = now;
}

bool IsPlaybackActive(const winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession& session) {
  try {
    const auto playback = session.GetPlaybackInfo();
    using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus;
    const auto status = playback.PlaybackStatus();
    return status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing ||
           status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Changing;
  } catch (...) {
    return false;
  }
}

bool IsYoutubeMusicContext(const std::string& source_lower,
                          const std::string& title_lower,
                          const std::string& artist_lower,
                          const std::string& album_lower) {
  if (ContainsAny(source_lower, {"youtube.music", "youtubemusic", "music.youtube"})) {
    return true;
  }

  return ContainsAny(title_lower, {"youtube music", "yt music"}) ||
         ContainsAny(artist_lower, {"youtube music", "yt music"}) ||
         ContainsAny(album_lower, {"youtube music", "yt music"});
}

int GetSourcePriority(const std::string& source_lower,
                      const std::string& title_lower,
                      const std::string& artist_lower,
                      const std::string& album_lower,
                      bool spotify_enabled,
                      bool ytm_enabled,
                      bool apple_enabled,
                      bool browser_enabled) {
  if (spotify_enabled && source_lower.find("spotify") != std::string::npos) {
    return 4;
  }

  const bool is_browser = ContainsAny(source_lower, {"chrome", "msedge", "firefox", "opera", "brave"});
  const bool is_youtube_music = IsYoutubeMusicContext(source_lower, title_lower, artist_lower, album_lower);

  const bool is_apple_music =
      ContainsAny(source_lower, {"apple", "itunes", "applemusic", "music.app"}) && !is_browser;

  if (apple_enabled && is_apple_music) {
    return 3;
  }

  if (ytm_enabled && is_youtube_music) {
    return 2;
  }

  if (browser_enabled && is_browser) {
    return 1;
  }

  return 0;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lparam) {
  auto* ctx = reinterpret_cast<SearchContext*>(lparam);
  if (!IsWindowVisible(hwnd) || !IsSpotifyWindow(hwnd)) {
    return TRUE;
  }

  char title[512] = {0};
  const int len = GetWindowTextA(hwnd, title, sizeof(title));
  if (len <= 0) {
    return TRUE;
  }

  std::string value(title, len);
  if (value == "Spotify" || value == "Spotify Free" || value == "Spotify Premium") {
    return TRUE;
  }

  ctx->title = value;
  return FALSE;
}

}  // namespace

void SpotifyMonitor::SetSourceToggles(bool spotify_enabled,
                                      bool youtube_music_enabled,
                                      bool apple_music_enabled,
                                      bool browser_enabled) {
  spotify_enabled_ = spotify_enabled;
  youtube_music_enabled_ = youtube_music_enabled;
  apple_music_enabled_ = apple_music_enabled;
  browser_enabled_ = browser_enabled;
}

void SpotifyMonitor::SetMarqueeShiftIntervalMs(int value_ms) {
  marquee_shift_interval_ms_ = std::clamp(value_ms, 50, 1500);
}

void SpotifyMonitor::SetTimelineRefreshIntervalMs(int value_ms) {
  timeline_refresh_interval_ms_ = std::clamp(value_ms, 100, 5000);
}

bool SpotifyMonitor::TryReadTimeline(long long& out_position_ms, long long& out_duration_ms) {
  out_position_ms = -1;
  out_duration_ms = -1;

  try {
    static bool apartment_checked = false;
    if (!apartment_checked) {
      apartment_checked = true;
      try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
      } catch (const winrt::hresult_error& e) {
        if (e.code() != RPC_E_CHANGED_MODE) {
          return false;
        }
      }
    }

    using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;
    using winrt::Windows::Media::Control::
        GlobalSystemMediaTransportControlsSessionManager;

    const auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    GlobalSystemMediaTransportControlsSession spotify_session = nullptr;

    const auto current_session = manager.GetCurrentSession();
    if (current_session != nullptr) {
      const std::string source = winrt::to_string(current_session.SourceAppUserModelId());
      const std::string source_lower = ToLower(source);
      if (source_lower.find("spotify") != std::string::npos) {
        spotify_session = current_session;
      }
    }

    if (spotify_session == nullptr) {
      const auto sessions = manager.GetSessions();
      for (uint32_t i = 0; i < sessions.Size(); ++i) {
        const auto session = sessions.GetAt(i);
        const std::string source = winrt::to_string(session.SourceAppUserModelId());
        const std::string source_lower = ToLower(source);
        if (source_lower.find("spotify") != std::string::npos) {
          spotify_session = session;
          break;
        }
      }
    }

    if (spotify_session == nullptr) {
      return false;
    }

    const auto timeline = spotify_session.GetTimelineProperties();
    const auto position = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.Position()).count();
    const auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.StartTime()).count();
    const auto end_time = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.EndTime()).count();
    const auto min_seek_time = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.MinSeekTime()).count();
    const auto max_seek_time = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.MaxSeekTime()).count();

    long long position_ms = -1;
    long long duration_ms = -1;
    if (!NormalizeTimelineMs(position, start_time, end_time, min_seek_time, max_seek_time,
                             position_ms, duration_ms)) {
      return false;
    }

    out_position_ms = position_ms;
    out_duration_ms = duration_ms;
    return true;
  } catch (...) {
    return false;
  }
}

std::string SpotifyMonitor::FormatDuration(int position_seconds, int duration_seconds) {
  if (position_seconds < 0 || duration_seconds <= 0) {
    return {};
  }

  const int pos_m = position_seconds / 60;
  const int pos_s = position_seconds % 60;
  const int dur_m = duration_seconds / 60;
  const int dur_s = duration_seconds % 60;

  char text[32] = {0};
  std::snprintf(text, sizeof(text), "%d:%02d/%d:%02d", pos_m, pos_s, dur_m, dur_s);
  return text;
}

std::string SpotifyMonitor::BuildMarqueeTitle(const std::string& title,
                                              const std::chrono::steady_clock::time_point& now) {
  constexpr std::size_t kMaxVisible = 15;
  const auto shift_interval = std::chrono::milliseconds(marquee_shift_interval_ms_);
  const auto glyphs = SplitUtf8Codepoints(title);

  if (glyphs.size() <= kMaxVisible) {
    marquee_offset_ = 0;
    next_marquee_shift_at_ = now;
    return title;
  }

  if (next_marquee_shift_at_ == std::chrono::steady_clock::time_point{}) {
    next_marquee_shift_at_ = now + shift_interval;
  }

  if (now >= next_marquee_shift_at_) {
    const std::size_t cycle_length = glyphs.size() + 3;
    marquee_offset_ = (marquee_offset_ + 1) % cycle_length;
    next_marquee_shift_at_ = now + shift_interval;
  }

  std::vector<std::string> padded = glyphs;
  padded.emplace_back(" ");
  padded.emplace_back(" ");
  padded.emplace_back(" ");
  padded.insert(padded.end(), glyphs.begin(), glyphs.end());

  if (marquee_offset_ + kMaxVisible > padded.size()) {
    marquee_offset_ = 0;
  }

  std::string out;
  out.reserve(kMaxVisible * 2);
  for (std::size_t i = 0; i < kMaxVisible; ++i) {
    out += padded[marquee_offset_ + i];
  }
  return out;
}

SpotifyState SpotifyMonitor::Poll() {
  SpotifyState state;
  std::string detected_title;
  std::string detected_artist;
  bool saw_paused_spotify_session = false;
  bool detected_from_media_session = false;

  try {
    using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;
    using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;

    const auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    GlobalSystemMediaTransportControlsSession selected_session = nullptr;
    int selected_priority = 0;

    auto consider_session = [&](const GlobalSystemMediaTransportControlsSession& session) {
      if (session == nullptr) {
        return;
      }

      const std::string source = ToLower(winrt::to_string(session.SourceAppUserModelId()));
      const auto props = session.TryGetMediaPropertiesAsync().get();
      const std::string title = winrt::to_string(props.Title());
      const std::string artist = winrt::to_string(props.Artist());
      const std::string album = winrt::to_string(props.AlbumTitle());
      const std::string title_lower = ToLower(title);
      const std::string artist_lower = ToLower(artist);
      const std::string album_lower = ToLower(album);

      if (title.empty()) {
        return;
      }

      const int priority = GetSourcePriority(source,
                                             title_lower,
                                             artist_lower,
                                             album_lower,
                                             spotify_enabled_,
                                             youtube_music_enabled_,
                                             apple_music_enabled_,
                                             browser_enabled_);
      if (priority <= 0) {
        return;
      }

      const bool is_spotify_source = source.find("spotify") != std::string::npos;
      const bool is_playing = IsPlaybackActive(session);

      if (!is_playing) {
        if (is_spotify_source && spotify_enabled_) {
          saw_paused_spotify_session = true;
        }
        return;
      }

      if (priority > selected_priority) {
        selected_priority = priority;
        selected_session = session;
      }
    };

    const auto current = manager.GetCurrentSession();
    consider_session(current);

    const auto sessions = manager.GetSessions();
    for (uint32_t i = 0; i < sessions.Size(); ++i) {
      consider_session(sessions.GetAt(i));
    }

    if (selected_session != nullptr) {
      const auto props = selected_session.TryGetMediaPropertiesAsync().get();
      detected_title = winrt::to_string(props.Title());
      detected_artist = winrt::to_string(props.Artist());
      if (detected_artist.empty()) {
        detected_artist = "Media";
      }

      const auto timeline = selected_session.GetTimelineProperties();
      const auto position = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.Position()).count();
      const auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.StartTime()).count();
      const auto end_time = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.EndTime()).count();
      const auto min_seek_time = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.MinSeekTime()).count();
      const auto max_seek_time = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.MaxSeekTime()).count();

      long long position_ms = -1;
      long long duration_ms = -1;
      const bool has_timeline = NormalizeTimelineMs(position, start_time, end_time, min_seek_time,
                                                    max_seek_time, position_ms, duration_ms);

      if (has_timeline) {
        const auto sample_now = std::chrono::steady_clock::now();
        const std::string sample_key = (detected_artist.empty() ? "Media" : detected_artist) + "|" + detected_title;
        MergeTimelineSample(sample_key,
                            position_ms,
                            duration_ms,
                            sample_now,
                            cached_position_milliseconds_,
                            cached_duration_seconds_,
                            last_timeline_track_key_,
                            last_timeline_refresh_,
                            last_timeline_sampled_at_);
      }

      detected_from_media_session = !detected_title.empty();
    }
  } catch (...) {
  }

  if (!detected_from_media_session && spotify_enabled_ && !saw_paused_spotify_session) {
    SearchContext context;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&context));
    if (!context.title.empty()) {
      const std::string separator = " - ";
      const auto sep_pos = context.title.find(separator);
      if (sep_pos != std::string::npos && sep_pos > 0 &&
          sep_pos + separator.size() < context.title.size()) {
        detected_artist = context.title.substr(0, sep_pos);
        detected_title = context.title.substr(sep_pos + separator.size());
      } else {
        detected_title = context.title;
        detected_artist = "Spotify";
      }
    }
  }

  if (detected_title.empty()) {
    last_track_key_.clear();
    last_timeline_track_key_.clear();
    marquee_track_key_.clear();
    duration_fallback_track_key_.clear();
    duration_fallback_started_at_ = std::chrono::steady_clock::time_point{};
    display_position_track_key_.clear();
    last_display_position_seconds_ = -1;
    marquee_offset_ = 0;
    next_marquee_shift_at_ = std::chrono::steady_clock::time_point{};
    cached_position_milliseconds_ = -1;
    cached_duration_seconds_ = -1;
    last_timeline_sampled_at_ = std::chrono::steady_clock::time_point{};
    return state;
  }

  state.active = true;
  state.title = detected_title;
  state.artist = detected_artist.empty() ? "Media" : detected_artist;

  const std::string key = state.artist + "|" + state.title;
  state.changed = key != last_track_key_;
  last_track_key_ = key;

  const auto now = std::chrono::steady_clock::now();

  if (key != duration_fallback_track_key_) {
    duration_fallback_track_key_ = key;
    duration_fallback_started_at_ = now;
  }

  // Yeni parcaya geciste eski parcadan kalan timeline/sure degerlerini tasimayalim.
  if (state.changed) {
    cached_position_milliseconds_ = -1;
    cached_duration_seconds_ = -1;
    last_timeline_track_key_.clear();
    last_timeline_refresh_ = std::chrono::steady_clock::time_point{};
    last_timeline_sampled_at_ = std::chrono::steady_clock::time_point{};
  }

  if (key != marquee_track_key_) {
    marquee_track_key_ = key;
    marquee_offset_ = 0;
    next_marquee_shift_at_ = now;
  }
  state.display_title = BuildMarqueeTitle(state.title, now);
  const bool needs_timeline_refresh =
      !detected_from_media_session &&
      (key != last_timeline_track_key_ ||
       last_timeline_refresh_ == std::chrono::steady_clock::time_point{} ||
       std::chrono::duration_cast<std::chrono::milliseconds>(now - last_timeline_refresh_).count() >=
           timeline_refresh_interval_ms_);

  if (needs_timeline_refresh) {
    long long sampled_position_ms = -1;
    long long sampled_duration_ms = -1;
    if (TryReadTimeline(sampled_position_ms, sampled_duration_ms)) {
      MergeTimelineSample(key,
                          sampled_position_ms,
                          sampled_duration_ms,
                          now,
                          cached_position_milliseconds_,
                          cached_duration_seconds_,
                          last_timeline_track_key_,
                          last_timeline_refresh_,
                          last_timeline_sampled_at_);
    }
  }

  long long effective_position_ms = cached_position_milliseconds_;
  if (effective_position_ms >= 0 && cached_duration_seconds_ > 0 &&
      last_timeline_sampled_at_ != std::chrono::steady_clock::time_point{}) {
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_timeline_sampled_at_)
            .count();
    if (elapsed_ms > 0) {
      effective_position_ms += elapsed_ms;
    }
    const long long duration_ms = static_cast<long long>(cached_duration_seconds_) * 1000LL;
    effective_position_ms = (std::min)(effective_position_ms, duration_ms);
  }

  const int effective_position_seconds =
      (effective_position_ms >= 0) ? static_cast<int>(effective_position_ms / 1000LL) : -1;

  int display_position_seconds = effective_position_seconds;
  if (display_position_seconds >= 0) {
    if (display_position_track_key_ != key) {
      display_position_track_key_ = key;
      last_display_position_seconds_ = display_position_seconds;
    } else if (last_display_position_seconds_ >= 0) {
      const int delta = display_position_seconds - last_display_position_seconds_;

      if (delta >= 0) {
        // Gecikme olmasin: ileri yone dogrudan esitle.
        last_display_position_seconds_ = display_position_seconds;
      } else if (delta <= -5) {
        // Belirgin geri sarma.
        last_display_position_seconds_ = display_position_seconds;
      }
      // Kucuk negatif delta: jitter kabul et, goruntuyu geri oynatma.

      display_position_seconds = last_display_position_seconds_;
    } else {
      last_display_position_seconds_ = display_position_seconds;
    }
  }

  state.position_seconds = display_position_seconds;
  state.duration_seconds = cached_duration_seconds_;
  state.duration_text = FormatDuration(display_position_seconds, cached_duration_seconds_);
  if (state.duration_text.empty() &&
      duration_fallback_started_at_ != std::chrono::steady_clock::time_point{}) {
    const auto elapsed_seconds =
        static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
                             now - duration_fallback_started_at_)
                             .count());
    const int elapsed_m = elapsed_seconds / 60;
    const int elapsed_s = elapsed_seconds % 60;
    char fallback[32] = {0};
    std::snprintf(fallback, sizeof(fallback), "%d:%02d/--:--", elapsed_m, elapsed_s);
    state.duration_text = fallback;
  }
  return state;
}

}  // namespace ssext
