#include "update/UpdateChecker.hpp"

#include <Windows.h>
#include <winhttp.h>

#include <fstream>
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace ssext {

namespace {

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

std::vector<int> SplitVersion(const std::string& version) {
  std::vector<int> parts;
  std::stringstream ss(version);
  std::string item;
  while (std::getline(ss, item, '.')) {
    try {
      parts.push_back(std::stoi(item));
    } catch (...) {
      parts.push_back(0);
    }
  }
  return parts;
}

}  // namespace

std::string UpdateChecker::NormalizeVersion(std::string version) {
  if (!version.empty() && (version[0] == 'v' || version[0] == 'V')) {
    version.erase(version.begin());
  }
  return version;
}

bool UpdateChecker::ParseTagName(const std::string& json, std::string& tag_name) {
  const std::string key = "\"tag_name\"";
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

  tag_name = json.substr(first_quote + 1, second_quote - first_quote - 1);
  return !tag_name.empty();
}

bool UpdateChecker::ParseAssetDownload(const std::string& json, std::string& download_url,
                                       std::string& asset_name) {
  const std::string url_key = "\"browser_download_url\"";
  const std::string name_key = "\"name\"";

  std::size_t pos = 0;
  while (true) {
    const auto key_pos = json.find(url_key, pos);
    if (key_pos == std::string::npos) {
      break;
    }

    const auto colon_pos = json.find(':', key_pos + url_key.size());
    const auto first_quote = json.find('"', colon_pos + 1);
    const auto second_quote = json.find('"', first_quote + 1);
    if (colon_pos == std::string::npos || first_quote == std::string::npos ||
        second_quote == std::string::npos || second_quote <= first_quote + 1) {
      pos = key_pos + url_key.size();
      continue;
    }

    const std::string candidate_url =
        json.substr(first_quote + 1, second_quote - first_quote - 1);

    std::string candidate_name;
    const auto name_pos = json.rfind(name_key, key_pos);
    if (name_pos != std::string::npos) {
      const auto name_colon = json.find(':', name_pos + name_key.size());
      const auto name_q1 = json.find('"', name_colon + 1);
      const auto name_q2 = json.find('"', name_q1 + 1);
      if (name_colon != std::string::npos && name_q1 != std::string::npos &&
          name_q2 != std::string::npos && name_q2 > name_q1 + 1) {
        candidate_name = json.substr(name_q1 + 1, name_q2 - name_q1 - 1);
      }
    }

    if (candidate_url.find(".exe") != std::string::npos) {
      download_url = candidate_url;
      asset_name = candidate_name.empty() ? "ss-ext-update.exe" : candidate_name;
      return true;
    }

    if (download_url.empty()) {
      download_url = candidate_url;
      asset_name = candidate_name.empty() ? "ss-ext-update.bin" : candidate_name;
    }

    pos = key_pos + url_key.size();
  }

  return !download_url.empty();
}

bool UpdateChecker::IsVersionNewer(const std::string& latest, const std::string& current) {
  const std::vector<int> latest_parts = SplitVersion(NormalizeVersion(latest));
  const std::vector<int> current_parts = SplitVersion(NormalizeVersion(current));
  const std::size_t max_parts = (std::max)(latest_parts.size(), current_parts.size());

  for (std::size_t i = 0; i < max_parts; ++i) {
    const int l = i < latest_parts.size() ? latest_parts[i] : 0;
    const int c = i < current_parts.size() ? current_parts[i] : 0;
    if (l > c) {
      return true;
    }
    if (l < c) {
      return false;
    }
  }

  return false;
}

UpdateInfo UpdateChecker::CheckLatest(const std::string& owner, const std::string& repo,
                                      const std::string& current_version) const {
  UpdateInfo info;

  const std::wstring user_agent = L"SS-EXT-Cpp/0.1";
  HINTERNET h_session = WinHttpOpen(user_agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!h_session) {
    return info;
  }

  HINTERNET h_connect = WinHttpConnect(h_session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!h_connect) {
    WinHttpCloseHandle(h_session);
    return info;
  }

  const std::string path = "/repos/" + owner + "/" + repo + "/releases/latest";
  std::wstring wide_path = ToWide(path);
  HINTERNET h_request = WinHttpOpenRequest(h_connect, L"GET", wide_path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
  if (!h_request) {
    WinHttpCloseHandle(h_connect);
    WinHttpCloseHandle(h_session);
    return info;
  }

  const std::wstring headers = L"Accept: application/vnd.github+json\r\n";
  BOOL sent = WinHttpSendRequest(h_request, headers.c_str(), static_cast<DWORD>(-1),
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

  if (sent && WinHttpReceiveResponse(h_request, nullptr)) {
    DWORD status_code = 0;
    DWORD size = sizeof(status_code);
    if (WinHttpQueryHeaders(h_request,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &size,
                            WINHTTP_NO_HEADER_INDEX)) {
      if (status_code >= 200 && status_code < 300) {
        const std::string body = ReadResponseBody(h_request);
        std::string tag;
        if (ParseTagName(body, tag)) {
          info.request_ok = true;
          info.latest_version = NormalizeVersion(tag);
          info.has_update = IsVersionNewer(info.latest_version, current_version);
          ParseAssetDownload(body, info.download_url, info.asset_name);
          if (!info.download_url.empty()) {
            info.checksum_url = info.download_url + ".sha256";
          }
        }
      }
    }
  }

  WinHttpCloseHandle(h_request);
  WinHttpCloseHandle(h_connect);
  WinHttpCloseHandle(h_session);
  return info;
}

bool UpdateChecker::DownloadToFile(const std::string& url, const std::string& file_path) const {
  std::wstring wide_url = ToWide(url);
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

  std::wstring host_name(uc.lpszHostName, uc.dwHostNameLength);
  std::wstring url_path(uc.lpszUrlPath, uc.dwUrlPathLength);
  const bool secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);

  HINTERNET h_session = WinHttpOpen(L"SS-EXT-Cpp/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
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

  std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    WinHttpCloseHandle(h_request);
    WinHttpCloseHandle(h_connect);
    WinHttpCloseHandle(h_session);
    return false;
  }

  bool ok = false;
  BOOL sent = WinHttpSendRequest(h_request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
  if (sent && WinHttpReceiveResponse(h_request, nullptr)) {
    DWORD status_code = 0;
    DWORD size = sizeof(status_code);
    if (WinHttpQueryHeaders(h_request,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &size,
                            WINHTTP_NO_HEADER_INDEX)) {
      if (status_code >= 200 && status_code < 300) {
        DWORD available = 0;
        do {
          available = 0;
          if (!WinHttpQueryDataAvailable(h_request, &available) || available == 0) {
            break;
          }

          std::vector<char> buffer(available);
          DWORD read = 0;
          if (!WinHttpReadData(h_request, buffer.data(), available, &read) || read == 0) {
            break;
          }
          out.write(buffer.data(), static_cast<std::streamsize>(read));
        } while (available > 0);

        out.flush();
        ok = out.good();
      }
    }
  }

  out.close();
  WinHttpCloseHandle(h_request);
  WinHttpCloseHandle(h_connect);
  WinHttpCloseHandle(h_session);
  return ok;
}

bool UpdateChecker::DownloadToString(const std::string& url, std::string& out_text) const {
  out_text.clear();

  std::wstring wide_url = ToWide(url);
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

  std::wstring host_name(uc.lpszHostName, uc.dwHostNameLength);
  std::wstring url_path(uc.lpszUrlPath, uc.dwUrlPathLength);
  const bool secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);

  HINTERNET h_session = WinHttpOpen(L"SS-EXT-Cpp/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
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
  BOOL sent = WinHttpSendRequest(h_request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
  if (sent && WinHttpReceiveResponse(h_request, nullptr)) {
    DWORD status_code = 0;
    DWORD size = sizeof(status_code);
    if (WinHttpQueryHeaders(h_request,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &size,
                            WINHTTP_NO_HEADER_INDEX)) {
      if (status_code >= 200 && status_code < 300) {
        out_text = ReadResponseBody(h_request);
        ok = !out_text.empty();
      }
    }
  }

  WinHttpCloseHandle(h_request);
  WinHttpCloseHandle(h_connect);
  WinHttpCloseHandle(h_session);
  return ok;
}

}  // namespace ssext
