#include "GameSenseClient.hpp"

#include <Windows.h>
#include <winhttp.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <chrono>

#include "Logger.hpp"

#pragma comment(lib, "winhttp.lib")

namespace ssext {

namespace {

constexpr DWORD kHttpTimeoutMs = 350;

}  // namespace

GameSenseClient::GameSenseClient(Config config) : config_(std::move(config)) {}

GameSenseClient::~GameSenseClient() {
  std::lock_guard<std::mutex> lock(http_mutex_);
  ResetHttpConnectionLocked();
}

void GameSenseClient::SetConfig(const Config& config) {
  std::lock_guard<std::mutex> lock(http_mutex_);
  config_ = config;
}

std::string GameSenseClient::ReadFile(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {};
  }

  std::ostringstream oss;
  oss << input.rdbuf();
  return oss.str();
}

bool GameSenseClient::ParseAddressFromCoreProps(const std::string& json, std::string& out_address) {
  const std::string key = "\"address\"";
  const auto key_pos = json.find(key);
  if (key_pos == std::string::npos) {
    return false;
  }

  const auto colon_pos = json.find(':', key_pos + key.size());
  if (colon_pos == std::string::npos) {
    return false;
  }

  const auto first_quote = json.find('"', colon_pos + 1);
  if (first_quote == std::string::npos) {
    return false;
  }

  const auto second_quote = json.find('"', first_quote + 1);
  if (second_quote == std::string::npos || second_quote <= first_quote + 1) {
    return false;
  }

  out_address = json.substr(first_quote + 1, second_quote - first_quote - 1);
  return !out_address.empty();
}

bool GameSenseClient::DiscoverServer() {
  char* program_data = nullptr;
  std::size_t len = 0;
  if (_dupenv_s(&program_data, &len, "PROGRAMDATA") != 0 || program_data == nullptr) {
    Logger::Instance().Log(LogLevel::Error, "PROGRAMDATA env alinamadi.");
    return false;
  }

  const std::string core_props_path =
      std::string(program_data) + "\\SteelSeries\\SteelSeries Engine 3\\coreProps.json";
  free(program_data);

  const std::string core_props = ReadFile(core_props_path);
  if (core_props.empty()) {
    Logger::Instance().Log(LogLevel::Error,
                           "coreProps.json okunamadi: " + core_props_path +
                               " (SteelSeries GG/Engine acik mi?)");
    return false;
  }

  std::string address;
  if (!ParseAddressFromCoreProps(core_props, address)) {
    Logger::Instance().Log(LogLevel::Error,
                           "coreProps.json icinden address parse edilemedi.");
    return false;
  }

  server_address_ = address;
  server_host_ = "127.0.0.1";
  server_port_ = 62501;

  const auto colon = address.rfind(':');
  if (colon != std::string::npos && colon + 1 < address.size()) {
    const std::string maybe_host = address.substr(0, colon);
    const std::string maybe_port = address.substr(colon + 1);
    try {
      const int port_value = std::stoi(maybe_port);
      if (port_value > 0 && port_value <= 65535) {
        server_host_ = maybe_host;
        server_port_ = static_cast<unsigned short>(port_value);
      }
    } catch (...) {
      Logger::Instance().Log(LogLevel::Warning,
                             "Address parse kismi basarisiz, varsayilan host/port kullaniliyor.");
    }
  }

  Logger::Instance().Log(LogLevel::Info, "GameSense server bulundu: " + server_address_);

  {
    std::lock_guard<std::mutex> lock(http_mutex_);
    ResetHttpConnectionLocked();
    consecutive_post_failures_ = 0;
    retry_after_ = std::chrono::steady_clock::time_point{};
    last_failure_log_at_ = std::chrono::steady_clock::time_point{};
    event_coalesce_cache_.clear();
    http_lock_wait_total_us_ = 0;
    http_lock_wait_samples_ = 0;
    http_lock_contention_samples_ = 0;
    next_lock_metrics_log_at_ = std::chrono::steady_clock::time_point{};
  }

  return true;
}

std::string GameSenseClient::EscapeJson(const std::string& text) {
  std::string out;
  out.reserve(text.size());

  for (char c : text) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }

  return out;
}

