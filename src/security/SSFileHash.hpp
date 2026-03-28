#pragma once

#include <string>

namespace ssext {

class SSFileHash {
 public:
  static bool Sha256Hex(const std::string& file_path, std::string& out_hex);
  static bool ParseSha256Text(const std::string& text, std::string& out_hex);
};

}  // namespace ssext
