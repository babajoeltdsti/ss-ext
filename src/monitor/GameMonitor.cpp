#include "monitor/GameMonitor.hpp"

#include <Windows.h>
#include <TlHelp32.h>
#include <winhttp.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Logger.hpp"
#include "PathUtils.hpp"

#pragma comment(lib, "winhttp.lib")

namespace ssext {

namespace {

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::string Trim(const std::string& value) {
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }

  return value.substr(start, end - start);
}

std::wstring ToWide(const std::string& value) {
  if (value.empty()) {
    return {};
  }

  const int len = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
  if (len <= 0) {
    return {};
  }

  std::wstring out;
  out.resize(static_cast<std::size_t>(len - 1));
  MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), len);
  return out;
}

std::string WideToUtf8(const std::wstring& value) {
  if (value.empty()) {
    return {};
  }

  const int len = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (len <= 0) {
    return {};
  }

  std::string out;
  out.resize(static_cast<std::size_t>(len - 1));
  WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), len, nullptr, nullptr);
  return out;
}

std::string ReadResponseBody(HINTERNET request) {
  std::string body;
  DWORD available = 0;

  do {
    available = 0;
    if (!WinHttpQueryDataAvailable(request, &available)) {
      break;
    }
    if (available == 0) {
      break;
    }

    std::vector<char> buffer(available + 1, '\0');
    DWORD read = 0;
    if (!WinHttpReadData(request, buffer.data(), available, &read) || read == 0) {
      break;
    }

    body.append(buffer.data(), read);
  } while (available > 0);

  return body;
}

const std::unordered_map<std::string, std::string> kBuiltInKnownGames = {
    {"valorant.exe", "Valorant"},
    {"cs2.exe", "Counter-Strike 2"},
    {"csgo.exe", "Counter-Strike"},
    {"r5apex.exe", "Apex Legends"},
    {"fortniteclient-win64-shipping.exe", "Fortnite"},
    {"league of legends.exe", "League of Legends"},
    {"leagueclient.exe", "League of Legends"},
    {"riotclientservices.exe", "Riot Client"},
    {"eldenring.exe", "Elden Ring"},
    {"gta5.exe", "GTA V"},
    {"minecraftlauncher.exe", "Minecraft"},
    {"javaw.exe", "Minecraft"},
    {"dota2.exe", "Dota 2"},
    {"overwatch.exe", "Overwatch"},
    {"rocketleague.exe", "Rocket League"},
    {"rainbowsix.exe", "Rainbow Six Siege"},
    {"fifa23.exe", "EA Sports FC"},
    {"fc24.exe", "EA Sports FC"},
    {"callofduty.exe", "Call of Duty"},
    {"cod.exe", "Call of Duty"},
    {"pubg.exe", "PUBG"},
    {"palworld.exe", "Palworld"},
    {"starfield.exe", "Starfield"},
    {"wow.exe", "World of Warcraft"},
    {"diabloiv.exe", "Diablo IV"},
    {"pathofexile.exe", "Path of Exile"},
    {"terraria.exe", "Terraria"},
    {"robloxplayerbeta.exe", "Roblox"},
    {"solitaire.exe", "Microsoft Solitaire Collection"},
    {"microsoftsolitairecollection.exe", "Microsoft Solitaire Collection"},
    {"forzahorizon5.exe", "Forza Horizon 5"},
    {"forzahorizon4.exe", "Forza Horizon 4"},
    {"assassinscreed.exe", "Assassin's Creed"},
    {"cyberpunk2077.exe", "Cyberpunk 2077"},
    {"witcher3.exe", "The Witcher 3"},
};

constexpr const char* kKnownGamesRemoteUrl =
    "https://raw.githubusercontent.com/babajoeltdsti/ss-ext-cpp/main/data/known_games.txt";

}  // namespace

void GameMonitor::EnsureKnownGamesLoaded() {
  if (initialized_) {
    return;
  }

  known_games_ = kBuiltInKnownGames;
  LoadFromCacheIfAvailable();
  RefreshKnownGamesFromRemote();
  initialized_ = true;
}

void GameMonitor::LoadFromCacheIfAvailable() {
  const std::string app_data_dir = GetAppDataDirectory();
  if (app_data_dir.empty()) {
    return;
  }

  const std::string cache_path = JoinPath(app_data_dir, "known_games_cache.txt");
  std::ifstream in(cache_path);
  if (!in.is_open()) {
    return;
  }

  std::ostringstream oss;
  oss << in.rdbuf();
  MergeKnownGamesFromText(oss.str());
}