bool GameSenseClient::EnsureHttpConnectionLocked() {
  if (session_handle_ == nullptr) {
    const std::wstring user_agent = L"Carex-Ext/0.2";
    session_handle_ = WinHttpOpen(user_agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session_handle_ == nullptr) {
      return false;
    }
  }

  if (connect_handle_ == nullptr) {
    std::wstring host(server_host_.begin(), server_host_.end());
    if (host.empty()) {
      host = L"127.0.0.1";
    }

    connect_handle_ =
        WinHttpConnect(session_handle_, host.c_str(), static_cast<INTERNET_PORT>(server_port_), 0);
    if (connect_handle_ == nullptr) {
      ResetHttpConnectionLocked();
      return false;
    }
  }

  return true;
}

void GameSenseClient::ResetHttpConnectionLocked() {
  if (connect_handle_ != nullptr) {
    WinHttpCloseHandle(connect_handle_);
    connect_handle_ = nullptr;
  }

  if (session_handle_ != nullptr) {
    WinHttpCloseHandle(session_handle_);
    session_handle_ = nullptr;
  }
}

void GameSenseClient::TrackHttpLockWaitLocked(
    long long wait_microseconds,
    const std::chrono::steady_clock::time_point& now) {
  if (wait_microseconds < 0) {
    wait_microseconds = 0;
  }

  http_lock_wait_total_us_ += wait_microseconds;
  ++http_lock_wait_samples_;
  if (wait_microseconds > 900) {
    ++http_lock_contention_samples_;
  }

  if (next_lock_metrics_log_at_ == std::chrono::steady_clock::time_point{}) {
    next_lock_metrics_log_at_ = now + std::chrono::seconds(60);
  }

  if (now >= next_lock_metrics_log_at_ && http_lock_wait_samples_ > 0) {
    const long long avg_wait_us = http_lock_wait_total_us_ / http_lock_wait_samples_;
    Logger::Instance().Log(
        LogLevel::Info,
        "GameSense mutex telemetry: samples=" + std::to_string(http_lock_wait_samples_) +
            ", avg_wait_us=" + std::to_string(avg_wait_us) +
            ", contention_samples=" + std::to_string(http_lock_contention_samples_));

    http_lock_wait_total_us_ = 0;
    http_lock_wait_samples_ = 0;
    http_lock_contention_samples_ = 0;
    next_lock_metrics_log_at_ = now + std::chrono::seconds(60);
  }
}

bool GameSenseClient::PostJson(const std::string& path, const std::string& body) {
  if (server_address_.empty()) {
    Logger::Instance().Log(LogLevel::Error, "Server adresi bos, once DiscoverServer cagrilmali.");
    return false;
  }

  const auto now = std::chrono::steady_clock::now();

  {
    const auto lock_started_at = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(http_mutex_);
    const long long wait_us = std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::steady_clock::now() - lock_started_at)
                              .count();
    TrackHttpLockWaitLocked(wait_us, now);
    if (retry_after_ != std::chrono::steady_clock::time_point{} && now < retry_after_) {
      return false;
    }
  }

  bool ok = false;
  for (int attempt = 0; attempt < 2 && !ok; ++attempt) {
    const auto lock_started_at = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(http_mutex_);
    const auto lock_now = std::chrono::steady_clock::now();
    const long long wait_us =
        std::chrono::duration_cast<std::chrono::microseconds>(lock_now - lock_started_at).count();
    TrackHttpLockWaitLocked(wait_us, lock_now);

    if (!EnsureHttpConnectionLocked()) {
      break;
    }

    std::wstring wide_path(path.begin(), path.end());
    HINTERNET h_request = WinHttpOpenRequest(
        connect_handle_, L"POST", wide_path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (h_request == nullptr) {
      ResetHttpConnectionLocked();
      continue;
    }

    WinHttpSetTimeouts(h_request, kHttpTimeoutMs, kHttpTimeoutMs, kHttpTimeoutMs, kHttpTimeoutMs);

    const std::wstring headers = L"Content-Type: application/json\r\nConnection: keep-alive\r\n";
    const BOOL sent = WinHttpSendRequest(h_request, headers.c_str(), static_cast<DWORD>(-1),
                                         const_cast<char*>(body.data()),
                                         static_cast<DWORD>(body.size()),
                                         static_cast<DWORD>(body.size()), 0);

    if (sent && WinHttpReceiveResponse(h_request, nullptr)) {
      DWORD status_code = 0;
      DWORD size = sizeof(status_code);
      if (WinHttpQueryHeaders(h_request,
                              WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                              WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &size,
                              WINHTTP_NO_HEADER_INDEX)) {
        ok = status_code >= 200 && status_code < 300;
      }
    }

    WinHttpCloseHandle(h_request);

    if (!ok) {
      ResetHttpConnectionLocked();
    }
  }

  {
    const auto lock_started_at = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(http_mutex_);
    const auto lock_now = std::chrono::steady_clock::now();
    const long long wait_us =
        std::chrono::duration_cast<std::chrono::microseconds>(lock_now - lock_started_at).count();
    TrackHttpLockWaitLocked(wait_us, lock_now);

    if (ok) {
      if (consecutive_post_failures_ > 0) {
        Logger::Instance().Log(LogLevel::Info, "GameSense baglantisi normale dondu.");
      }
      consecutive_post_failures_ = 0;
      retry_after_ = std::chrono::steady_clock::time_point{};
      return true;
    }

    ++consecutive_post_failures_;
    if (consecutive_post_failures_ >= 4) {
      retry_after_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(1200);
    }

    const auto log_now = std::chrono::steady_clock::now();
    if (last_failure_log_at_ == std::chrono::steady_clock::time_point{} ||
        std::chrono::duration_cast<std::chrono::seconds>(log_now - last_failure_log_at_).count() >=
            15) {
      last_failure_log_at_ = log_now;
      Logger::Instance().Log(LogLevel::Warning,
                             "GameSense POST gecikiyor, tekrar denenecek: " + path);
    }
  }

  return false;
}

