#include "App.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>

#include "Logger.hpp"
#include "PathUtils.hpp"
#include "security/SSFileHash.hpp"
#include "startup/ProcessLauncher.hpp"

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

std::string TrimAsciiText(const std::string& value) {
  auto begin = value.begin();
  while (begin != value.end() && std::isspace(static_cast<unsigned char>(*begin)) != 0) {
    ++begin;
  }

  auto end = value.end();
  while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1))) != 0) {
    --end;
  }

  return std::string(begin, end);
}

std::string NormalizeUiLanguage(std::string language_code) {
  std::transform(language_code.begin(), language_code.end(), language_code.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (language_code == "tr" || language_code == "en" || language_code == "de" ||
      language_code == "fr" || language_code == "es") {
    return language_code;
  }
  return "tr";
}

std::string LanguageNameKeyForCode(const std::string& language_code) {
  if (language_code == "en") {
    return "language_name_en";
  }
  if (language_code == "de") {
    return "language_name_de";
  }
  if (language_code == "fr") {
    return "language_name_fr";
  }
  if (language_code == "es") {
    return "language_name_es";
  }
  return "language_name_tr";
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

std::string ReadActiveProfileName(const std::string& state_path) {
  std::ifstream in(state_path);
  if (!in.is_open()) {
    return "default";
  }

  std::string line;
  while (std::getline(in, line)) {
    const auto eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }

    const std::string key = line.substr(0, eq);
    const std::string value = line.substr(eq + 1);
    if (key == "active_profile") {
      return value.empty() ? "default" : value;
    }
  }

  return "default";
}

void WriteActiveProfileName(const std::string& state_path, const std::string& profile_name) {
  std::ofstream out(state_path, std::ios::trunc);
  if (!out.is_open()) {
    return;
  }

  out << "active_profile=" << profile_name << "\n";
}

std::string ProfileLockPath(const std::string& app_data_dir, const std::string& profile_name) {
  return Config::ProfilePath(app_data_dir, profile_name) + ".lock";
}

std::string ProfileBackupPath(const std::string& profile_path) {
  return profile_path + ".lastgood";
}

std::string ExportDirectoryPath(const std::string& app_data_dir) {
  return JoinPath(app_data_dir, "profile_exports");
}

std::string NormalizeCredentialSuffix(const std::string& profile_name) {
  std::string normalized = Config::NormalizeProfileName(profile_name);
  if (normalized.empty()) {
    normalized = "default";
  }

  for (char& ch : normalized) {
    const unsigned char value = static_cast<unsigned char>(ch);
    if (value >= 'a' && value <= 'z') {
      ch = static_cast<char>(std::toupper(value));
    } else if (!std::isalnum(value) && ch != '_') {
      ch = '_';
    }
  }

  return normalized;
}

bool CopyFileIfExists(const std::string& from_path,
                      const std::string& to_path,
                      bool overwrite_existing) {
  std::error_code ec;
  if (!std::filesystem::exists(from_path, ec)) {
    return false;
  }

  const std::filesystem::copy_options options = overwrite_existing
                                                     ? std::filesystem::copy_options::overwrite_existing
                                                     : std::filesystem::copy_options::none;
  std::filesystem::copy_file(from_path, to_path, options, ec);
  return !ec;
}

std::string FileStemName(const std::string& path) {
  std::error_code ec;
  const std::filesystem::path fs_path(path);
  const std::string stem = fs_path.stem().string();
  return Config::NormalizeProfileName(stem);
}

void NormalizeProfileList(std::vector<std::string>& profile_names,
                          const std::string& active_profile_name) {
  if (profile_names.empty()) {
    profile_names.push_back("default");
  }

  if (std::find(profile_names.begin(), profile_names.end(), active_profile_name) ==
      profile_names.end()) {
    profile_names.push_back(active_profile_name);
  }

  std::sort(profile_names.begin(), profile_names.end());
  profile_names.erase(std::unique(profile_names.begin(), profile_names.end()), profile_names.end());
}

}  // namespace

void App::PlayStartupIntro() {
  std::string version_label = i18n_.Text("version_label");
  if (version_label.empty()) {
    version_label = "Version";
  }
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
  std::string bye = i18n_.Text("shutdown");
  if (bye.empty()) {
    bye = "Shutting down";
  }
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

bool App::IsNotificationSourceEnabled(const std::string& app_name) const {
  std::string lower = app_name;
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });

  if (lower.find("whatsapp") != std::string::npos) {
    return config_.notification_whatsapp_enabled;
  }
  if (lower.find("discord") != std::string::npos) {
    return config_.notification_discord_enabled;
  }
  if (lower.find("instagram") != std::string::npos || lower.find("insta") != std::string::npos) {
    return config_.notification_instagram_enabled;
  }

  return false;
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

