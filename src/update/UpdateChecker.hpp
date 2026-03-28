#pragma once

#include <string>

namespace ssext {

struct UpdateInfo {
  bool request_ok = false;
  bool has_update = false;
  std::string latest_version;
  std::string download_url;
  std::string asset_name;
  std::string checksum_url;
};

class UpdateChecker {
 public:
  UpdateInfo CheckLatest(const std::string& owner, const std::string& repo,
                         const std::string& current_version) const;
  bool DownloadToFile(const std::string& url, const std::string& file_path) const;
  bool DownloadToString(const std::string& url, std::string& out_text) const;

 private:
  static bool ParseTagName(const std::string& json, std::string& tag_name);
  static bool ParseAssetDownload(const std::string& json, std::string& download_url,
                                 std::string& asset_name);
  static bool IsVersionNewer(const std::string& latest, const std::string& current);
  static std::string NormalizeVersion(std::string version);
};

}  // namespace ssext
