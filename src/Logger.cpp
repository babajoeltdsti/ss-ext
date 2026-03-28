#include "Logger.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace ssext {

namespace {

const char* LevelToText(LogLevel level) {
  switch (level) {
    case LogLevel::Info:
      return "INFO";
    case LogLevel::Warning:
      return "WARN";
    case LogLevel::Error:
      return "ERROR";
    case LogLevel::Debug:
      return "DEBUG";
    default:
      return "INFO";
  }
}

std::string TimestampNow() {
  const auto now = std::chrono::system_clock::now();
  const auto tt = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  localtime_s(&tm, &tt);

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

}  // namespace

Logger& Logger::Instance() {
  static Logger logger;
  return logger;
}

void Logger::RotateIfNeeded() {
  if (log_file_path_.empty()) {
    return;
  }

  std::error_code ec;
  if (!std::filesystem::exists(log_file_path_, ec)) {
    return;
  }

  const auto size = std::filesystem::file_size(log_file_path_, ec);
  constexpr std::uintmax_t kMaxSize = 5 * 1024 * 1024;
  if (ec || size < kMaxSize) {
    return;
  }

  const std::string backup = log_file_path_ + ".1";
  std::filesystem::remove(backup, ec);
  ec.clear();
  std::filesystem::rename(log_file_path_, backup, ec);
}

void Logger::Initialize(const std::string& log_file_path) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (file_stream_.is_open()) {
    return;
  }
  log_file_path_ = log_file_path;
  RotateIfNeeded();
  file_stream_.open(log_file_path, std::ios::app);
}

void Logger::Log(LogLevel level, const std::string& message) {
  std::lock_guard<std::mutex> lock(mutex_);
  const std::string line = "[" + TimestampNow() + "]" +
                           "[" + LevelToText(level) + "] " + message;

  std::cout << line << std::endl;

  if (file_stream_.is_open()) {
    file_stream_ << line << std::endl;
    file_stream_.flush();
  }
}

}  // namespace ssext