bool App::InitializeProfiles() {
  if (app_data_dir_.empty()) {
    return false;
  }

  const std::string profiles_dir = Config::ProfilesDirectory(app_data_dir_);
  EnsureDirectory(profiles_dir);

  profile_state_path_ = JoinPath(app_data_dir_, "profile_state.ini");
  active_profile_name_ =
      Config::NormalizeProfileName(ReadActiveProfileName(profile_state_path_));
  active_profile_settings_path_ =
      Config::ProfilePath(app_data_dir_, active_profile_name_);

  const std::string legacy_settings_path = JoinPath(app_data_dir_, "settings.ini");
  std::error_code ec;
  if (!std::filesystem::exists(active_profile_settings_path_, ec) &&
      std::filesystem::exists(legacy_settings_path, ec)) {
    std::filesystem::copy_file(legacy_settings_path, active_profile_settings_path_,
                               std::filesystem::copy_options::overwrite_existing, ec);
  }

  if (!std::filesystem::exists(active_profile_settings_path_, ec)) {
    const std::string backup_path = ProfileBackupPath(active_profile_settings_path_);
    CopyFileIfExists(backup_path, active_profile_settings_path_, true);
  }

  Config::SaveTemplateIfMissingAtPath(active_profile_settings_path_);

  profile_names_ = Config::ListProfiles(app_data_dir_);
  NormalizeProfileList(profile_names_, active_profile_name_);
  WriteActiveProfileName(profile_state_path_, active_profile_name_);

  config_ = Config::LoadFromFile(active_profile_settings_path_);
  config_.language = NormalizeUiLanguage(config_.language);
  config_.email_imap_ssl = true;
  config_.email_smtp_ssl = true;
  config_.email_credential_target = BuildProfileCredentialTarget(active_profile_name_);
  SaveConfigWithRollback(active_profile_settings_path_, config_);
  return true;
}

void App::SaveActiveProfileConfig() {
  if (active_profile_settings_path_.empty()) {
    return;
  }
  if (!SaveConfigWithRollback(active_profile_settings_path_, config_)) {
    Logger::Instance().Log(LogLevel::Warning,
                           "Profil ayarlari kaydedilemedi, son saglam kopya geri yuklendi.");
  }
}

TrayProfileSelectionResult App::BuildTrayProfileSelectionResult(
    bool success,
    const std::string& error_message,
    const std::string& info_message) const {
  TrayProfileSelectionResult result;
  result.success = success;
  result.error_message = error_message;
  result.info_message = info_message;
  if (!success) {
    return result;
  }

  result.settings_path = active_profile_settings_path_;
  result.auto_update_enabled = config_.auto_update_check;
  result.email_enabled = config_.email_enabled;
  result.auto_start_enabled = config_.auto_start_task_enabled;
  result.game_mode_enabled = config_.game_mode_enabled;
  result.wp_notifications_enabled = config_.wp_notifications_enabled;
  result.notification_whatsapp_enabled = config_.notification_whatsapp_enabled;
  result.notification_discord_enabled = config_.notification_discord_enabled;
  result.notification_instagram_enabled = config_.notification_instagram_enabled;
  result.media_spotify_enabled = config_.media_spotify_enabled;
  result.media_youtube_music_enabled = config_.media_youtube_music_enabled;
  result.media_apple_music_enabled = config_.media_apple_music_enabled;
  result.media_browser_enabled = config_.media_browser_enabled;
  result.clock_24h_format = config_.clock_24h_format;
  result.language = config_.language;
  result.marquee_shift_ms = config_.spotify_marquee_shift_ms;
  result.email_settings.imap_server = config_.email_imap_server;
  result.email_settings.imap_port = config_.email_imap_port;
  result.email_settings.imap_ssl = config_.email_imap_ssl;
  result.email_settings.smtp_server = config_.email_smtp_server;
  result.email_settings.smtp_port = config_.email_smtp_port;
  result.email_settings.smtp_ssl = config_.email_smtp_ssl;
  result.email_settings.username = config_.email_username;
  result.email_settings.poll_interval_seconds = config_.email_check_interval_seconds;
  result.profile_names = profile_names_;
  result.active_profile_name = active_profile_name_;
  result.active_profile_locked = IsProfileLocked(active_profile_name_);
  return result;
}

bool App::CreateProfile(const std::string& profile_name) {
  if (app_data_dir_.empty()) {
    return false;
  }

  const std::string normalized = Config::NormalizeProfileName(profile_name);
  if (normalized.empty()) {
    return false;
  }

  const std::string target_profile_path = Config::ProfilePath(app_data_dir_, normalized);
  std::error_code ec;
  if (std::filesystem::exists(target_profile_path, ec)) {
    return false;
  }

  SaveActiveProfileConfig();
  if (!SaveConfigWithRollback(target_profile_path, config_)) {
    return false;
  }

  return SwitchProfile(normalized);
}

bool App::CopyProfile(const std::string& from_profile_name,
                      const std::string& to_profile_name) {
  if (app_data_dir_.empty()) {
    return false;
  }

  const std::string from_normalized = Config::NormalizeProfileName(from_profile_name);
  const std::string to_normalized = Config::NormalizeProfileName(to_profile_name);
  if (from_normalized.empty() || to_normalized.empty() || from_normalized == to_normalized) {
    return false;
  }

  if (IsProfileLocked(from_normalized)) {
    return false;
  }

  const std::string from_path = Config::ProfilePath(app_data_dir_, from_normalized);
  const std::string to_path = Config::ProfilePath(app_data_dir_, to_normalized);

  std::error_code ec;
  if (!std::filesystem::exists(from_path, ec) || std::filesystem::exists(to_path, ec)) {
    return false;
  }

  SaveActiveProfileConfig();
  std::filesystem::copy_file(from_path, to_path, std::filesystem::copy_options::none, ec);
  if (ec) {
    return false;
  }

  const std::string from_lock_path = ProfileLockPath(app_data_dir_, from_normalized);
  const std::string to_lock_path = ProfileLockPath(app_data_dir_, to_normalized);
  if (std::filesystem::exists(from_lock_path, ec)) {
    CopyFileIfExists(from_lock_path, to_lock_path, true);
  }

  Logger::Instance().Log(LogLevel::Info,
                         "Profil kopyalandi: " + from_normalized + " -> " + to_normalized);
  return SwitchProfile(to_normalized);
}

