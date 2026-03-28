#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

namespace {

std::string GetArgValue(const std::string& key) {
  for (int i = 1; i < __argc - 1; ++i) {
    if (std::string(__argv[i]) == key) {
      return __argv[i + 1];
    }
  }
  return {};
}

void WaitForPid(const std::string& pid_text) {
  if (pid_text.empty()) {
    return;
  }

  int pid = 0;
  try {
    pid = std::stoi(pid_text);
  } catch (...) {
    return;
  }

  if (pid <= 0) {
    return;
  }

  HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
  if (process == nullptr) {
    return;
  }

  WaitForSingleObject(process, 30000);
  CloseHandle(process);
}

bool ReplaceWithRetries(const std::string& source, const std::string& target) {
  const std::string backup = target + ".bak";
  std::error_code ec;
  std::filesystem::remove(backup, ec);

  bool backup_created = false;
  for (int i = 0; i < 30; ++i) {
    if (MoveFileExA(target.c_str(), backup.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
      backup_created = true;
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  if (!backup_created && std::filesystem::exists(target, ec)) {
    return false;
  }

  for (int i = 0; i < 30; ++i) {
    if (MoveFileExA(source.c_str(), target.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
      if (backup_created) {
        std::filesystem::remove(backup, ec);
      }
      return true;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  if (backup_created) {
    MoveFileExA(backup.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
  }

  return false;
}

void LaunchTarget(const std::string& target_path) {
  if (target_path.empty()) {
    return;
  }

  std::string cmd = "\"" + target_path + "\"";
  STARTUPINFOA si{};
  PROCESS_INFORMATION pi{};
  si.cb = sizeof(si);

  if (CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
}

}  // namespace

int main() {
  const std::string source = GetArgValue("--source");
  const std::string target = GetArgValue("--target");
  const std::string pid = GetArgValue("--wait-pid");
  const std::string launch = GetArgValue("--launch");

  if (source.empty() || target.empty()) {
    return 1;
  }

  WaitForPid(pid);

  if (!ReplaceWithRetries(source, target)) {
    return 2;
  }

  std::error_code ec;
  std::filesystem::remove(source, ec);

  LaunchTarget(launch.empty() ? target : launch);
  return 0;
}
