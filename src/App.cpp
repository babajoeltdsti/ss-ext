#include "App.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>

#include "Logger.hpp"
#include "PathUtils.hpp"
#include "security/SSFileHash.hpp"

namespace ssext {

namespace {

std::string NowTimeText(bool use_24h) {
  std::time_t t = std::time(nullptr);
  std::tm tm{};
  localtime_s(&tm, &t);

  std::ostringstream oss;
  if (use_24h) {
    oss << std::put_time(&tm, "%H:%M:%S");
  } else {
    oss << std::put_time(&tm, "%I:%M:%S %p");
  }
  return oss.str();
}

std::string NowDateText() {
  std::time_t t = std::time(nullptr);
  std::tm tm{};
  localtime_s(&tm, &t);

  std::ostringstream oss;
  oss << std::put_time(&tm, "%m/%d/%Y");
  return oss.str();
}

std::string NormalizeDisplayVersion(const std::string& version) {
  if (version.empty()) {
    return "1.0";
  }

  const auto first_dot = version.find('.');
  if (first_dot == std::string::npos) {
    return version + ".0";
  }

  const auto second_dot = version.find('.', first_dot + 1);
  if (second_dot == std::string::npos) {
    return version;
  }

  return version.substr(0, second_dot);
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

}  // namespace

void App::PlayStartupIntro() {
  const std::string version_label = (config_.language == "en") ? "Version" : "Sürüm";
  const std::string version_text = version_label + " v" + NormalizeDisplayVersion(config_.app_version);

  // 3 saniyelik acilis animasyonu ve surum bilgisi goster.
  const std::vector<std::string> frames = {
      "Carex-Ext", "Carex-Ext.", "Carex-Ext..", "Carex-Ext...", "Carex-Ext..", "Carex-Ext."};
  for (const auto& frame : frames) {
    client_.SendClockEvent(frame, version_text);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  client_.SendUpdateEvent("Carex-Ext", version_text);
}

void App::PlayShutdownOutro() {
  const std::string bye = (config_.language == "en") ? "Shutting down" : "Kapatılıyor";
  const std::string subtitle = "Carex-Ext";

  const std::vector<std::string> frames = {
      bye + "...", bye + "..", bye + ".", bye};

  for (const auto& frame : frames) {
    client_.SendUpdateEvent(frame, subtitle);
    std::this_thread::sleep_for(std::chrono::milliseconds(180));
  }

  client_.SendUpdateEvent("", "");
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
}

std::string App::BuildGameModeTitle(const std::string& title,
                                    const std::chrono::steady_clock::time_point& now) {
  constexpr std::size_t kMaxVisible = 16;
  constexpr auto kShiftInterval = std::chrono::milliseconds(220);
  const auto glyphs = SplitUtf8Codepoints(title);

  if (glyphs.size() <= kMaxVisible) {
    game_marquee_offset_ = 0;
    game_marquee_next_shift_at_ = now;
    return title;
  }

  if (game_marquee_track_key_ != title) {
    game_marquee_track_key_ = title;
    game_marquee_offset_ = 0;
    game_marquee_next_shift_at_ = now + kShiftInterval;
  }

  if (game_marquee_next_shift_at_ == std::chrono::steady_clock::time_point{}) {
    game_marquee_next_shift_at_ = now + kShiftInterval;
  }

  if (now >= game_marquee_next_shift_at_) {
    const std::size_t cycle_len = glyphs.size() + 3;
    game_marquee_offset_ = (game_marquee_offset_ + 1) % cycle_len;
    game_marquee_next_shift_at_ = now + kShiftInterval;
  }

  std::vector<std::string> padded = glyphs;
  padded.emplace_back(" ");
  padded.emplace_back(" ");
  padded.emplace_back(" ");
  padded.insert(padded.end(), glyphs.begin(), glyphs.end());

  if (game_marquee_offset_ + kMaxVisible > padded.size()) {
    game_marquee_offset_ = 0;
  }

  std::string out;
  out.reserve(kMaxVisible * 2);
  for (std::size_t i = 0; i < kMaxVisible; ++i) {
    out += padded[game_marquee_offset_ + i];
  }
  return out;
}

void App::ApplyMediaSourceToggles() {
  spotify_monitor_.SetSourceToggles(
      config_.media_spotify_enabled,
      config_.media_youtube_music_enabled,
      config_.media_apple_music_enabled,
      config_.media_browser_enabled);
}

void App::QueueLanguageOverlay(const std::string& title, const std::string& subtitle) {
  std::lock_guard<std::mutex> lock(language_overlay_mutex_);
  language_overlay_title_ = title;
  language_overlay_subtitle_ = subtitle;
  language_overlay_pending_ = true;
}

App::App(HANDLE stop_event)
    : app_data_dir_{},
      stop_event_(stop_event),
      config_{},
      client_(config_),
    i18n_{},
    credential_store_{},
    email_monitor_(config_),
      running_(false) {}

bool App::Initialize() {
  app_data_dir_ = GetAppDataDirectory();
  if (app_data_dir_.empty()) {
    Logger::Instance().Log(LogLevel::Warning,
                           "%APPDATA% yolu alinamadi, yalnizca konsol log kullanilacak.");
  } else {
    const std::string log_path = JoinPath(app_data_dir_, "app.log");
    Logger::Instance().Initialize(log_path);
  }

  if (!app_data_dir_.empty()) {
    Config::SaveTemplateIfMissing(app_data_dir_);
    config_ = Config::Load(app_data_dir_);

    if (config_.update_repo_owner == "babajoeltdsti" && config_.update_repo_name == "ss-ext") {
      config_.update_repo_name = "ss-ext-cpp";
      config_.auto_update_check = false;
      Logger::Instance().Log(
          LogLevel::Info,
          "Legacy update repo ayari algilandi. Yanlis update bildirimi engellendi.");
    }

    config_.app_version = NormalizeDisplayVersion(config_.app_version);

    client_ = GameSenseClient(config_);
    email_monitor_ = EmailMonitor(config_);
      ApplyMediaSourceToggles();
    spotify_monitor_.SetMarqueeShiftIntervalMs(config_.spotify_marquee_shift_ms);
    spotify_monitor_.SetTimelineRefreshIntervalMs(config_.spotify_timeline_refresh_ms);
    Config::Save(app_data_dir_, config_);
  }

  i18n_.SetLanguage(config_.language);

  if (config_.auto_start_task_enabled) {
    const std::string exe_path = GetExecutablePath();
    if (!exe_path.empty()) {
      if (!startup_manager_.EnsureTaskEnabled(config_.auto_start_task_name, exe_path)) {
        Logger::Instance().Log(LogLevel::Warning,
                               "Task Scheduler autostart kaydi olusturulamadi.");
      }
    }
  }

  Logger::Instance().Log(LogLevel::Info, "Uygulama baslatiliyor.");

  if (!client_.DiscoverServer()) {
    return false;
  }

  if (!client_.RegisterGame()) {
    Logger::Instance().Log(LogLevel::Error, "Game metadata register basarisiz.");
    return false;
  }

  if (!client_.SetupClockHandler()) {
    Logger::Instance().Log(LogLevel::Error, "CLOCK handler setup basarisiz.");
    return false;
  }

  if (!client_.SetupSpotifyHandler()) {
    Logger::Instance().Log(LogLevel::Error, "SPOTIFY handler setup basarisiz.");
    return false;
  }

  if (!client_.SetupVolumeHandler()) {
    Logger::Instance().Log(LogLevel::Error, "VOLUME handler setup basarisiz.");
    return false;
  }

  if (!client_.SetupNotificationHandler()) {
    Logger::Instance().Log(LogLevel::Error, "NOTIFICATION handler setup basarisiz.");
    return false;
  }

  if (!client_.SetupGameModeHandler()) {
    Logger::Instance().Log(LogLevel::Error, "GAME_MODE handler setup basarisiz.");
    return false;
  }

  if (!client_.SetupUpdateHandler()) {
    Logger::Instance().Log(LogLevel::Error, "UPDATE handler setup basarisiz.");
    return false;
  }

  if (!client_.SetupEmailHandler()) {
    Logger::Instance().Log(LogLevel::Error, "EMAIL handler setup basarisiz.");
    return false;
  }

  CheckEmailCredentialStatus();

  notification_monitor_.Start();
  const std::string settings_path = app_data_dir_.empty() ? std::string() : JoinPath(app_data_dir_, "settings.ini");
  const std::string log_path = app_data_dir_.empty() ? std::string() : JoinPath(app_data_dir_, "app.log");
  tray_controller_.Start(
      [this]() { RequestStop(); },
      [this]() {
        const std::string exe = GetExecutablePath();
        if (!exe.empty()) {
          STARTUPINFOA si{};
          PROCESS_INFORMATION pi{};
          si.cb = sizeof(si);
          std::string cmd = "\"" + exe + "\"";
          if (CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si,
                             &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
          }
        }
        RequestStop();
      },
      [this](bool enabled) {
        config_.auto_update_check = enabled;
        if (!app_data_dir_.empty()) {
          Config::Save(app_data_dir_, config_);
        }
        Logger::Instance().Log(LogLevel::Info,
                               std::string("Oto guncelleme ") + (enabled ? "acildi" : "kapatildi"));
      },
      [this](bool enabled) {
        config_.email_enabled = enabled;
        email_monitor_.UpdateConfig(config_);
        if (enabled) {
          CheckEmailCredentialStatus();
        }
        if (!app_data_dir_.empty()) {
          Config::Save(app_data_dir_, config_);
        }
        Logger::Instance().Log(LogLevel::Info,
                               std::string("Email monitor ") + (enabled ? "acildi" : "kapatildi"));
      },
      [this](bool enabled) {
        config_.auto_start_task_enabled = enabled;
        if (enabled) {
          const std::string exe_path = GetExecutablePath();
          if (!exe_path.empty()) {
            startup_manager_.EnsureTaskEnabled(config_.auto_start_task_name, exe_path);
          }
        } else {
          startup_manager_.RemoveTask(config_.auto_start_task_name);
        }
        if (!app_data_dir_.empty()) {
          Config::Save(app_data_dir_, config_);
        }
        Logger::Instance().Log(LogLevel::Info,
                               std::string("Windows acilisinda baslat ") + (enabled ? "acildi" : "kapatildi"));
      },
      [this](bool enabled) {
        config_.game_mode_enabled = enabled;
        if (!app_data_dir_.empty()) {
          Config::Save(app_data_dir_, config_);
        }
      },
      [this](const std::string& source, bool enabled) {
        if (source == "spotify") {
          config_.media_spotify_enabled = enabled;
        } else if (source == "youtube_music") {
          config_.media_youtube_music_enabled = enabled;
        } else if (source == "apple_music") {
          config_.media_apple_music_enabled = enabled;
        } else if (source == "browser") {
          config_.media_browser_enabled = enabled;
        }
        ApplyMediaSourceToggles();
        if (!app_data_dir_.empty()) {
          Config::Save(app_data_dir_, config_);
        }
      },
      [this](bool use_24h) {
        config_.clock_24h_format = use_24h;
        if (!app_data_dir_.empty()) {
          Config::Save(app_data_dir_, config_);
        }
      },
      [this](const std::string& language) {
        config_.language = (language == "en") ? "en" : "tr";
        i18n_.SetLanguage(config_.language);
        if (config_.language == "en") {
          QueueLanguageOverlay("New Language", "English");
        } else {
          QueueLanguageOverlay("Yeni Dil", "Türkçe");
        }
        if (!app_data_dir_.empty()) {
          Config::Save(app_data_dir_, config_);
        }
        Logger::Instance().Log(LogLevel::Info,
                               std::string("Dil degisti: ") + config_.language);
      },
      [this](int marquee_shift_ms) {
        config_.spotify_marquee_shift_ms = marquee_shift_ms;
        spotify_monitor_.SetMarqueeShiftIntervalMs(config_.spotify_marquee_shift_ms);
        if (!app_data_dir_.empty()) {
          Config::Save(app_data_dir_, config_);
        }
        Logger::Instance().Log(LogLevel::Info,
                               "Spotify kayma hizi guncellendi: " +
                                   std::to_string(config_.spotify_marquee_shift_ms) + "ms");
      },
      settings_path,
      log_path,
      config_.auto_update_check,
      config_.email_enabled,
      config_.auto_start_task_enabled,
      config_.game_mode_enabled,
      config_.media_spotify_enabled,
      config_.media_youtube_music_enabled,
      config_.media_apple_music_enabled,
      config_.media_browser_enabled,
      config_.clock_24h_format,
      config_.language,
      config_.spotify_marquee_shift_ms);
  PlayStartupIntro();
  CheckUpdatesOnStartup();
  last_update_check_at_ = std::chrono::steady_clock::now();

  Logger::Instance().Log(LogLevel::Info, "Game register tamam.");
  running_.store(true);
  return true;
}

void App::CheckEmailCredentialStatus() {
  if (!config_.email_enabled) {
    return;
  }

  std::string username;
  std::string secret;
  const bool has_credential =
      credential_store_.ReadGeneric(config_.email_credential_target, username, secret);

  if (!has_credential) {
    Logger::Instance().Log(
        LogLevel::Warning,
        "Email acik ama credential bulunamadi. --set-email-credential ile kaydet.");
    return;
  }

  email_monitor_.SetCredential(username, secret);
  Logger::Instance().Log(LogLevel::Info, "Email credential bulundu: " + username);
}

void App::CheckUpdatesOnStartup() {
  Logger::Instance().Log(LogLevel::Info, "Guncelleme kontrolu baslatildi.");
  const UpdateInfo info = update_checker_.CheckLatest(config_.update_repo_owner,
                                                      config_.update_repo_name,
                                                      config_.app_version);
  if (!info.request_ok) {
    Logger::Instance().Log(LogLevel::Warning, "Guncelleme kontrolu basarisiz.");
    return;
  }

  if (info.has_update) {
    client_.SendUpdateEvent(i18n_.Text("update_title"),
                            i18n_.Text("update_new") + info.latest_version);
    Logger::Instance().Log(LogLevel::Info, "Yeni surum bulundu: " + info.latest_version);
    if (config_.auto_update_apply) {
      if (TryApplyUpdate(info)) {
        Logger::Instance().Log(LogLevel::Info, "Self-update baslatildi, uygulama kapatiliyor.");
        std::exit(0);
      }
      Logger::Instance().Log(LogLevel::Warning, "Guncelleme indirildi ancak self-update baslatilamadi.");
    }
  } else {
    client_.SendUpdateEvent(i18n_.Text("update_title"), i18n_.Text("update_latest"));
  }
}

void App::CheckUpdatesPeriodically() {
  if (!config_.auto_update_check) {
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  if (last_update_check_at_ == std::chrono::steady_clock::time_point{}) {
    last_update_check_at_ = now;
    return;
  }

  const auto elapsed_minutes =
      std::chrono::duration_cast<std::chrono::minutes>(now - last_update_check_at_).count();
  if (elapsed_minutes < config_.auto_update_check_interval_minutes) {
    return;
  }

  last_update_check_at_ = now;
  CheckUpdatesOnStartup();
}

bool App::TryApplyUpdate(const UpdateInfo& info) {
  if (app_data_dir_.empty() || info.download_url.empty()) {
    return false;
  }

  const std::string exe_path = GetExecutablePath();
  if (exe_path.empty()) {
    return false;
  }

  const std::string update_dir = JoinPath(app_data_dir_, "updates");
  EnsureDirectory(update_dir);
  const std::string downloaded_path =
      JoinPath(update_dir, "ss-ext-update-" + info.latest_version + ".exe");
  const std::string script_path = JoinPath(update_dir, "apply_update.cmd");

  client_.SendUpdateEvent(i18n_.Text("update_title"), i18n_.Text("update_downloading"));
  if (!update_checker_.DownloadToFile(info.download_url, downloaded_path)) {
    return false;
  }

  if (config_.auto_update_require_checksum) {
    std::string checksum_text;
    if (info.checksum_url.empty() ||
        !update_checker_.DownloadToString(info.checksum_url, checksum_text)) {
      Logger::Instance().Log(LogLevel::Warning,
                             "Checksum dosyasi alinamadi, update iptal edildi.");
      return false;
    }

    std::string expected_hash;
    if (!SSFileHash::ParseSha256Text(checksum_text, expected_hash)) {
      Logger::Instance().Log(LogLevel::Warning,
                             "Checksum dosyasi parse edilemedi, update iptal edildi.");
      return false;
    }

    std::string actual_hash;
    if (!SSFileHash::Sha256Hex(downloaded_path, actual_hash)) {
      Logger::Instance().Log(LogLevel::Warning,
                             "Indirilen dosya hash hesaplanamadi, update iptal edildi.");
      return false;
    }

    if (actual_hash != expected_hash) {
      Logger::Instance().Log(LogLevel::Error,
                             "Update hash dogrulamasi basarisiz, update iptal edildi.");
      return false;
    }
  }

  client_.SendUpdateEvent(i18n_.Text("update_title"), i18n_.Text("update_installing"));

  const DWORD pid = GetCurrentProcessId();
  {
    std::ofstream script(script_path, std::ios::binary | std::ios::trunc);
    if (!script.is_open()) {
      return false;
    }

    script << "@echo off\r\n";
    script << "setlocal\r\n";
    script << "set \"SRC=" << downloaded_path << "\"\r\n";
    script << "set \"DST=" << exe_path << "\"\r\n";
    script << "for /L %%i in (1,1,180) do (\r\n";
    script << "  tasklist /FI \"PID eq " << pid << "\" 2>nul | find \" " << pid << " \" >nul\r\n";
    script << "  if errorlevel 1 goto :copy\r\n";
    script << "  timeout /t 1 /nobreak >nul\r\n";
    script << ")\r\n";
    script << "goto :end\r\n";
    script << ":copy\r\n";
    script << "copy /Y \"%SRC%\" \"%DST%\" >nul\r\n";
    script << "if errorlevel 1 goto :end\r\n";
    script << "start \"\" \"%DST%\"\r\n";
    script << ":end\r\n";
    script << "del \"%~f0\" >nul 2>&1\r\n";
  }

  std::string command = "cmd.exe /C \"\"" + script_path + "\"\"";

  STARTUPINFOA si{};
  PROCESS_INFORMATION pi{};
  si.cb = sizeof(si);

  if (!CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &si, &pi)) {
    return false;
  }

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}

void App::SendClock() {
  const std::string time_text = NowTimeText(config_.clock_24h_format);
  const std::string date_text = NowDateText();

  if (!client_.SendClockEvent(time_text, date_text)) {
    Logger::Instance().Log(LogLevel::Warning, "CLOCK event gonderilemedi.");
  }
}

bool App::SendSpotifyOrClock(bool allow_clock_fallback) {
  const SpotifyState spotify = spotify_monitor_.Poll();
  if (spotify.active) {
    std::string duration = spotify.duration_text;
    if (duration.empty()) {
      duration = "--:--/--:--";
      if (spotify.changed) {
        Logger::Instance().Log(LogLevel::Warning,
                               "Spotify timeline alinamadi, fallback sure kullaniliyor: " +
                                   spotify.title);
      }
    }
    const std::string subtitle = duration;
    const std::string display_title =
        spotify.display_title.empty() ? spotify.title : spotify.display_title;
    if (!client_.SendSpotifyEvent(display_title, subtitle, duration)) {
      Logger::Instance().Log(LogLevel::Warning, "SPOTIFY event gonderilemedi.");
    }
    return true;
  }

  if (allow_clock_fallback) {
    SendClock();
  }
  return false;
}

bool App::SendGameMode() {
  const GameState game = game_monitor_.Poll();
  if (!game.active) {
    game_started_at_.reset();
    last_game_process_.clear();
    game_marquee_track_key_.clear();
    game_marquee_offset_ = 0;
    game_marquee_next_shift_at_ = std::chrono::steady_clock::time_point{};
    return false;
  }

  const auto now = std::chrono::steady_clock::now();
  if (last_game_process_ != game.process_name || !game_started_at_.has_value()) {
    last_game_process_ = game.process_name;
    game_started_at_ = now;
  }

  long long played_seconds = 0;
  if (game_started_at_.has_value()) {
    played_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                         now - game_started_at_.value())
                         .count();
  }

  std::string info = (config_.language == "en") ? "Played " : "Oynama ";

  const int mins = static_cast<int>(played_seconds / 60);
  const int secs = static_cast<int>(played_seconds % 60);
  info += std::to_string(mins) + ":" + (secs < 10 ? "0" : "") + std::to_string(secs);

  const std::string game_name = BuildGameModeTitle(game.display_name, now);

  if (!client_.SendGameModeEvent(game_name, info)) {
    Logger::Instance().Log(LogLevel::Warning, "GAME_MODE event gonderilemedi.");
  }
  return true;
}

void App::Run() {
  using namespace std::chrono;

  auto aligned_next_clock_tick = [&]() {
    const auto now_steady = steady_clock::now();
    const auto now_system = system_clock::now();
    const auto now_ms = duration_cast<milliseconds>(now_system.time_since_epoch());
    const auto ms_to_next_second = 1000 - (now_ms.count() % 1000);
    return now_steady + milliseconds(ms_to_next_second == 1000 ? 0 : ms_to_next_second);
  };

  auto next_clock_send_at = aligned_next_clock_tick();
  auto last_spotify_poll =
      steady_clock::now() - milliseconds(config_.spotify_poll_interval_ms);
  auto last_game_poll = steady_clock::now() - milliseconds(250);
  bool spotify_was_active = false;
  bool game_mode_was_active = false;
  bool skip_first_volume_overlay = true;
  overlay_end_time_ = steady_clock::now();
  overlay_type_ = OverlayType::None;

  Logger::Instance().Log(LogLevel::Info, "Main loop basladi.");
  while (running_.load()) {
    if (stop_event_ != nullptr && WaitForSingleObject(stop_event_, 0) == WAIT_OBJECT_0) {
      Logger::Instance().Log(LogLevel::Info, "Dis stop sinyali alindi.");
      RequestStop();
      break;
    }

    const auto now = steady_clock::now();

    {
      std::lock_guard<std::mutex> lock(language_overlay_mutex_);
      if (language_overlay_pending_) {
        client_.SendUpdateEvent(language_overlay_title_, language_overlay_subtitle_);
        overlay_type_ = OverlayType::Language;
        overlay_end_time_ = now + seconds(1);
        language_overlay_pending_ = false;
      }
    }

    const EmailItem email = email_monitor_.Poll();
    if (email.has_value) {
      client_.SendEmailEvent(email.subject, email.sender);
      overlay_type_ = OverlayType::Email;
      overlay_end_time_ = now + seconds(10);
    }

    const NotificationItem notification = notification_monitor_.PopLatest();
    if (notification.has_value) {
      client_.SendNotificationEvent(notification.app, i18n_.Text("notification_generic"));
      overlay_type_ = OverlayType::Notification;
      overlay_end_time_ = now + seconds(3);
    }

    const VolumeState volume = volume_monitor_.Poll();
    if (skip_first_volume_overlay) {
      skip_first_volume_overlay = false;
    } else if (volume.changed) {
      client_.SendVolumeEvent(i18n_.Text("volume_title"), std::to_string(volume.percent) + "%");
      overlay_type_ = OverlayType::Volume;
      overlay_end_time_ = now + seconds(2);
    }

    if (overlay_type_ != OverlayType::None && now < overlay_end_time_) {
      std::this_thread::sleep_for(milliseconds(config_.loop_interval_ms));
      continue;
    }

    overlay_type_ = OverlayType::None;

    if (duration_cast<milliseconds>(now - last_game_poll).count() >= 250) {
      game_mode_was_active = config_.game_mode_enabled && SendGameMode();
      last_game_poll = now;
    }

    CheckUpdatesPeriodically();

    if (!game_mode_was_active && duration_cast<milliseconds>(now - last_spotify_poll).count() >=
        config_.spotify_poll_interval_ms) {
      const bool spotify_active = SendSpotifyOrClock(false);
      last_spotify_poll = now;
      if (spotify_active) {
        spotify_was_active = true;
        next_clock_send_at = aligned_next_clock_tick();
      } else if (spotify_was_active) {
        SendClock();
        spotify_was_active = false;
        next_clock_send_at = aligned_next_clock_tick();
      }
    }

    if (!spotify_was_active && now >= next_clock_send_at) {
      SendClock();
      while (next_clock_send_at <= now) {
        next_clock_send_at += seconds(1);
      }
    }

    std::this_thread::sleep_for(milliseconds(config_.loop_interval_ms));
  }

  Logger::Instance().Log(LogLevel::Info, "Main loop durdu.");
  PlayShutdownOutro();
  notification_monitor_.Stop();
  tray_controller_.Stop();
}

void App::RequestStop() {
  running_.store(false);
}

}  // namespace ssext
