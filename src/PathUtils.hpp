#pragma once

#include <string>

namespace ssext {

bool EnsureDirectory(const std::string& path);
std::string GetAppDataDirectory();
std::string GetAppDirectory();
std::string GetExecutablePath();
std::string JoinPath(const std::string& left, const std::string& right);

}  // namespace ssext
