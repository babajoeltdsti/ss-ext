#include "GameSenseClient.hpp"

#include <Windows.h>
#include <winhttp.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Logger.hpp"

#pragma comment(lib, "winhttp.lib")

namespace ssext {

GameSenseClient::GameSenseClient(Config config) : config_(std::move(config)) {}

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

bool GameSenseClient::PostJson(const std::string& path, const std::string& body) {
  if (server_address_.empty()) {
    Logger::Instance().Log(LogLevel::Error, "Server adresi bos, once DiscoverServer cagrilmali.");
    return false;
  }

  const std::wstring user_agent = L"SS-EXT-Cpp/0.1";
  HINTERNET h_session = WinHttpOpen(user_agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!h_session) {
    Logger::Instance().Log(LogLevel::Error, "WinHttpOpen basarisiz.");
    return false;
  }

  std::wstring host(server_host_.begin(), server_host_.end());
  if (host.empty()) {
    host = L"127.0.0.1";
  }
  const INTERNET_PORT port = static_cast<INTERNET_PORT>(server_port_);
  HINTERNET h_connect = WinHttpConnect(h_session, host.c_str(), port, 0);
  if (!h_connect) {
    WinHttpCloseHandle(h_session);
    Logger::Instance().Log(LogLevel::Error, "WinHttpConnect basarisiz.");
    return false;
  }

  std::wstring wide_path(path.begin(), path.end());
  HINTERNET h_request = WinHttpOpenRequest(
      h_connect, L"POST", wide_path.c_str(), nullptr, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
  if (!h_request) {
    WinHttpCloseHandle(h_connect);
    WinHttpCloseHandle(h_session);
    Logger::Instance().Log(LogLevel::Error, "WinHttpOpenRequest basarisiz: " + path);
    return false;
  }

  constexpr DWORD timeout_ms = 2000;
  WinHttpSetTimeouts(h_request, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

  const std::wstring headers = L"Content-Type: application/json\r\n";
  BOOL sent = WinHttpSendRequest(h_request, headers.c_str(), static_cast<DWORD>(-1),
                                 (LPVOID)body.data(), static_cast<DWORD>(body.size()),
                                 static_cast<DWORD>(body.size()), 0);

  bool ok = false;
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
  WinHttpCloseHandle(h_connect);
  WinHttpCloseHandle(h_session);

  if (!ok) {
    Logger::Instance().Log(LogLevel::Warning, "GameSense POST basarisiz: " + path);
  }

  return ok;
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

  return PostJson("/game_event", body);
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

  return PostJson("/game_event", body);
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

  return PostJson("/game_event", body);
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

  return PostJson("/game_event", body);
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

  return PostJson("/game_event", body);
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

  return PostJson("/game_event", body);
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

  return PostJson("/game_event", body);
}

}  // namespace ssext
