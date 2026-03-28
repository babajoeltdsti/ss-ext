#pragma once

#include <string>

namespace ssext {

class I18n {
 public:
  void SetLanguage(const std::string& language_code);
  const std::string& GetLanguage() const;
  std::string Text(const std::string& key) const;

 private:
  std::string language_ = "tr";
};

}  // namespace ssext
