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
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

#include "Logger.hpp"

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
    predicted_position_ms = cached_position_milliseconds +
                            (elapsed_since_last_sample_ms > 0 ? elapsed_since_last_sample_ms : 0);
  }

  long long merged_position_ms = sampled_position_ms;
  if (cached_position_milliseconds >= 0 &&
      last_timeline_sampled_at != std::chrono::steady_clock::time_point{}) {
    const long long diff_ms = sampled_position_ms - predicted_position_ms;

    constexpr long long kHardForwardSeekThresholdMs = 4500;
    constexpr long long kHardBackwardSeekThresholdMs = -7000;
    constexpr long long kSoftBackwardJitterMs = -2200;

    if (diff_ms >= kHardForwardSeekThresholdMs || diff_ms <= kHardBackwardSeekThresholdMs) {
      // Gercek seek/atlama durumlarinda orneklenen degeri dogrudan kabul et.
      merged_position_ms = sampled_position_ms;
    } else if (diff_ms >= 0) {
      // Ileri yon farklarini bir anda degil, kademeli toparla.
      merged_position_ms = predicted_position_ms + (diff_ms / 4);
    } else if (diff_ms >= kSoftBackwardJitterMs) {
      // Kucuk geri oynama dalgalanmalarini yoksay.
      merged_position_ms = predicted_position_ms;
    } else {
      // Ciddi geri kayma: muhtemel seek.
      merged_position_ms = sampled_position_ms;
    }
  }

  cached_position_milliseconds = (std::max)(0LL, merged_position_ms);

  if (sampled_duration_ms > 0) {
    const int sampled_duration_seconds =
        static_cast<int>((sampled_duration_ms + 500LL) / 1000LL);
    if (cached_duration_seconds > 0 && track_key == last_timeline_track_key) {
      // Browser tabanli kaynaklarda duration 1-2sn oynayabildigi icin
      // ayni parcada kisa sureli asagi-yonlu sapmalari yoksay.
      const int diff_seconds = sampled_duration_seconds - cached_duration_seconds;
      if (diff_seconds >= 3) {
        cached_duration_seconds += (std::min)(diff_seconds, 2);
      } else if (diff_seconds <= -20) {
        cached_duration_seconds = sampled_duration_seconds;
      }
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

int CalculateSourceConfidence(int source_priority,
                              bool active_playback,
                              const std::string& title,
                              const std::string& artist) {
  int confidence = source_priority * 22;
  if (active_playback) {
    confidence += 12;
  }

  if (!title.empty()) {
    confidence += 8;
  }
  if (!artist.empty()) {
    confidence += 6;
  }

  return std::clamp(confidence, 0, 100);
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
  std::scoped_lock lock(state_mutex_);
  spotify_enabled_ = spotify_enabled;
  youtube_music_enabled_ = youtube_music_enabled;
  apple_music_enabled_ = apple_music_enabled;
  browser_enabled_ = browser_enabled;
  session_list_dirty_ = true;
}

void SpotifyMonitor::SetMarqueeShiftIntervalMs(int value_ms) {
  std::scoped_lock lock(state_mutex_);
  marquee_shift_interval_ms_ = std::clamp(value_ms, 50, 1500);
}

void SpotifyMonitor::SetTimelineRefreshIntervalMs(int value_ms) {
  std::scoped_lock lock(state_mutex_);
  timeline_refresh_interval_ms_ = std::clamp(value_ms, 100, 5000);
}

SpotifyMonitor::~SpotifyMonitor() {
  std::scoped_lock lock(state_mutex_);
  DetachSessionEventsLocked();

  if (manager_events_attached_ && session_manager_ != nullptr) {
    try {
      session_manager_.SessionsChanged(manager_sessions_changed_token_);
    } catch (...) {
    }
    try {
      session_manager_.CurrentSessionChanged(manager_current_changed_token_);
    } catch (...) {
    }
  }

  manager_events_attached_ = false;
  session_manager_ = nullptr;
  manager_ready_ = false;
}

bool SpotifyMonitor::EnsureManagerReady() {
  const auto now = std::chrono::steady_clock::now();

  {
    std::scoped_lock lock(state_mutex_);
    if (manager_ready_ && session_manager_ != nullptr) {
      return true;
    }

    if (next_manager_retry_at_ != std::chrono::steady_clock::time_point{} &&
        now < next_manager_retry_at_) {
      return false;
    }
  }

  static std::once_flag apartment_once;
  static bool apartment_ready = false;
  std::call_once(apartment_once, []() {
    try {
      winrt::init_apartment(winrt::apartment_type::multi_threaded);
      apartment_ready = true;
    } catch (const winrt::hresult_error& e) {
      apartment_ready = (e.code() == RPC_E_CHANGED_MODE);
    } catch (...) {
      apartment_ready = false;
    }
  });

  if (!apartment_ready) {
    std::scoped_lock lock(state_mutex_);
    manager_ready_ = false;
    next_manager_retry_at_ = now + std::chrono::seconds(2);
    return false;
  }

  try {
    using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;
    const auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();

    if (manager == nullptr) {
      throw std::runtime_error("GMTC manager null");
    }

    std::scoped_lock lock(state_mutex_);
    session_manager_ = manager;
    manager_ready_ = true;
    next_manager_retry_at_ = std::chrono::steady_clock::time_point{};
    session_list_dirty_ = true;
    media_properties_dirty_ = true;
    timeline_dirty_ = true;

    if (!manager_events_attached_) {
      manager_sessions_changed_token_ =
          session_manager_.SessionsChanged([this](const auto&, const auto&) {
            std::scoped_lock cb_lock(state_mutex_);
            session_list_dirty_ = true;
          });

      manager_current_changed_token_ =
          session_manager_.CurrentSessionChanged([this](const auto&, const auto&) {
            std::scoped_lock cb_lock(state_mutex_);
            session_list_dirty_ = true;
          });

      manager_events_attached_ = true;
    }

    return true;
  } catch (...) {
    std::scoped_lock lock(state_mutex_);
    manager_ready_ = false;
    next_manager_retry_at_ = now + std::chrono::seconds(2);
    return false;
  }
}

void SpotifyMonitor::ResetMediaCacheLocked() {
  active_source_lower_.clear();
  active_source_confidence_ = 0;
  active_title_.clear();
  active_artist_.clear();
  active_album_.clear();
  active_playback_ = false;

  cached_position_milliseconds_ = -1;
  cached_duration_seconds_ = -1;
  last_timeline_track_key_.clear();
  last_timeline_refresh_ = std::chrono::steady_clock::time_point{};
  last_timeline_sampled_at_ = std::chrono::steady_clock::time_point{};
}

void SpotifyMonitor::DetachSessionEventsLocked() {
  if (!session_events_attached_ || active_session_ == nullptr) {
    session_events_attached_ = false;
    return;
  }

  try {
    active_session_.PlaybackInfoChanged(session_playback_changed_token_);
  } catch (...) {
  }
  try {
    active_session_.MediaPropertiesChanged(session_media_changed_token_);
  } catch (...) {
  }
  try {
    active_session_.TimelinePropertiesChanged(session_timeline_changed_token_);
  } catch (...) {
  }

  session_playback_changed_token_ = {};
  session_media_changed_token_ = {};
  session_timeline_changed_token_ = {};
  session_events_attached_ = false;
}

void SpotifyMonitor::AttachSessionEventsLocked() {
  if (active_session_ == nullptr) {
    return;
  }

  try {
    session_playback_changed_token_ =
        active_session_.PlaybackInfoChanged([this](const auto&, const auto&) {
          std::scoped_lock cb_lock(state_mutex_);
          session_list_dirty_ = true;
          timeline_dirty_ = true;
        });

    session_media_changed_token_ =
        active_session_.MediaPropertiesChanged([this](const auto&, const auto&) {
          std::scoped_lock cb_lock(state_mutex_);
          media_properties_dirty_ = true;
        });

    session_timeline_changed_token_ =
        active_session_.TimelinePropertiesChanged([this](const auto&, const auto&) {
          std::scoped_lock cb_lock(state_mutex_);
          timeline_dirty_ = true;
        });

    session_events_attached_ = true;
  } catch (...) {
    DetachSessionEventsLocked();
  }
}

void SpotifyMonitor::RefreshSessionSelection(const std::chrono::steady_clock::time_point& now) {
  using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;

  winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager manager = nullptr;
  bool spotify_enabled = true;
  bool youtube_music_enabled = true;
  bool apple_music_enabled = true;
  bool browser_enabled = true;

  {
    std::scoped_lock lock(state_mutex_);
    if (!manager_ready_ || session_manager_ == nullptr) {
      return;
    }
    manager = session_manager_;
    spotify_enabled = spotify_enabled_;
    youtube_music_enabled = youtube_music_enabled_;
    apple_music_enabled = apple_music_enabled_;
    browser_enabled = browser_enabled_;
  }

  GlobalSystemMediaTransportControlsSession selected_session = nullptr;
  std::string selected_source;
  std::string selected_title;
  std::string selected_artist;
  std::string selected_album;
  int selected_priority = 0;
  int selected_confidence = 0;
  bool saw_paused_spotify = false;

  auto consider_session = [&](const GlobalSystemMediaTransportControlsSession& session) {
    if (session == nullptr) {
      return;
    }

    std::string source;
    try {
      source = ToLower(winrt::to_string(session.SourceAppUserModelId()));
    } catch (...) {
      return;
    }

    const bool is_spotify_source = source.find("spotify") != std::string::npos;
    const bool is_playing = IsPlaybackActive(session);

    if (!is_playing) {
      if (is_spotify_source && spotify_enabled) {
        saw_paused_spotify = true;
      }
      return;
    }

    std::string title;
    std::string artist;
    std::string album;
    try {
      const auto props = session.TryGetMediaPropertiesAsync().get();
      title = winrt::to_string(props.Title());
      artist = winrt::to_string(props.Artist());
      album = winrt::to_string(props.AlbumTitle());
    } catch (...) {
      return;
    }

    if (title.empty()) {
      return;
    }

    const int priority = GetSourcePriority(source,
                                           ToLower(title),
                                           ToLower(artist),
                                           ToLower(album),
                                           spotify_enabled,
                                           youtube_music_enabled,
                                           apple_music_enabled,
                                           browser_enabled);
    if (priority <= 0) {
      return;
    }

    if (priority > selected_priority) {
      selected_priority = priority;
      selected_confidence = CalculateSourceConfidence(priority, is_playing, title, artist);
      selected_session = session;
      selected_source = source;
      selected_title = title;
      selected_artist = artist;
      selected_album = album;
    }
  };

  try {
    consider_session(manager.GetCurrentSession());
  } catch (...) {
  }

  try {
    const auto sessions = manager.GetSessions();
    for (uint32_t i = 0; i < sessions.Size(); ++i) {
      consider_session(sessions.GetAt(i));
    }
  } catch (...) {
  }

  std::scoped_lock lock(state_mutex_);
  last_session_scan_at_ = now;
  session_list_dirty_ = false;
  saw_paused_spotify_session_ = saw_paused_spotify;

  const bool session_changed = selected_session != active_session_;
  if (session_changed) {
    DetachSessionEventsLocked();
    active_session_ = selected_session;
    if (active_session_ != nullptr) {
      AttachSessionEventsLocked();
    }
    media_properties_dirty_ = true;
    timeline_dirty_ = true;
  }

  if (active_session_ == nullptr) {
    ResetMediaCacheLocked();
    return;
  }

  active_source_lower_ = selected_source;
  active_source_confidence_ = selected_confidence;
  active_title_ = selected_title;
  active_artist_ = selected_artist;
  active_album_ = selected_album;
  active_playback_ = true;
}

void SpotifyMonitor::RefreshActiveSessionMediaProperties() {
  using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;

  GlobalSystemMediaTransportControlsSession session = nullptr;
  {
    std::scoped_lock lock(state_mutex_);
    if (active_session_ == nullptr) {
      return;
    }
    session = active_session_;
  }

  std::string source;
  std::string title;
  std::string artist;
  std::string album;
  bool is_playing = false;

  try {
    source = ToLower(winrt::to_string(session.SourceAppUserModelId()));
    const auto props = session.TryGetMediaPropertiesAsync().get();
    title = winrt::to_string(props.Title());
    artist = winrt::to_string(props.Artist());
    album = winrt::to_string(props.AlbumTitle());
    is_playing = IsPlaybackActive(session);
  } catch (...) {
    return;
  }

  std::scoped_lock lock(state_mutex_);
  if (session != active_session_) {
    return;
  }

  media_properties_dirty_ = false;
  active_source_lower_ = source;
  active_title_ = title;
  active_artist_ = artist;
  active_album_ = album;
  active_playback_ = is_playing;
  active_source_confidence_ =
      CalculateSourceConfidence(
          GetSourcePriority(source,
                            ToLower(title),
                            ToLower(artist),
                            ToLower(album),
                            spotify_enabled_,
                            youtube_music_enabled_,
                            apple_music_enabled_,
                            browser_enabled_),
          is_playing,
          title,
          artist);

  if (!active_playback_) {
    session_list_dirty_ = true;
  }
}

void SpotifyMonitor::RefreshActiveSessionTimeline(const std::chrono::steady_clock::time_point& now) {
  using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;

  GlobalSystemMediaTransportControlsSession session = nullptr;
  std::string track_key;

  {
    std::scoped_lock lock(state_mutex_);
    if (active_session_ == nullptr || !active_playback_) {
      return;
    }

    session = active_session_;
    const std::string timeline_artist = active_artist_.empty() ? "Media" : active_artist_;
    track_key = timeline_artist + "|" + active_title_;
  }

  if (track_key.empty() || track_key == "Media|") {
    return;
  }

  long long position = -1;
  long long start_time = -1;
  long long end_time = -1;
  long long min_seek_time = -1;
  long long max_seek_time = -1;

  try {
    const auto timeline = session.GetTimelineProperties();
    position = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.Position()).count();
    start_time = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.StartTime()).count();
    end_time = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.EndTime()).count();
    min_seek_time = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.MinSeekTime()).count();
    max_seek_time = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.MaxSeekTime()).count();

    try {
      const auto last_updated_sys = winrt::clock::to_sys(timeline.LastUpdatedTime());
      const auto now_sys = std::chrono::system_clock::now();
      const auto age_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(now_sys - last_updated_sys).count();
      if (age_ms > 0) {
        const long long bounded_age_ms = (std::min)(age_ms, 12000LL);
        position += bounded_age_ms;
      }
    } catch (...) {
    }
  } catch (...) {
    return;
  }

  long long position_ms = -1;
  long long duration_ms = -1;
  if (!NormalizeTimelineMs(position,
                           start_time,
                           end_time,
                           min_seek_time,
                           max_seek_time,
                           position_ms,
                           duration_ms)) {
    return;
  }

  std::scoped_lock lock(state_mutex_);
  if (session != active_session_) {
    return;
  }

  timeline_dirty_ = false;
  MergeTimelineSample(track_key,
                      position_ms,
                      duration_ms,
                      now,
                      cached_position_milliseconds_,
                      cached_duration_seconds_,
                      last_timeline_track_key_,
                      last_timeline_refresh_,
                      last_timeline_sampled_at_);
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
  int marquee_shift_interval_ms = 140;
  {
    std::scoped_lock lock(state_mutex_);
    marquee_shift_interval_ms = marquee_shift_interval_ms_;
  }
  const auto shift_interval = std::chrono::milliseconds(marquee_shift_interval_ms);
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
  int detected_confidence = 0;
  bool saw_paused_spotify_session = false;
  bool detected_from_media_session = false;
  bool allow_spotify_fallback = false;
  const auto now = std::chrono::steady_clock::now();

  if (EnsureManagerReady()) {
    bool should_refresh_session_selection = false;
    bool should_refresh_media_properties = false;
    bool should_refresh_timeline = false;

    {
      std::scoped_lock lock(state_mutex_);
      const bool session_scan_stale =
          last_session_scan_at_ == std::chrono::steady_clock::time_point{} ||
          std::chrono::duration_cast<std::chrono::milliseconds>(now - last_session_scan_at_).count() >= 2000;
      should_refresh_session_selection = session_list_dirty_ || session_scan_stale;

      int adaptive_refresh_interval_ms = timeline_refresh_interval_ms_;
      if (active_playback_) {
        if (active_source_lower_.find("spotify") != std::string::npos) {
          adaptive_refresh_interval_ms = (std::max)(150, timeline_refresh_interval_ms_ - 250);
        } else if (active_source_lower_.find("chrome") != std::string::npos ||
                   active_source_lower_.find("msedge") != std::string::npos ||
                   active_source_lower_.find("firefox") != std::string::npos) {
          adaptive_refresh_interval_ms = (std::max)(220, timeline_refresh_interval_ms_ - 80);
        } else {
          adaptive_refresh_interval_ms = (std::max)(180, timeline_refresh_interval_ms_ - 160);
        }
      } else {
        adaptive_refresh_interval_ms = (std::min)(1600, timeline_refresh_interval_ms_ + 750);
      }

      const bool timeline_stale =
          last_timeline_refresh_ == std::chrono::steady_clock::time_point{} ||
          std::chrono::duration_cast<std::chrono::milliseconds>(now - last_timeline_refresh_).count() >=
              adaptive_refresh_interval_ms;

      should_refresh_media_properties = active_session_ != nullptr && media_properties_dirty_;
      should_refresh_timeline = active_session_ != nullptr && (timeline_dirty_ || timeline_stale);
      allow_spotify_fallback = spotify_enabled_;
    }

    if (should_refresh_session_selection) {
      RefreshSessionSelection(now);
    }
    if (should_refresh_media_properties) {
      RefreshActiveSessionMediaProperties();
    }
    if (should_refresh_timeline) {
      RefreshActiveSessionTimeline(now);
    }

    {
      std::scoped_lock lock(state_mutex_);
      saw_paused_spotify_session = saw_paused_spotify_session_;
      if (active_playback_ && !active_title_.empty()) {
        detected_title = active_title_;
        detected_artist = active_artist_.empty() ? "Media" : active_artist_;
        detected_confidence = active_source_confidence_;
        detected_from_media_session = true;
      }
    }
  } else {
    std::scoped_lock lock(state_mutex_);
    allow_spotify_fallback = spotify_enabled_;
  }

  if (!detected_from_media_session && allow_spotify_fallback && !saw_paused_spotify_session) {
    SearchContext context;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&context));
    if (!context.title.empty()) {
      const std::string separator = " - ";
      const auto sep_pos = context.title.find(separator);
      if (sep_pos != std::string::npos && sep_pos > 0 &&
          sep_pos + separator.size() < context.title.size()) {
        detected_artist = context.title.substr(0, sep_pos);
        detected_title = context.title.substr(sep_pos + separator.size());
        detected_confidence = 45;
      } else {
        detected_title = context.title;
        detected_artist = "Spotify";
        detected_confidence = 38;
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
    last_display_position_milliseconds_ = -1;
    last_display_position_sampled_at_ = std::chrono::steady_clock::time_point{};
    marquee_offset_ = 0;
    next_marquee_shift_at_ = std::chrono::steady_clock::time_point{};
    cached_position_milliseconds_ = -1;
    cached_duration_seconds_ = -1;
    last_timeline_sampled_at_ = std::chrono::steady_clock::time_point{};
    drift_accumulated_ms_ = 0;
    drift_max_abs_ms_ = 0;
    drift_samples_ = 0;
    next_drift_log_at_ = std::chrono::steady_clock::time_point{};
    return state;
  }

  state.active = true;
  state.title = detected_title;
  state.artist = detected_artist.empty() ? "Media" : detected_artist;
  state.source_confidence = detected_confidence;

  const std::string key = state.artist + "|" + state.title;
  state.changed = key != last_track_key_;
  last_track_key_ = key;

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
    display_position_track_key_.clear();
    last_display_position_milliseconds_ = -1;
    last_display_position_sampled_at_ = std::chrono::steady_clock::time_point{};
  }

  if (key != marquee_track_key_) {
    marquee_track_key_ = key;
    marquee_offset_ = 0;
    next_marquee_shift_at_ = now;
  }
  state.display_title = BuildMarqueeTitle(state.title, now);

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

  int display_position_seconds = -1;
  if (effective_position_ms >= 0) {
    if (display_position_track_key_ != key || last_display_position_milliseconds_ < 0) {
      display_position_track_key_ = key;
      last_display_position_milliseconds_ = effective_position_ms;
      last_display_position_sampled_at_ = now;
    } else {
      long long expected_position_ms = last_display_position_milliseconds_;
      if (last_display_position_sampled_at_ != std::chrono::steady_clock::time_point{}) {
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    now - last_display_position_sampled_at_)
                                    .count();
        if (elapsed_ms > 0) {
          expected_position_ms += elapsed_ms;
        }
      }

      const long long drift_ms = effective_position_ms - expected_position_ms;
      const long long abs_drift_ms = (drift_ms < 0) ? -drift_ms : drift_ms;
      drift_accumulated_ms_ += abs_drift_ms;
      drift_max_abs_ms_ = (std::max)(drift_max_abs_ms_, abs_drift_ms);
      ++drift_samples_;

      if (next_drift_log_at_ == std::chrono::steady_clock::time_point{}) {
        next_drift_log_at_ = now + std::chrono::seconds(30);
      }

      if (now >= next_drift_log_at_ && drift_samples_ > 0) {
        const long long avg_abs_drift_ms = drift_accumulated_ms_ / drift_samples_;
        Logger::Instance().Log(
            LogLevel::Info,
            "Media drift telemetry: samples=" + std::to_string(drift_samples_) +
                ", avg_abs_ms=" + std::to_string(avg_abs_drift_ms) +
                ", max_abs_ms=" + std::to_string(drift_max_abs_ms_) +
                ", confidence=" + std::to_string(state.source_confidence));

        drift_accumulated_ms_ = 0;
        drift_max_abs_ms_ = 0;
        drift_samples_ = 0;
        next_drift_log_at_ = now + std::chrono::seconds(30);
      }

      constexpr long long kHardForwardSeekMs = 5000;
      constexpr long long kHardBackwardSeekMs = -7000;

      if (drift_ms >= kHardForwardSeekMs || drift_ms <= kHardBackwardSeekMs) {
        last_display_position_milliseconds_ = effective_position_ms;
      } else {
        long long blended_ms = expected_position_ms;
        if (drift_ms > 0) {
          blended_ms += drift_ms / 5;
        } else if (drift_ms < -2500) {
          blended_ms += drift_ms / 6;
        }

        if (blended_ms + 500 < last_display_position_milliseconds_) {
          blended_ms = last_display_position_milliseconds_;
        }

        last_display_position_milliseconds_ = blended_ms;
      }

      last_display_position_sampled_at_ = now;
    }

    if (cached_duration_seconds_ > 0) {
      const long long duration_ms = static_cast<long long>(cached_duration_seconds_) * 1000LL;
      last_display_position_milliseconds_ =
          (std::min)(last_display_position_milliseconds_, duration_ms);
    }

    last_display_position_milliseconds_ = (std::max)(0LL, last_display_position_milliseconds_);
    display_position_seconds =
      static_cast<int>(last_display_position_milliseconds_ / 1000LL);
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