bool GameSenseClient::PostGameEvent(const std::string& event_name, const std::string& body) {
  const auto now = std::chrono::steady_clock::now();
  {
    const auto lock_started_at = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(http_mutex_);
    const long long wait_us = std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::steady_clock::now() - lock_started_at)
                              .count();
    TrackHttpLockWaitLocked(wait_us, now);

    auto& entry = event_coalesce_cache_[event_name];
    if (!entry.last_body.empty() && entry.last_body == body &&
        entry.last_sent_at != std::chrono::steady_clock::time_point{} &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now - entry.last_sent_at).count() <=
            120) {
      return true;
    }
  }

  const bool posted = PostJson("/game_event", body);
  if (!posted) {
    return false;
  }

  const auto lock_started_at = std::chrono::steady_clock::now();
  std::unique_lock<std::mutex> lock(http_mutex_);
  const long long wait_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - lock_started_at)
                            .count();
  TrackHttpLockWaitLocked(wait_us, std::chrono::steady_clock::now());
  auto& entry = event_coalesce_cache_[event_name];
  entry.last_body = body;
  entry.last_sent_at = now;
  return true;
}

bool GameSenseClient::RegisterGame() {
  const std::string body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"game_display_name\":\"" + EscapeJson(config_.app_name) + "\"," +
      "\"developer\":\"" + EscapeJson(config_.developer) + "\"," +
      "\"deinitialize_timer_length_ms\":" + std::to_string(config_.deinitialize_timer_ms) +
      "}";

  return PostJson("/game_metadata", body);
}

bool GameSenseClient::SetupClockHandler() {
  const std::string register_body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"CLOCK\"," +
      "\"value_optional\":true"
      "}";

  if (!PostJson("/register_game_event", register_body)) {
    Logger::Instance().Log(LogLevel::Error, "CLOCK register_game_event basarisiz.");
    return false;
  }

  const std::string bind_body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"CLOCK\"," +
      "\"handlers\":[{"
      "\"device-type\":\"screened\"," +
      "\"zone\":\"one\"," +
      "\"mode\":\"screen\"," +
      "\"datas\":[{"
      "\"icon-id\":15,"
      "\"lines\":["
      "{\"has-text\":true,\"context-frame-key\":\"time\",\"bold\":true},"
      "{\"has-text\":true,\"context-frame-key\":\"date\"}"
      "]"
      "}]"
      "}]"
      "}";

  if (!PostJson("/bind_game_event", bind_body)) {
    Logger::Instance().Log(LogLevel::Error, "CLOCK bind_game_event basarisiz.");
    return false;
  }

  return true;
}

bool GameSenseClient::SetupSpotifyHandler() {
  const std::string register_body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"SPOTIFY\"," +
      "\"value_optional\":true"
      "}";

  if (!PostJson("/register_game_event", register_body)) {
    Logger::Instance().Log(LogLevel::Error, "SPOTIFY register_game_event basarisiz.");
    return false;
  }

  const std::string bind_body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"SPOTIFY\"," +
      "\"handlers\":[{"
      "\"device-type\":\"screened\"," +
      "\"zone\":\"one\"," +
      "\"mode\":\"screen\"," +
      "\"datas\":[{"
      "\"icon-id\":23,"
      "\"lines\":["
      "{\"has-text\":true,\"context-frame-key\":\"title\",\"bold\":true},"
      "{\"has-text\":true,\"context-frame-key\":\"artist\"},"
      "{\"has-text\":true,\"context-frame-key\":\"duration\"}"
      "]"
      "}]"
      "}]"
      "}";

  return PostJson("/bind_game_event", bind_body);
}

