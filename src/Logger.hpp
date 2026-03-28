#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace ssext {

enum class LogLevel {
  Info,
  Warning,
  Error,
  Debug
};

class Logger {
 public:
  static Logger& Instance();
  void Initialize(const std::string& log_file_path);
  void Log(LogLevel level, const std::string& message);

 private:
  void RotateIfNeeded();

  Logger() = default;
  std::mutex mutex_;
  std::ofstream file_stream_;
  std::string log_file_path_;
};

}  // namespace ssext
