#include "Config.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Logger.hpp"
#include "PathUtils.hpp"

namespace ssext {

namespace {

std::string Trim(const std::string& value) {
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

std::string ToLower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

int ClampInt(int value, int min_value, int max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

void ApplyLockedMetadata(Config& cfg) {
	cfg.app_id = "SSEXT";
	cfg.app_name = "Carex-Ext";
	cfg.developer = "OMERBABACO";
	cfg.app_version = "1.0";
	cfg.update_repo_owner = "babajoeltdsti";
	cfg.update_repo_name = "ss-ext";
}

void WriteConfig(std::ofstream& out, const Config& cfg) {
	out << "# Carex-Ext settings\n";
	out << "language=" << cfg.language << "\n";
	out << "auto_update_check=" << (cfg.auto_update_check ? "true" : "false") << "\n";
	out << "auto_update_apply=" << (cfg.auto_update_apply ? "true" : "false") << "\n";
	out << "auto_update_require_checksum="
			<< (cfg.auto_update_require_checksum ? "true" : "false") << "\n";
	out << "auto_update_check_interval_minutes=" << cfg.auto_update_check_interval_minutes << "\n";
	out << "auto_start_task_enabled=" << (cfg.auto_start_task_enabled ? "true" : "false") << "\n";
	out << "auto_start_task_name=" << cfg.auto_start_task_name << "\n";
	out << "game_mode_enabled=" << (cfg.game_mode_enabled ? "true" : "false") << "\n";
	out << "media_spotify_enabled=" << (cfg.media_spotify_enabled ? "true" : "false") << "\n";
	out << "media_youtube_music_enabled=" << (cfg.media_youtube_music_enabled ? "true" : "false")
			<< "\n";
	out << "media_apple_music_enabled=" << (cfg.media_apple_music_enabled ? "true" : "false")
			<< "\n";
	out << "media_browser_enabled=" << (cfg.media_browser_enabled ? "true" : "false") << "\n";
	out << "wp_notifications_enabled=" << (cfg.wp_notifications_enabled ? "true" : "false") << "\n";
	out << "notification_whatsapp_enabled="
			<< (cfg.notification_whatsapp_enabled ? "true" : "false") << "\n";
	out << "notification_discord_enabled="
			<< (cfg.notification_discord_enabled ? "true" : "false") << "\n";
	out << "notification_instagram_enabled="
			<< (cfg.notification_instagram_enabled ? "true" : "false") << "\n";
	out << "email_enabled=" << (cfg.email_enabled ? "true" : "false") << "\n";
	out << "email_check_interval_seconds=" << cfg.email_check_interval_seconds << "\n";
	out << "email_imap_server=" << cfg.email_imap_server << "\n";
	out << "email_imap_port=" << cfg.email_imap_port << "\n";
	out << "email_imap_ssl=" << (cfg.email_imap_ssl ? "true" : "false") << "\n";
	out << "email_smtp_server=" << cfg.email_smtp_server << "\n";
	out << "email_smtp_port=" << cfg.email_smtp_port << "\n";
	out << "email_smtp_ssl=" << (cfg.email_smtp_ssl ? "true" : "false") << "\n";
	out << "email_username=" << cfg.email_username << "\n";
	out << "email_credential_target=" << cfg.email_credential_target << "\n";
	out << "email_test_subject=" << cfg.email_test_subject << "\n";
	out << "email_test_sender=" << cfg.email_test_sender << "\n";
	out << "loop_interval_ms=" << cfg.loop_interval_ms << "\n";
	out << "clock_send_interval_ms=" << cfg.clock_send_interval_ms << "\n";
	out << "clock_24h_format=" << (cfg.clock_24h_format ? "true" : "false") << "\n";
	out << "spotify_poll_interval_ms=" << cfg.spotify_poll_interval_ms << "\n";
	out << "spotify_marquee_shift_ms=" << cfg.spotify_marquee_shift_ms << "\n";
	out << "spotify_timeline_refresh_ms=" << cfg.spotify_timeline_refresh_ms << "\n";
}

void ParseConfigLine(const std::string& line, Config& cfg) {
	const std::string trimmed = Trim(line);
	if (trimmed.empty() || trimmed[0] == '#') {
		return;
	}

	const auto eq_pos = trimmed.find('=');
	if (eq_pos == std::string::npos) {
		return;
	}

	const std::string key = ToLower(Trim(trimmed.substr(0, eq_pos)));
	const std::string value = Trim(trimmed.substr(eq_pos + 1));

	try {
		if (key == "language") {
			const std::string lower = ToLower(value);
			if (lower == "en" || lower == "tr" || lower == "de" || lower == "fr" ||
						lower == "es") {
				cfg.language = lower;
			} else {
				cfg.language = "tr";
			}
		} else if (key == "auto_update_check") {
			cfg.auto_update_check = ToLower(value) == "true" || value == "1";
		} else if (key == "auto_update_apply") {
			cfg.auto_update_apply = ToLower(value) == "true" || value == "1";
		} else if (key == "auto_update_require_checksum") {
			cfg.auto_update_require_checksum = ToLower(value) == "true" || value == "1";
		} else if (key == "auto_update_check_interval_minutes") {
			cfg.auto_update_check_interval_minutes = ClampInt(std::stoi(value), 5, 1440);
		} else if (key == "auto_start_task_enabled") {
			cfg.auto_start_task_enabled = ToLower(value) == "true" || value == "1";
		} else if (key == "auto_start_task_name") {
			cfg.auto_start_task_name = value;
		} else if (key == "game_mode_enabled") {
			cfg.game_mode_enabled = ToLower(value) == "true" || value == "1";
		} else if (key == "media_spotify_enabled") {
			cfg.media_spotify_enabled = ToLower(value) == "true" || value == "1";
		} else if (key == "media_youtube_music_enabled") {
			cfg.media_youtube_music_enabled = ToLower(value) == "true" || value == "1";
		} else if (key == "media_apple_music_enabled") {
			cfg.media_apple_music_enabled = ToLower(value) == "true" || value == "1";
		} else if (key == "media_browser_enabled") {
			cfg.media_browser_enabled = ToLower(value) == "true" || value == "1";
		} else if (key == "wp_notifications_enabled") {
			cfg.wp_notifications_enabled = ToLower(value) == "true" || value == "1";
		} else if (key == "notification_whatsapp_enabled") {
			cfg.notification_whatsapp_enabled = ToLower(value) == "true" || value == "1";
		} else if (key == "notification_discord_enabled") {
			cfg.notification_discord_enabled = ToLower(value) == "true" || value == "1";
		} else if (key == "notification_instagram_enabled") {
			cfg.notification_instagram_enabled = ToLower(value) == "true" || value == "1";
		} else if (key == "email_enabled") {
			cfg.email_enabled = ToLower(value) == "true" || value == "1";
		} else if (key == "email_check_interval_seconds") {
			cfg.email_check_interval_seconds = ClampInt(std::stoi(value), 5, 600);
		} else if (key == "email_imap_server") {
			cfg.email_imap_server = value;
		} else if (key == "email_imap_port") {
			cfg.email_imap_port = ClampInt(std::stoi(value), 1, 65535);
		} else if (key == "email_imap_ssl") {
			cfg.email_imap_ssl = ToLower(value) == "true" || value == "1";
		} else if (key == "email_smtp_server") {
			cfg.email_smtp_server = value;
		} else if (key == "email_smtp_port") {
			cfg.email_smtp_port = ClampInt(std::stoi(value), 1, 65535);
		} else if (key == "email_smtp_ssl") {
			cfg.email_smtp_ssl = ToLower(value) == "true" || value == "1";
		} else if (key == "email_username") {
			cfg.email_username = value;
		} else if (key == "email_credential_target") {
			cfg.email_credential_target = value;
		} else if (key == "email_test_subject") {
			cfg.email_test_subject = value;
		} else if (key == "email_test_sender") {
			cfg.email_test_sender = value;
		} else if (key == "loop_interval_ms") {
			cfg.loop_interval_ms = ClampInt(std::stoi(value), 50, 2000);
		} else if (key == "clock_send_interval_ms") {
			cfg.clock_send_interval_ms = ClampInt(std::stoi(value), 250, 60000);
		} else if (key == "clock_24h_format") {
			cfg.clock_24h_format = ToLower(value) == "true" || value == "1";
		} else if (key == "spotify_poll_interval_ms") {
			cfg.spotify_poll_interval_ms = ClampInt(std::stoi(value), 50, 2000);
		} else if (key == "spotify_marquee_shift_ms") {
			cfg.spotify_marquee_shift_ms = ClampInt(std::stoi(value), 50, 1500);
		} else if (key == "spotify_timeline_refresh_ms") {
			cfg.spotify_timeline_refresh_ms = ClampInt(std::stoi(value), 100, 2000);
		}
	} catch (...) {
		Logger::Instance().Log(LogLevel::Warning, "settings.ini satiri parse edilemedi: " + line);
	}
}

std::string SanitizeProfileName(const std::string& profile_name) {
	std::string trimmed = Trim(profile_name);
	if (trimmed.empty()) {
		return "default";
	}

	std::string out;
	out.reserve(trimmed.size());
	for (char ch : trimmed) {
		const unsigned char c = static_cast<unsigned char>(ch);
		if (std::isalnum(c) != 0) {
			out.push_back(static_cast<char>(std::tolower(c)));
		} else if (ch == '-' || ch == '_') {
			out.push_back(ch);
		} else if (std::isspace(c) != 0) {
			out.push_back('_');
		}
	}

	if (out.empty()) {
		return "default";
	}

	return out;
}

}  // namespace

std::string Config::ProfilesDirectory(const std::string& app_data_dir) {
	return JoinPath(app_data_dir, "profiles");
}

std::string Config::NormalizeProfileName(const std::string& profile_name) {
	return SanitizeProfileName(profile_name);
}

std::string Config::ProfilePath(const std::string& app_data_dir,
																const std::string& profile_name) {
	const std::string profiles_dir = ProfilesDirectory(app_data_dir);
	const std::string normalized_name = NormalizeProfileName(profile_name);
	return JoinPath(profiles_dir, normalized_name + ".ini");
}

std::vector<std::string> Config::ListProfiles(const std::string& app_data_dir) {
	std::vector<std::string> out;
	const std::string profiles_dir = ProfilesDirectory(app_data_dir);
	std::error_code ec;
	if (!std::filesystem::exists(profiles_dir, ec)) {
		out.push_back("default");
		return out;
	}

	for (const auto& entry : std::filesystem::directory_iterator(profiles_dir, ec)) {
		if (ec || !entry.is_regular_file(ec)) {
			continue;
		}

		const auto path = entry.path();
		if (path.extension() != ".ini") {
			continue;
		}

		const std::string stem = path.stem().string();
		const std::string normalized = NormalizeProfileName(stem);
		if (!normalized.empty()) {
			out.push_back(normalized);
		}
	}

	if (out.empty()) {
		out.push_back("default");
	}

	std::sort(out.begin(), out.end());
	out.erase(std::unique(out.begin(), out.end()), out.end());
	return out;
}

void Config::SaveTemplateIfMissing(const std::string& app_data_dir) {
	const std::string settings_path = JoinPath(app_data_dir, "settings.ini");
	SaveTemplateIfMissingAtPath(settings_path);
}

void Config::SaveTemplateIfMissingAtPath(const std::string& settings_path) {
	std::ifstream check(settings_path);
	if (check.good()) {
		return;
	}

	Config defaults;
	ApplyLockedMetadata(defaults);
	if (!SaveToFile(settings_path, defaults)) {
		Logger::Instance().Log(LogLevel::Warning, "settings.ini olusturulamadi.");
	}
}

bool Config::Save(const std::string& app_data_dir, const Config& cfg) {
	const std::string settings_path = JoinPath(app_data_dir, "settings.ini");
	return SaveToFile(settings_path, cfg);
}

bool Config::SaveToFile(const std::string& settings_path, const Config& cfg) {
	const std::filesystem::path file_path(settings_path);
	std::error_code ec;
	const auto parent = file_path.parent_path();
	if (!parent.empty()) {
		std::filesystem::create_directories(parent, ec);
	}

	std::ofstream out(settings_path, std::ios::trunc);
	if (!out.is_open()) {
		Logger::Instance().Log(LogLevel::Warning, "settings.ini yazilamadi: " + settings_path);
		return false;
	}

	Config to_write = cfg;
	ApplyLockedMetadata(to_write);
	WriteConfig(out, to_write);
	return true;
}

Config Config::Load(const std::string& app_data_dir) {
	const std::string settings_path = JoinPath(app_data_dir, "settings.ini");
	return LoadFromFile(settings_path);
}

Config Config::LoadFromFile(const std::string& settings_path) {
	Config cfg;
	ApplyLockedMetadata(cfg);

	std::ifstream in(settings_path);
	if (!in.is_open()) {
		Logger::Instance().Log(LogLevel::Info,
													 "settings.ini bulunamadi, varsayilan config kullaniliyor: " +
															 settings_path);
		ApplyLockedMetadata(cfg);
		return cfg;
	}

	std::string line;
	while (std::getline(in, line)) {
		ParseConfigLine(line, cfg);
	}

	ApplyLockedMetadata(cfg);
	return cfg;
}

}  // namespace ssext