bool GameSenseClient::SetupVolumeHandler() {
  const std::string register_body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"VOLUME\"," +
      "\"value_optional\":true"
      "}";

  if (!PostJson("/register_game_event", register_body)) {
    Logger::Instance().Log(LogLevel::Error, "VOLUME register_game_event basarisiz.");
    return false;
  }

  const std::string bind_body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"VOLUME\"," +
      "\"handlers\":[{"
      "\"device-type\":\"screened\"," +
      "\"zone\":\"one\"," +
      "\"mode\":\"screen\"," +
      "\"datas\":[{"
      "\"icon-id\":0,"
      "\"lines\":["
      "{\"has-text\":true,\"context-frame-key\":\"title\",\"bold\":true},"
      "{\"has-text\":true,\"context-frame-key\":\"level\"}"
      "]"
      "}]"
      "}]"
      "}";

  return PostJson("/bind_game_event", bind_body);
}

bool GameSenseClient::SetupNotificationHandler() {
  const std::string register_body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"NOTIFICATION\"," +
      "\"value_optional\":true"
      "}";

  if (!PostJson("/register_game_event", register_body)) {
    Logger::Instance().Log(LogLevel::Error, "NOTIFICATION register_game_event basarisiz.");
    return false;
  }

  const std::string bind_body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"NOTIFICATION\"," +
      "\"handlers\":[{"
      "\"device-type\":\"screened\"," +
      "\"zone\":\"one\"," +
      "\"mode\":\"screen\"," +
      "\"datas\":[{"
      "\"icon-id\":0,"
      "\"lines\":["
      "{\"has-text\":true,\"context-frame-key\":\"app\",\"bold\":true},"
      "{\"has-text\":true,\"context-frame-key\":\"message\"}"
      "]"
      "}]"
      "}]"
      "}";

  return PostJson("/bind_game_event", bind_body);
}

bool GameSenseClient::SetupGameModeHandler() {
  const std::string register_body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"GAME_MODE\"," +
      "\"value_optional\":true"
      "}";

  if (!PostJson("/register_game_event", register_body)) {
    Logger::Instance().Log(LogLevel::Error, "GAME_MODE register_game_event basarisiz.");
    return false;
  }

  const std::string bind_body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"GAME_MODE\"," +
      "\"handlers\":[{"
      "\"device-type\":\"screened\"," +
      "\"zone\":\"one\"," +
      "\"mode\":\"screen\"," +
      "\"datas\":[{"
      "\"icon-id\":0,"
      "\"lines\":["
      "{\"has-text\":true,\"context-frame-key\":\"game\",\"bold\":true},"
      "{\"has-text\":true,\"context-frame-key\":\"info\"}"
      "]"
      "}]"
      "}]"
      "}";

  return PostJson("/bind_game_event", bind_body);
}

bool GameSenseClient::SetupUpdateHandler() {
  const std::string register_body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"UPDATE\"," +
      "\"value_optional\":true"
      "}";

  if (!PostJson("/register_game_event", register_body)) {
    Logger::Instance().Log(LogLevel::Error, "UPDATE register_game_event basarisiz.");
    return false;
  }

  const std::string bind_body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"UPDATE\"," +
      "\"handlers\":[{"
      "\"device-type\":\"screened\"," +
      "\"zone\":\"one\"," +
      "\"mode\":\"screen\"," +
      "\"datas\":[{"
      "\"icon-id\":0,"
      "\"lines\":["
      "{\"has-text\":true,\"context-frame-key\":\"title\",\"bold\":true},"
      "{\"has-text\":true,\"context-frame-key\":\"status\"}"
      "]"
      "}]"
      "}]"
      "}";

  return PostJson("/bind_game_event", bind_body);
}

bool GameSenseClient::SetupEmailHandler() {
  const std::string register_body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"EMAIL_NOTIFICATION\"," +
      "\"value_optional\":true"
      "}";

  if (!PostJson("/register_game_event", register_body)) {
    Logger::Instance().Log(LogLevel::Error, "EMAIL register_game_event basarisiz.");
    return false;
  }

  const std::string bind_body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"EMAIL_NOTIFICATION\"," +
      "\"handlers\":[{"
      "\"device-type\":\"screened\"," +
      "\"zone\":\"one\"," +
      "\"mode\":\"screen\"," +
      "\"datas\":[{"
      "\"icon-id\":19,"
      "\"lines\":["
      "{\"has-text\":true,\"context-frame-key\":\"subject\",\"bold\":true},"
      "{\"has-text\":true,\"context-frame-key\":\"sender\"}"
      "]"
      "}]"
      "}]"
      "}";

  return PostJson("/bind_game_event", bind_body);
}

