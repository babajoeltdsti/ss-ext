#pragma once

#include <Windows.h>

#include <string>

namespace ssext {

class ProcessLauncher {
 public:
  static bool ScheduleRestartAfterExit(const std::string& exe_path, DWORD process_id);
  static bool ScheduleReplaceAndRestartAfterExit(const std::string& source_path,
                                                 const std::string& destination_path,
                                                 DWORD process_id);
};

}  // namespace ssext
