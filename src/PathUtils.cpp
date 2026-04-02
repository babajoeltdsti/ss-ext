#include "PathUtils.hpp"

#include <Windows.h>
#include <KnownFolders.h>
#include <ShlObj.h>

#include <filesystem>
#include <string>

namespace ssext {

bool EnsureDirectory(const std::string& path) {
  std::error_code ec;
  if (std::filesystem::exists(path, ec)) {
    return true;
  }
  return std::filesystem::create_directories(path, ec);
}

std::string JoinPath(const std::string& left, const std::string& right) {
  if (left.empty()) {
    return right;
  }
  if (left.back() == '\\') {
    return left + right;
  }
  return left + "\\" + right;
}

std::string GetAppDataDirectory() {
  PWSTR wide_path = nullptr;
  const HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &wide_path);
  if (FAILED(hr) || wide_path == nullptr) {
    return {};
  }

  int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, nullptr, 0, nullptr, nullptr);
  std::string utf8;
  if (utf8_len > 0) {
    utf8.resize(static_cast<std::size_t>(utf8_len - 1));
    WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, utf8.data(), utf8_len, nullptr, nullptr);
  }

  CoTaskMemFree(wide_path);

  if (utf8.empty()) {
    return {};
  }

  const std::string old_app_dir = JoinPath(utf8, "ss-ext-cpp");
  const std::string app_dir = JoinPath(utf8, "carex-ext");

  std::error_code ec;
  const bool new_dir_exists = std::filesystem::exists(app_dir, ec);
  EnsureDirectory(app_dir);

  if (!new_dir_exists && std::filesystem::exists(old_app_dir, ec)) {
    std::filesystem::copy(old_app_dir,
                          app_dir,
                          std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing,
                          ec);
  }

  return app_dir;
}

std::string GetAppDirectory() {
  const std::string exe = GetExecutablePath();
  if (exe.empty()) {
    return {};
  }

  std::filesystem::path exe_path(exe);
  std::error_code ec;
  return exe_path.parent_path().string();
}

std::string GetExecutablePath() {
  char buffer[MAX_PATH] = {0};
  const DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return {};
  }

  return std::string(buffer, len);
}

}  // namespace ssext