bool App::RenameProfile(const std::string& from_profile_name,
                        const std::string& to_profile_name) {
  if (app_data_dir_.empty()) {
    return false;
  }

  const std::string from_normalized = Config::NormalizeProfileName(from_profile_name);
  const std::string to_normalized = Config::NormalizeProfileName(to_profile_name);
  if (from_normalized.empty() || to_normalized.empty()) {
    return false;
  }

  if (from_normalized == to_normalized) {
    return true;
  }

  if (IsProfileLocked(from_normalized)) {
    return false;
  }

  const std::string from_path = Config::ProfilePath(app_data_dir_, from_normalized);
  const std::string to_path = Config::ProfilePath(app_data_dir_, to_normalized);

  std::error_code ec;
  if (!std::filesystem::exists(from_path, ec) ||
      std::filesystem::exists(to_path, ec)) {
    return false;
  }

  const bool renamed_active_profile = (from_normalized == active_profile_name_);

  SaveActiveProfileConfig();
  std::filesystem::rename(from_path, to_path, ec);
  if (ec) {
    return false;
  }

  const std::string from_lock_path = ProfileLockPath(app_data_dir_, from_normalized);
  const std::string to_lock_path = ProfileLockPath(app_data_dir_, to_normalized);
  std::filesystem::rename(from_lock_path, to_lock_path, ec);
  ec.clear();

  if (renamed_active_profile) {
    active_profile_name_ = to_normalized;
    active_profile_settings_path_ = to_path;
    WriteActiveProfileName(profile_state_path_, active_profile_name_);
  }

  profile_names_ = Config::ListProfiles(app_data_dir_);
  NormalizeProfileList(profile_names_, active_profile_name_);

  Logger::Instance().Log(LogLevel::Info,
                         "Profil yeniden adlandirildi: " + from_normalized + " -> " +
                             to_normalized);
  return true;
}

bool App::DeleteProfile(const std::string& profile_name) {
  if (app_data_dir_.empty()) {
    return false;
  }

  const std::string normalized = Config::NormalizeProfileName(profile_name);
  if (normalized.empty()) {
    return false;
  }

  if (IsProfileLocked(normalized)) {
    return false;
  }

  const std::string profile_path = Config::ProfilePath(app_data_dir_, normalized);
  std::error_code ec;
  if (!std::filesystem::exists(profile_path, ec)) {
    return false;
  }

  SaveActiveProfileConfig();
  const bool removed = std::filesystem::remove(profile_path, ec);
  if (!removed || ec) {
    return false;
  }

  const std::string lock_path = ProfileLockPath(app_data_dir_, normalized);
  std::filesystem::remove(lock_path, ec);
  ec.clear();

  if (normalized == active_profile_name_) {
    std::vector<std::string> available_profiles = Config::ListProfiles(app_data_dir_);
    if (available_profiles.empty()) {
      available_profiles.push_back("default");
    }

    std::string next_profile = available_profiles.front();
    if (next_profile == normalized && available_profiles.size() > 1) {
      next_profile = available_profiles[1];
    }

    if (next_profile == normalized) {
      next_profile = "default";
    }

    return SwitchProfile(next_profile);
  }

  profile_names_ = Config::ListProfiles(app_data_dir_);
  NormalizeProfileList(profile_names_, active_profile_name_);

  Logger::Instance().Log(LogLevel::Info,
                         "Profil silindi: " + normalized);
  return true;
}

bool App::ExportProfile(const std::string& profile_name,
                        const std::string& destination_path) {
  if (app_data_dir_.empty()) {
    return false;
  }

  const std::string normalized = Config::NormalizeProfileName(profile_name);
  if (normalized.empty() || destination_path.empty()) {
    return false;
  }

  const std::string source_path = Config::ProfilePath(app_data_dir_, normalized);
  std::error_code ec;
  if (!std::filesystem::exists(source_path, ec)) {
    return false;
  }

  if (normalized == active_profile_name_) {
    SaveActiveProfileConfig();
  }

  const std::filesystem::path target_path(destination_path);
  const auto parent = target_path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
  }

  std::filesystem::copy_file(source_path, destination_path,
                             std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) {
    return false;
  }

  Logger::Instance().Log(LogLevel::Info,
                         "Profil disa aktarildi: " + normalized + " -> " + destination_path);
  return true;
}

bool App::ImportProfile(const std::string& source_path,
                        const std::string& preferred_profile_name) {
  if (app_data_dir_.empty()) {
    return false;
  }

  std::error_code ec;
  if (!std::filesystem::exists(source_path, ec)) {
    return false;
  }

  std::string target_profile_name = Config::NormalizeProfileName(preferred_profile_name);
  if (target_profile_name.empty()) {
    target_profile_name = FileStemName(source_path);
  }
  if (target_profile_name.empty()) {
    return false;
  }

  const std::string target_path = Config::ProfilePath(app_data_dir_, target_profile_name);
  if (std::filesystem::exists(target_path, ec)) {
    return false;
  }

  std::filesystem::copy_file(source_path, target_path, std::filesystem::copy_options::none, ec);
  if (ec) {
    return false;
  }

  Config imported = Config::LoadFromFile(target_path);
  imported.email_credential_target = BuildProfileCredentialTarget(target_profile_name);
  if (!SaveConfigWithRollback(target_path, imported)) {
    std::filesystem::remove(target_path, ec);
    return false;
  }

  Logger::Instance().Log(LogLevel::Info,
                         "Profil ice aktarildi: " + target_profile_name +
                             " (kaynak: " + source_path + ")");
  return SwitchProfile(target_profile_name);
}

