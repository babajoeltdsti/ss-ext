#pragma once

#include <string>

namespace ssext {

class CredentialStore {
 public:
  bool SaveGeneric(const std::string& target, const std::string& username,
                   const std::string& secret) const;
  bool ReadGeneric(const std::string& target, std::string& username,
                   std::string& secret) const;
  bool DeleteGeneric(const std::string& target) const;
};

}  // namespace ssext