void GameMonitor::SaveCache(const std::string& text) const {
  const std::string app_data_dir = GetAppDataDirectory();
  if (app_data_dir.empty()) {
    return;
  }

  const std::string cache_path = JoinPath(app_data_dir, "known_games_cache.txt");
  std::ofstream out(cache_path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return;
  }

  out << text;
}

void GameMonitor::MergeKnownGamesFromText(const std::string& text) {
  std::istringstream iss(text);
  std::string line;
  int added = 0;
  while (std::getline(iss, line)) {
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
      continue;
    }

    auto eq = trimmed.find('=');
    if (eq == std::string::npos) {
      continue;
    }

    std::string exe = ToLower(Trim(trimmed.substr(0, eq)));
    std::string title = Trim(trimmed.substr(eq + 1));
    if (exe.empty() || title.empty()) {
      continue;
    }

    if (exe.size() < 4 || exe.substr(exe.size() - 4) != ".exe") {
      exe += ".exe";
    }

    known_games_[exe] = title;
    ++added;
  }

  if (added > 0) {
    Logger::Instance().Log(LogLevel::Info,
                           "Known games listesi guncellendi: " + std::to_string(added) + " kayit.");
  }
}

bool GameMonitor::DownloadToString(const std::string& url, std::string& out_text) const {
  out_text.clear();

  const std::wstring wide_url = ToWide(url);
  if (wide_url.empty()) {
    return false;
  }

  URL_COMPONENTS uc{};
  wchar_t host[256] = {0};
  wchar_t path[2048] = {0};
  uc.dwStructSize = sizeof(uc);
  uc.lpszHostName = host;
  uc.dwHostNameLength = static_cast<DWORD>(sizeof(host) / sizeof(host[0]));
  uc.lpszUrlPath = path;
  uc.dwUrlPathLength = static_cast<DWORD>(sizeof(path) / sizeof(path[0]));

  if (!WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()), 0, &uc)) {
    return false;
  }

  const std::wstring host_name(uc.lpszHostName, uc.dwHostNameLength);
  const std::wstring url_path(uc.lpszUrlPath, uc.dwUrlPathLength);
  const bool secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);

  HINTERNET h_session = WinHttpOpen(L"Carex-Ext/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!h_session) {
    return false;
  }

  HINTERNET h_connect = WinHttpConnect(h_session, host_name.c_str(), uc.nPort, 0);
  if (!h_connect) {
    WinHttpCloseHandle(h_session);
    return false;
  }

  HINTERNET h_request = WinHttpOpenRequest(
      h_connect, L"GET", url_path.c_str(), nullptr, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
  if (!h_request) {
    WinHttpCloseHandle(h_connect);
    WinHttpCloseHandle(h_session);
    return false;
  }

  bool ok = false;
  if (WinHttpSendRequest(h_request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                         WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
      WinHttpReceiveResponse(h_request, nullptr)) {
    DWORD status_code = 0;
    DWORD size = sizeof(status_code);
    if (WinHttpQueryHeaders(h_request,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &size,
                            WINHTTP_NO_HEADER_INDEX) &&
        status_code >= 200 && status_code < 300) {
      out_text = ReadResponseBody(h_request);
      ok = !out_text.empty();
    }
  }

  WinHttpCloseHandle(h_request);
  WinHttpCloseHandle(h_connect);
  WinHttpCloseHandle(h_session);
  return ok;
}

void GameMonitor::RefreshKnownGamesFromRemote() {
  const auto now = std::chrono::steady_clock::now();
  if (last_remote_refresh_ != std::chrono::steady_clock::time_point{} &&
      std::chrono::duration_cast<std::chrono::hours>(now - last_remote_refresh_).count() < 12) {
    return;
  }

  last_remote_refresh_ = now;

  std::string text;
  if (!DownloadToString(kKnownGamesRemoteUrl, text)) {
    return;
  }

  MergeKnownGamesFromText(text);
  SaveCache(text);
}

GameState GameMonitor::Poll() {
  GameState state;
  EnsureKnownGamesLoaded();
  RefreshKnownGamesFromRemote();

  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return state;
  }

  PROCESSENTRY32 entry{};
  entry.dwSize = sizeof(entry);

  std::string key;
  if (Process32First(snapshot, &entry)) {
    do {
      std::wstring ws(entry.szExeFile);
      const std::string exe = WideToUtf8(ws);
      const std::string lower = ToLower(exe);
      const auto it = known_games_.find(lower);
        if (it != known_games_.end()) {
        state.active = true;
        state.process_name = exe;
        state.display_name = it->second;
        key = lower;
        break;
      }
    } while (Process32Next(snapshot, &entry));
  }

  CloseHandle(snapshot);

  state.changed = key != last_game_key_;
  last_game_key_ = key;

  return state;
}

}  // namespace ssext