bool App::SetProfileLocked(const std::string& profile_name, bool locked) {
  if (app_data_dir_.empty()) {
    return false;
  }

  const std::string normalized = Config::NormalizeProfileName(profile_name);
  if (normalized.empty()) {
    return false;
  }

  const std::string lock_path = ProfileLockPath(app_data_dir_, normalized);
  std::error_code ec;
  if (locked) {
    std::ofstream out(lock_path, std::ios::trunc);
    if (!out.is_open()) {
      return false;
    }
    out << "locked=true\n";
    out << "profile=" << normalized << "\n";
    out << "by=Carex-Ext\n";
    out.close();
    return true;
  }

  std::filesystem::remove(lock_path, ec);
  return !ec;
}

bool App::IsProfileLocked(const std::string& profile_name) const {
  if (app_data_dir_.empty()) {
    return false;
  }

  const std::string normalized = Config::NormalizeProfileName(profile_name);
  if (normalized.empty()) {
    return false;
  }

  std::error_code ec;
  return std::filesystem::exists(ProfileLockPath(app_data_dir_, normalized), ec);
}

std::string App::BuildProfileCredentialTarget(const std::string& profile_name) const {
  return "CAREX_EXT_IMAP_" + NormalizeCredentialSuffix(profile_name);
}

bool App::SaveConfigWithRollback(const std::string& settings_path, const Config& cfg) const {
  if (settings_path.empty()) {
    return false;
  }

  const std::string backup_path = ProfileBackupPath(settings_path);
  CopyFileIfExists(settings_path, backup_path, true);

  if (Config::SaveToFile(settings_path, cfg)) {
    return true;
  }

  CopyFileIfExists(backup_path, settings_path, true);
  return false;
}

bool App::TestSmtpConnectivity(const TrayEmailSettings& email_settings,
                               std::string& error_message) const {
  if (!EmailMonitor::TestSmtpConnection(email_settings.smtp_server,
                                        email_settings.smtp_port,
                                        email_settings.smtp_ssl,
                                        error_message)) {
    return false;
  }

  return true;
}

