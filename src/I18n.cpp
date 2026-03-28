#include "I18n.hpp"

#include <algorithm>
#include <unordered_map>

namespace ssext {

namespace {

using Dict = std::unordered_map<std::string, std::string>;

const Dict kTr = {
    {"volume_title", "Ses Seviyesi"},
  {"update_title", "Güncelleme"},
  {"update_latest", "Güncel sürüm kullanıyorsun"},
  {"update_new", "Yeni sürüm: "},
  {"update_downloading", "İndiriliyor..."},
  {"update_installing", "Kurulum başlatılıyor..."},
  {"temp_prefix", "Sıcaklık: "},
  {"temp_na", "Sıcaklık: N/A"},
    {"notification_generic", "Yeni bildirim"},
};

const Dict kEn = {
    {"volume_title", "Volume"},
    {"update_title", "Update"},
    {"update_latest", "You are up to date"},
    {"update_new", "New version: "},
    {"update_downloading", "Downloading..."},
    {"update_installing", "Starting install..."},
    {"temp_prefix", "Temp: "},
    {"temp_na", "Temp: N/A"},
    {"notification_generic", "Notification"},
};

const std::string kEmpty;

}  // namespace

void I18n::SetLanguage(const std::string& language_code) {
  std::string lowered = language_code;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  if (lowered == "en" || lowered == "tr") {
    language_ = lowered;
  }
}

const std::string& I18n::GetLanguage() const {
  return language_;
}

std::string I18n::Text(const std::string& key) const {
  const Dict& dict = (language_ == "en") ? kEn : kTr;
  auto it = dict.find(key);
  if (it != dict.end()) {
    return it->second;
  }

  auto tr_it = kTr.find(key);
  if (tr_it != kTr.end()) {
    return tr_it->second;
  }

  return kEmpty;
}

}  // namespace ssext
