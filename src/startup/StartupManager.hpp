#pragma once

#include <string>

namespace ssext {

class StartupManager {
 public:
  bool EnsureTaskEnabled(const std::string& task_name, const std::string& exe_path) const;
  bool RemoveTask(const std::string& task_name) const;

 private:
  static bool RunCommand(const std::string& command);
};

}  // namespace ssext
