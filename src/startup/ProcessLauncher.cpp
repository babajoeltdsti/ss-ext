#include "startup/ProcessLauncher.hpp"

#include <Windows.h>

#include <fstream>
#include <string>

#include "PathUtils.hpp"

namespace ssext {

namespace {

bool ScheduleScriptExecution(const std::string& script_path) {
  std::string command = "cmd.exe /C \"\"" + script_path + "\"\"";

  STARTUPINFOA si{};
  PROCESS_INFORMATION pi{};
  si.cb = sizeof(si);

  if (!CreateProcessA(nullptr,
                      command.data(),
                      nullptr,
                      nullptr,
                      FALSE,
                      CREATE_NO_WINDOW | DETACHED_PROCESS,
                      nullptr,
                      nullptr,
                      &si,
                      &pi)) {
    return false;
  }

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}

std::string BuildTempScriptPath(const std::string& file_name) {
  char temp_dir_buffer[MAX_PATH] = {0};
  const DWORD temp_dir_len =
      GetTempPathA(static_cast<DWORD>(sizeof(temp_dir_buffer)), temp_dir_buffer);
  if (temp_dir_len == 0 || temp_dir_len >= sizeof(temp_dir_buffer)) {
    return {};
  }

  return JoinPath(std::string(temp_dir_buffer), file_name);
}

}  // namespace

bool ProcessLauncher::ScheduleRestartAfterExit(const std::string& exe_path, DWORD process_id) {
  if (exe_path.empty() || process_id == 0) {
    return false;
  }

  std::string command = "\"" + exe_path + "\" --restart-wait " + std::to_string(process_id);

  STARTUPINFOA si{};
  PROCESS_INFORMATION pi{};
  si.cb = sizeof(si);

  if (!CreateProcessA(nullptr,
                      command.data(),
                      nullptr,
                      nullptr,
                      FALSE,
                      DETACHED_PROCESS,
                      nullptr,
                      nullptr,
                      &si,
                      &pi)) {
    return false;
  }

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}

bool ProcessLauncher::ScheduleReplaceAndRestartAfterExit(const std::string& source_path,
                                                         const std::string& destination_path,
                                                         DWORD process_id) {
  if (source_path.empty() || destination_path.empty()) {
    return false;
  }

  const std::string script_path = BuildTempScriptPath("carex-ext-update.cmd");
  if (script_path.empty()) {
    return false;
  }

  {
    std::ofstream script(script_path, std::ios::binary | std::ios::trunc);
    if (!script.is_open()) {
      return false;
    }

    script << "@echo off\r\n";
    script << "setlocal\r\n";
    script << "set \"PID=" << process_id << "\"\r\n";
    script << "set \"SRC=" << source_path << "\"\r\n";
    script << "set \"DST=" << destination_path << "\"\r\n";
    script << "for /L %%i in (1,1,180) do (\r\n";
    script << "  tasklist /FI \"PID eq %PID%\" 2>nul | find \" %PID% \" >nul\r\n";
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

  return ScheduleScriptExecution(script_path);
}

}  // namespace ssext
