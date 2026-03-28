#include "startup/StartupManager.hpp"

#include <cstdlib>
#include <string>

namespace ssext {

bool StartupManager::RunCommand(const std::string& command) {
  const int code = std::system(command.c_str());
  return code == 0;
}

bool StartupManager::EnsureTaskEnabled(const std::string& task_name,
                                       const std::string& exe_path) const {
  const std::string query_cmd = "schtasks /Query /TN \"" + task_name + "\" >nul 2>nul";
  if (RunCommand(query_cmd)) {
    return true;
  }

  const std::string create_cmd =
      "schtasks /Create /SC ONLOGON /TN \"" + task_name + "\" /TR \"\\\"" +
      exe_path + "\\\"\" /F >nul 2>nul";
  return RunCommand(create_cmd);
}

bool StartupManager::RemoveTask(const std::string& task_name) const {
  const std::string delete_cmd = "schtasks /Delete /TN \"" + task_name + "\" /F >nul 2>nul";
  return RunCommand(delete_cmd);
}

}  // namespace ssext