bool App::SwitchProfile(const std::string& profile_name) {
  if (app_data_dir_.empty()) {
    return false;
  }

  const std::string normalized = Config::NormalizeProfileName(profile_name);
  if (normalized.empty()) {
    return false;
  }

  if (normalized == active_profile_name_) {
    return true;
  }

  SaveActiveProfileConfig();

  const std::string next_profile_path = Config::ProfilePath(app_data_dir_, normalized);
  const std::string next_backup_path = ProfileBackupPath(next_profile_path);
  std::error_code ec;
  if (!std::filesystem::exists(next_profile_path, ec) &&
      std::filesystem::exists(next_backup_path, ec)) {
    CopyFileIfExists(next_backup_path, next_profile_path, true);
  }
  Config::SaveTemplateIfMissingAtPath(next_profile_path);

  active_profile_name_ = normalized;
  active_profile_settings_path_ = next_profile_path;
  WriteActiveProfileName(profile_state_path_, active_profile_name_);

  config_ = Config::LoadFromFile(active_profile_settings_path_);
  config_.language = NormalizeUiLanguage(config_.language);
  config_.email_imap_ssl = true;
  config_.email_smtp_ssl = true;
  config_.email_credential_target = BuildProfileCredentialTarget(active_profile_name_);
  client_.SetConfig(config_);
  i18n_.SetLanguage(config_.language);
  email_monitor_.UpdateConfig(config_);
  spotify_monitor_.SetMarqueeShiftIntervalMs(config_.spotify_marquee_shift_ms);
  spotify_monitor_.SetTimelineRefreshIntervalMs(config_.spotify_timeline_refresh_ms);
  ApplyMediaSourceToggles();

  profile_names_ = Config::ListProfiles(app_data_dir_);
  NormalizeProfileList(profile_names_, active_profile_name_);

  if (config_.auto_start_task_enabled) {
    const std::string exe_path = GetExecutablePath();
    if (!exe_path.empty()) {
      startup_manager_.EnsureTaskEnabled(config_.auto_start_task_name, exe_path);
    }
  } else {
    startup_manager_.RemoveTask(config_.auto_start_task_name);
  }

  if (config_.email_enabled) {
    CheckEmailCredentialStatus();
  }

  SaveActiveProfileConfig();

  Logger::Instance().Log(LogLevel::Info,
                         "Profil degisti: " + active_profile_name_);
  return true;
}

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
    if (!InitializeProfiles()) {
      Logger::Instance().Log(LogLevel::Error, "Profil sistemi baslatilamadi.");
      return false;
    }

    config_.email_credential_target = BuildProfileCredentialTarget(active_profile_name_);

    config_.app_version = NormalizeDisplayVersion(config_.app_version);

    client_.SetConfig(config_);
    email_monitor_ = EmailMonitor(config_);
      ApplyMediaSourceToggles();
    spotify_monitor_.SetMarqueeShiftIntervalMs(config_.spotify_marquee_shift_ms);
    spotify_monitor_.SetTimelineRefreshIntervalMs(config_.spotify_timeline_refresh_ms);
    SaveActiveProfileConfig();
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

  auto should_run_notification_monitor = [this]() {
    return config_.wp_notifications_enabled &&
           (config_.notification_whatsapp_enabled ||
            config_.notification_discord_enabled ||
            config_.notification_instagram_enabled);
  };

  if (should_run_notification_monitor()) {
    notification_monitor_.Start();
  }
  const std::string settings_path =
      app_data_dir_.empty() ? std::string() : active_profile_settings_path_;
  const std::string log_path = app_data_dir_.empty() ? std::string() : JoinPath(app_data_dir_, "app.log");
  tray_controller_.Start(
      [this]() { RequestStop(); },
      [this]() {
        if (restart_requested_.exchange(true)) {
          Logger::Instance().Log(LogLevel::Info,
                                 "Yeniden baslatma istegi zaten alindi, tekrar yok sayildi.");
          return;
        }

        bool scheduled = false;
        const std::string exe = GetExecutablePath();
        if (!exe.empty()) {
          scheduled = ProcessLauncher::ScheduleRestartAfterExit(exe, GetCurrentProcessId());
          if (!scheduled) {
            Logger::Instance().Log(LogLevel::Warning,
                                   "Yeniden baslatma zamanlanamadi.");
          }
        }

        if (!scheduled) {
          restart_requested_.store(false);
          return;
        }

        RequestStop();
      },
      [this](bool enabled) {
        config_.auto_update_check = enabled;
        SaveActiveProfileConfig();
        Logger::Instance().Log(LogLevel::Info,
                               std::string("Oto guncelleme ") + (enabled ? "acildi" : "kapatildi"));
      },
      [this](bool enabled) {
        config_.email_enabled = enabled;
        email_monitor_.UpdateConfig(config_);
        if (enabled) {
          CheckEmailCredentialStatus();
        }
        SaveActiveProfileConfig();
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
        SaveActiveProfileConfig();
        Logger::Instance().Log(LogLevel::Info,
                               std::string("Windows acilisinda baslat ") + (enabled ? "acildi" : "kapatildi"));
      },
      [this](bool enabled) {
        config_.game_mode_enabled = enabled;
        SaveActiveProfileConfig();
      },
      [this](bool enabled) {
        config_.wp_notifications_enabled = enabled;
        const bool should_run_notification_monitor =
            config_.wp_notifications_enabled &&
            (config_.notification_whatsapp_enabled ||
             config_.notification_discord_enabled ||
             config_.notification_instagram_enabled);
        if (should_run_notification_monitor) {
          notification_monitor_.Start();
        } else {
          notification_monitor_.Stop();
          notification_monitor_.PopLatest();
        }
        SaveActiveProfileConfig();
        Logger::Instance().Log(LogLevel::Info,
                               std::string("WP bildirimleri ") +
                                   (enabled ? "acildi" : "kapatildi"));
      },
      [this](const std::string& source, bool enabled) {
        if (source == "whatsapp") {
          config_.notification_whatsapp_enabled = enabled;
        } else if (source == "discord") {
          config_.notification_discord_enabled = enabled;
        } else if (source == "instagram") {
          config_.notification_instagram_enabled = enabled;
        }

        const bool should_run_notification_monitor =
            config_.wp_notifications_enabled &&
            (config_.notification_whatsapp_enabled ||
             config_.notification_discord_enabled ||
             config_.notification_instagram_enabled);
        if (should_run_notification_monitor) {
          notification_monitor_.Start();
        } else {
          notification_monitor_.Stop();
          notification_monitor_.PopLatest();
        }

        SaveActiveProfileConfig();
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
        SaveActiveProfileConfig();
      },
      [this](bool use_24h) {
        config_.clock_24h_format = use_24h;
        SaveActiveProfileConfig();
      },
      [this](const std::string& language) {
        config_.language = NormalizeUiLanguage(language);
        i18n_.SetLanguage(config_.language);
        std::string overlay_title = i18n_.Text("language_changed_title");
        if (overlay_title.empty()) {
          overlay_title = "Language";
        }
        std::string overlay_subtitle = i18n_.Text(LanguageNameKeyForCode(config_.language));
        if (overlay_subtitle.empty()) {
          overlay_subtitle = config_.language;
        }
        QueueLanguageOverlay(overlay_title, overlay_subtitle);
        SaveActiveProfileConfig();
        Logger::Instance().Log(LogLevel::Info,
                               std::string("Dil degisti: ") + config_.language);
      },
      [this](int marquee_shift_ms) {
        config_.spotify_marquee_shift_ms = marquee_shift_ms;
        spotify_monitor_.SetMarqueeShiftIntervalMs(config_.spotify_marquee_shift_ms);
        SaveActiveProfileConfig();
        Logger::Instance().Log(LogLevel::Info,
                               "Medya isim kayma hizi guncellendi: " +
                                   std::to_string(config_.spotify_marquee_shift_ms) + "ms");
      },
      [this]() {
        return CheckForUpdatesManually();
      },
      [this](const TrayEmailSettings& email_settings) {
        config_.email_imap_server = email_settings.imap_server;
        config_.email_imap_port = email_settings.imap_port;
        config_.email_imap_ssl = email_settings.imap_ssl;
        config_.email_smtp_server = email_settings.smtp_server;
        config_.email_smtp_port = email_settings.smtp_port;
        config_.email_smtp_ssl = email_settings.smtp_ssl;
        config_.email_username = email_settings.username;
        config_.email_check_interval_seconds = email_settings.poll_interval_seconds;
        email_monitor_.UpdateConfig(config_);
        if (config_.email_enabled) {
          CheckEmailCredentialStatus();
        }
        SaveActiveProfileConfig();
        Logger::Instance().Log(LogLevel::Info,
                               "E-posta ayarlari guncellendi.");
      },
      [this](const std::string& username, const std::string& password) {
        return SyncEmailCredential(username, password);
      },
      [this](const TrayEmailSettings& email_settings) {
        std::string error_message;
        if (!TestSmtpConnectivity(email_settings, error_message)) {
          if (error_message.empty()) {
            error_message = (config_.language == "en")
                                ? "SMTP connection test failed."
                                : "SMTP baglanti testi basarisiz oldu.";
          }
          return error_message;
        }

        return std::string();
      },
      [this](const std::string& profile_name) {
        const bool success = SwitchProfile(profile_name);
        if (!success) {
          const std::string message = (config_.language == "en")
                                          ? "Selected profile could not be loaded."
                                          : "Secilen profil yuklenemedi.";
          return BuildTrayProfileSelectionResult(false, message);
        }
        return BuildTrayProfileSelectionResult(true);
      },
      [this](const std::string& profile_name) {
        const std::string normalized = Config::NormalizeProfileName(profile_name);
        if (normalized.empty()) {
          const std::string message = (config_.language == "en")
                                          ? "Invalid profile name."
                                          : "Gecersiz profil adi.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        const std::string target_path = Config::ProfilePath(app_data_dir_, normalized);
        std::error_code ec;
        if (std::filesystem::exists(target_path, ec)) {
          const std::string message = (config_.language == "en")
                                          ? "A profile with this name already exists."
                                          : "Bu isimde bir profil zaten var.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        if (!CreateProfile(normalized)) {
          const std::string message = (config_.language == "en")
                                          ? "Profile could not be created."
                                          : "Profil olusturulamadi.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        return BuildTrayProfileSelectionResult(true);
      },
      [this](const std::string& from_profile_name, const std::string& to_profile_name) {
        const std::string from_normalized = Config::NormalizeProfileName(from_profile_name);
        const std::string to_normalized = Config::NormalizeProfileName(to_profile_name);
        if (from_normalized.empty() || to_normalized.empty()) {
          const std::string message = (config_.language == "en")
                                          ? "Select a source profile and enter a target profile name."
                                          : "Kaynak profil secin ve hedef profil adini girin.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        if (IsProfileLocked(from_normalized)) {
          const std::string message = (config_.language == "en")
                                          ? "Source profile is locked."
                                          : "Kaynak profil kilitli.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        if (!CopyProfile(from_normalized, to_normalized)) {
          const std::string message = (config_.language == "en")
                                          ? "Profile could not be copied."
                                          : "Profil kopyalanamadi.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        const std::string info = (config_.language == "en")
                                     ? "Profile copied successfully."
                                     : "Profil basariyla kopyalandi.";
        return BuildTrayProfileSelectionResult(true, {}, info);
      },
      [this](const std::string& from_profile_name, const std::string& to_profile_name) {
        const std::string from_normalized = Config::NormalizeProfileName(from_profile_name);
        const std::string to_normalized = Config::NormalizeProfileName(to_profile_name);

        if (to_normalized.empty()) {
          const std::string message = (config_.language == "en")
                                          ? "Invalid new profile name."
                                          : "Yeni profil adi gecersiz.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        const std::string from_path = Config::ProfilePath(app_data_dir_, from_normalized);
        const std::string to_path = Config::ProfilePath(app_data_dir_, to_normalized);
        std::error_code ec;
        if (!std::filesystem::exists(from_path, ec)) {
          const std::string message = (config_.language == "en")
                                          ? "Source profile was not found."
                                          : "Yeniden adlandirilacak profil bulunamadi.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        if (IsProfileLocked(from_normalized)) {
          const std::string message = (config_.language == "en")
                                          ? "Locked profiles cannot be renamed."
                                          : "Kilitli profiller yeniden adlandirilamaz.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        if (from_normalized != to_normalized && std::filesystem::exists(to_path, ec)) {
          const std::string message = (config_.language == "en")
                                          ? "A profile with this new name already exists."
                                          : "Bu yeni isimde bir profil zaten var.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        if (!RenameProfile(from_normalized, to_normalized)) {
          const std::string message = (config_.language == "en")
                                          ? "Profile could not be renamed."
                                          : "Profil yeniden adlandirilamadi.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        return BuildTrayProfileSelectionResult(true);
      },
      [this](const std::string& profile_name) {
        const std::string normalized = Config::NormalizeProfileName(profile_name);
        const std::string profile_path = Config::ProfilePath(app_data_dir_, normalized);
        std::error_code ec;
        if (!std::filesystem::exists(profile_path, ec)) {
          const std::string message = (config_.language == "en")
                                          ? "Profile was not found."
                                          : "Profil bulunamadi.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        if (!DeleteProfile(normalized)) {
          if (IsProfileLocked(normalized)) {
            const std::string message = (config_.language == "en")
                                            ? "Locked profiles cannot be deleted."
                                            : "Kilitli profiller silinemez.";
            return BuildTrayProfileSelectionResult(false, message);
          }

          const std::string message = (config_.language == "en")
                                          ? "Profile could not be deleted."
                                          : "Profil silinemedi.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        return BuildTrayProfileSelectionResult(true);
      },
      [this](const std::string& profile_name, const std::string& destination_path) {
        const std::string normalized = Config::NormalizeProfileName(profile_name);
        std::string output_path = destination_path;
        if (output_path.empty()) {
          const std::string export_dir = ExportDirectoryPath(app_data_dir_);
          EnsureDirectory(export_dir);
          output_path = JoinPath(export_dir, normalized + "-export.ini");
        }

        if (!ExportProfile(normalized, output_path)) {
          const std::string message = (config_.language == "en")
                                          ? "Profile could not be exported."
                                          : "Profil disa aktarilamadi.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        const std::string info = (config_.language == "en")
                                     ? "Profile exported to: " + output_path
                                     : "Profil disa aktarildi: " + output_path;
        return BuildTrayProfileSelectionResult(true, {}, info);
      },
      [this](const std::string& source_path, const std::string& preferred_name) {
        if (source_path.empty()) {
          const std::string message = (config_.language == "en")
                                          ? "Enter an import file path first."
                                          : "Once ice aktarim dosya yolunu girin.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        if (!ImportProfile(source_path, preferred_name)) {
          const std::string message = (config_.language == "en")
                                          ? "Profile could not be imported."
                                          : "Profil ice aktarilamadi.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        const std::string info = (config_.language == "en")
                                     ? "Profile imported successfully."
                                     : "Profil basariyla ice aktarildi.";
        return BuildTrayProfileSelectionResult(true, {}, info);
      },
      [this](const std::string& profile_name, bool locked) {
        const std::string normalized = Config::NormalizeProfileName(profile_name);
        if (normalized.empty()) {
          const std::string message = (config_.language == "en")
                                          ? "Select a profile first."
                                          : "Once bir profil secin.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        if (!SetProfileLocked(normalized, locked)) {
          const std::string message = (config_.language == "en")
                                          ? "Profile lock state could not be updated."
                                          : "Profil kilit durumu guncellenemedi.";
          return BuildTrayProfileSelectionResult(false, message);
        }

        const std::string info = locked
                                     ? ((config_.language == "en") ? "Profile locked." : "Profil kilitlendi.")
                                     : ((config_.language == "en") ? "Profile unlocked." : "Profil kilidi kaldirildi.");
        return BuildTrayProfileSelectionResult(true, {}, info);
      },
      settings_path,
      log_path,
      config_.auto_update_check,
      config_.email_enabled,
      config_.auto_start_task_enabled,
      config_.game_mode_enabled,
      config_.wp_notifications_enabled,
      config_.notification_whatsapp_enabled,
      config_.notification_discord_enabled,
      config_.notification_instagram_enabled,
      config_.media_spotify_enabled,
      config_.media_youtube_music_enabled,
      config_.media_apple_music_enabled,
      config_.media_browser_enabled,
      config_.clock_24h_format,
      config_.language,
      config_.app_version,
      config_.developer,
      config_.spotify_marquee_shift_ms,
        TrayEmailSettings{
          config_.email_imap_server,
          config_.email_imap_port,
          config_.email_imap_ssl,
          config_.email_smtp_server,
          config_.email_smtp_port,
          config_.email_smtp_ssl,
          config_.email_username,
          config_.email_check_interval_seconds},
      profile_names_,
          active_profile_name_,
          IsProfileLocked(active_profile_name_));
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

  if (config_.email_credential_target.empty()) {
    config_.email_credential_target = BuildProfileCredentialTarget(active_profile_name_);
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

std::string App::SyncEmailCredential(const std::string& username,
                                     const std::string& password) {
  if (config_.email_credential_target.empty()) {
    config_.email_credential_target = BuildProfileCredentialTarget(active_profile_name_);
  }

  const std::string requested_username = TrimAsciiText(username);

  std::string stored_username;
  std::string stored_secret;
  const bool has_existing =
      credential_store_.ReadGeneric(config_.email_credential_target, stored_username, stored_secret);

  if (!password.empty()) {
    const std::string credential_username =
        requested_username.empty() ? TrimAsciiText(config_.email_username) : requested_username;
    if (credential_username.empty()) {
      return config_.language == "en"
                 ? "Username is required to save IMAP password."
                 : "IMAP sifresini kaydetmek icin kullanici adi gerekli.";
    }

    if (!credential_store_.SaveGeneric(config_.email_credential_target,
                                       credential_username,
                                       password)) {
      return config_.language == "en"
                 ? "IMAP credentials could not be saved."
                 : "IMAP kimlik bilgileri kaydedilemedi.";
    }

    email_monitor_.SetCredential(credential_username, password);
    Logger::Instance().Log(LogLevel::Info,
                           "Email credential guncellendi: " + credential_username);
    return {};
  }

  if (!has_existing) {
    return config_.language == "en"
               ? "IMAP password is missing. Enter password once and apply settings again."
               : "IMAP sifresi eksik. Bir kez sifre girip ayarlari tekrar uygulayin.";
  }

  std::string credential_username = stored_username;
  if (!requested_username.empty()) {
    credential_username = requested_username;
  }

  if (credential_username != stored_username) {
    if (!credential_store_.SaveGeneric(config_.email_credential_target,
                                       credential_username,
                                       stored_secret)) {
      return config_.language == "en"
                 ? "Credential username update failed."
                 : "Kayitli kimlik kullanici adi guncellenemedi.";
    }
  }

  email_monitor_.SetCredential(credential_username, stored_secret);
  return {};
}

TrayActionStatus App::CheckForUpdatesManually() {
  TrayActionStatus status;
  status.success = true;

  const UpdateInfo info = update_checker_.CheckLatest(config_.update_repo_owner,
                                                      config_.update_repo_name,
                                                      config_.app_version);
  if (!info.request_ok) {
    status.success = false;
    status.message = i18n_.Text("update_check_failed");
    if (status.message.empty()) {
      status.message = "Update check failed.";
    }
    Logger::Instance().Log(LogLevel::Warning, "Manuel guncelleme kontrolu basarisiz.");
    return status;
  }

  if (!info.has_update) {
    client_.SendUpdateEvent(i18n_.Text("update_title"), i18n_.Text("update_latest"));
    status.message = i18n_.Text("update_latest");
    if (status.message.empty()) {
      status.message = "You are up to date";
    }
    return status;
  }

  client_.SendUpdateEvent(i18n_.Text("update_title"), i18n_.Text("update_new") + info.latest_version);

  if (!TryApplyUpdate(info)) {
    status.success = false;
    status.message = i18n_.Text("update_apply_failed");
    if (status.message.empty()) {
      status.message = "Update could not be installed.";
    }
    return status;
  }

  status.message = i18n_.Text("update_apply_started");
  if (status.message.empty()) {
    status.message = "Installing update. Restarting app...";
  }

  RequestStop();
  return status;
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
      JoinPath(update_dir, "carex-ext-update-" + info.latest_version + ".exe");
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
  return ProcessLauncher::ScheduleReplaceAndRestartAfterExit(
      downloaded_path,
      exe_path,
      GetCurrentProcessId());
}

void App::SendClock() {
  const std::string time_text = NowTimeText(config_.clock_24h_format);
  const std::string date_text = NowDateText();

  client_.SendClockEvent(time_text, date_text);
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

    if (spotify.source_confidence > 0 && spotify.source_confidence < 40 && spotify.changed) {
      Logger::Instance().Log(LogLevel::Info,
                             "Media source confidence dusuk: " +
                                 std::to_string(spotify.source_confidence) +
                                 " (" + spotify.title + ")");
    }

    const std::string subtitle = duration;
    const std::string display_title =
        spotify.display_title.empty() ? spotify.title : spotify.display_title;
    client_.SendSpotifyEvent(display_title, subtitle, duration);
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

  std::string info = i18n_.Text("game_played");
  if (info.empty()) {
    info = "Played ";
  }

  const int mins = static_cast<int>(played_seconds / 60);
  const int secs = static_cast<int>(played_seconds % 60);
  info += std::to_string(mins) + ":" + (secs < 10 ? "0" : "") + std::to_string(secs);

  const std::string game_name = BuildGameModeTitle(game.display_name, now);

  client_.SendGameModeEvent(game_name, info);
  return true;
}

void App::Run() {
  using namespace std::chrono;

  const auto loop_sleep = milliseconds((std::max)(50, config_.loop_interval_ms));
  const auto game_poll_interval = milliseconds(250);
  const auto spotify_poll_interval = milliseconds((std::max)(100, config_.spotify_poll_interval_ms));
  const auto clock_send_interval = milliseconds((std::max)(250, config_.clock_send_interval_ms));

  auto aligned_next_clock_tick = [&]() {
    const auto now_steady = steady_clock::now();
    if (clock_send_interval != seconds(1)) {
      return now_steady + clock_send_interval;
    }

    const auto now_system = system_clock::now();
    const auto now_ms = duration_cast<milliseconds>(now_system.time_since_epoch());
    const auto ms_to_next_second = 1000 - (now_ms.count() % 1000);
    return now_steady + milliseconds(ms_to_next_second == 1000 ? 0 : ms_to_next_second);
  };

  auto next_clock_send_at = aligned_next_clock_tick();
  auto last_spotify_poll = steady_clock::now() - spotify_poll_interval;
  auto last_game_poll = steady_clock::now() - game_poll_interval;
  bool spotify_was_active = false;
  bool game_mode_was_active = false;
  bool skip_first_volume_overlay = true;
  long long language_lock_wait_total_us = 0;
  int language_lock_wait_samples = 0;
  int language_lock_contention_samples = 0;
  auto next_language_lock_log_at = steady_clock::now() + seconds(60);
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
      const auto lock_started_at = steady_clock::now();
      std::unique_lock<std::mutex> lock(language_overlay_mutex_);
      const long long wait_us = duration_cast<microseconds>(steady_clock::now() - lock_started_at).count();
      language_lock_wait_total_us += wait_us;
      ++language_lock_wait_samples;
      if (wait_us > 900) {
        ++language_lock_contention_samples;
      }

      if (now >= next_language_lock_log_at && language_lock_wait_samples > 0) {
        const long long avg_wait_us = language_lock_wait_total_us / language_lock_wait_samples;
        Logger::Instance().Log(
            LogLevel::Info,
            "Overlay mutex telemetry: samples=" + std::to_string(language_lock_wait_samples) +
                ", avg_wait_us=" + std::to_string(avg_wait_us) +
                ", contention_samples=" + std::to_string(language_lock_contention_samples));
        language_lock_wait_total_us = 0;
        language_lock_wait_samples = 0;
        language_lock_contention_samples = 0;
        next_language_lock_log_at = now + seconds(60);
      }

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

    if (config_.wp_notifications_enabled) {
      const NotificationItem notification = notification_monitor_.PopLatest();
      if (notification.has_value && IsNotificationSourceEnabled(notification.app)) {
        client_.SendNotificationEvent(notification.app, i18n_.Text("notification_generic"));
        overlay_type_ = OverlayType::Notification;
        overlay_end_time_ = now + seconds(3);
      }
    } else {
      notification_monitor_.PopLatest();
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
      auto next_overlay_wakeup = now + loop_sleep;
      if (overlay_end_time_ < next_overlay_wakeup) {
        next_overlay_wakeup = overlay_end_time_;
      }
      std::this_thread::sleep_until(next_overlay_wakeup);
      continue;
    }

    overlay_type_ = OverlayType::None;

    if (duration_cast<milliseconds>(now - last_game_poll) >= game_poll_interval) {
      game_mode_was_active = config_.game_mode_enabled && SendGameMode();
      last_game_poll = now;
    }

    CheckUpdatesPeriodically();

    const bool any_media_enabled =
        config_.media_spotify_enabled || config_.media_youtube_music_enabled ||
        config_.media_apple_music_enabled || config_.media_browser_enabled;

    if (!any_media_enabled) {
      if (spotify_was_active) {
        SendClock();
        spotify_was_active = false;
        next_clock_send_at = aligned_next_clock_tick();
      }
    }

    if (any_media_enabled && !game_mode_was_active &&
        duration_cast<milliseconds>(now - last_spotify_poll) >= spotify_poll_interval) {
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
        next_clock_send_at += clock_send_interval;
      }
    }

    const auto after_work = steady_clock::now();
    auto next_wakeup = after_work + loop_sleep;
    if (!spotify_was_active && next_clock_send_at < next_wakeup) {
      next_wakeup = next_clock_send_at;
    }

    if (next_wakeup > after_work) {
      std::this_thread::sleep_until(next_wakeup);
    }
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