bool GameSenseClient::SendClockEvent(const std::string& time_text, const std::string& date_text) {
  const auto value = static_cast<int>(GetTickCount64() % 100000);
  const std::string body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"CLOCK\"," +
      "\"data\":{"
      "\"value\":" + std::to_string(value) + ","
      "\"frame\":{"
      "\"time\":\"" + EscapeJson(time_text) + "\"," +
      "\"date\":\"" + EscapeJson(date_text) + "\""
      "}"
      "}"
      "}";

  return PostGameEvent("CLOCK", body);
}

bool GameSenseClient::SendSpotifyEvent(const std::string& title, const std::string& artist,
                                       const std::string& duration) {
  const auto value = static_cast<int>(GetTickCount64() % 100000);
  const std::string body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"SPOTIFY\"," +
      "\"data\":{"
      "\"value\":" + std::to_string(value) + ","
      "\"frame\":{"
      "\"title\":\"" + EscapeJson(title) + "\"," +
      "\"artist\":\"" + EscapeJson(artist) + "\"," +
      "\"duration\":\"" + EscapeJson(duration) + "\""
      "}"
      "}"
      "}";

  return PostGameEvent("SPOTIFY", body);
}

bool GameSenseClient::SendVolumeEvent(const std::string& title, const std::string& level) {
  const auto value = static_cast<int>(GetTickCount64() % 100000);
  const std::string body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"VOLUME\"," +
      "\"data\":{"
      "\"value\":" + std::to_string(value) + ","
      "\"frame\":{"
      "\"title\":\"" + EscapeJson(title) + "\"," +
      "\"level\":\"" + EscapeJson(level) + "\""
      "}"
      "}"
      "}";

  return PostGameEvent("VOLUME", body);
}

bool GameSenseClient::SendNotificationEvent(const std::string& app, const std::string& message) {
  const auto value = static_cast<int>(GetTickCount64() % 100000);
  const std::string body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"NOTIFICATION\"," +
      "\"data\":{"
      "\"value\":" + std::to_string(value) + ","
      "\"frame\":{"
      "\"app\":\"" + EscapeJson(app) + "\"," +
      "\"message\":\"" + EscapeJson(message) + "\""
      "}"
      "}"
      "}";

  return PostGameEvent("NOTIFICATION", body);
}

bool GameSenseClient::SendGameModeEvent(const std::string& game, const std::string& info) {
  const auto value = static_cast<int>(GetTickCount64() % 100000);
  const std::string body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"GAME_MODE\"," +
      "\"data\":{"
      "\"value\":" + std::to_string(value) + ","
      "\"frame\":{"
      "\"game\":\"" + EscapeJson(game) + "\"," +
      "\"info\":\"" + EscapeJson(info) + "\""
      "}"
      "}"
      "}";

  return PostGameEvent("GAME_MODE", body);
}

bool GameSenseClient::SendUpdateEvent(const std::string& title, const std::string& status) {
  const auto value = static_cast<int>(GetTickCount64() % 100000);
  const std::string body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"UPDATE\"," +
      "\"data\":{"
      "\"value\":" + std::to_string(value) + ","
      "\"frame\":{"
      "\"title\":\"" + EscapeJson(title) + "\"," +
      "\"status\":\"" + EscapeJson(status) + "\""
      "}"
      "}"
      "}";

  return PostGameEvent("UPDATE", body);
}

bool GameSenseClient::SendEmailEvent(const std::string& subject, const std::string& sender) {
  const auto value = static_cast<int>(GetTickCount64() % 100000);
  const std::string body =
      "{"
      "\"game\":\"" + EscapeJson(config_.app_id) + "\"," +
      "\"event\":\"EMAIL_NOTIFICATION\"," +
      "\"data\":{"
      "\"value\":" + std::to_string(value) + ","
      "\"frame\":{"
      "\"subject\":\"" + EscapeJson(subject) + "\"," +
      "\"sender\":\"" + EscapeJson(sender) + "\""
      "}"
      "}"
      "}";

  return PostGameEvent("EMAIL_NOTIFICATION", body);
}

}  // namespace ssext
