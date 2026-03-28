#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

namespace ssext {

struct GameState {
  bool active = false;
  bool changed = false;
  std::string process_name;
  std::string display_name;
};

class GameMonitor {
 public:
  GameState Poll();

 private:
  void EnsureKnownGamesLoaded();
  void RefreshKnownGamesFromRemote();
  bool DownloadToString(const std::string& url, std::string& out_text) const;
  void LoadFromCacheIfAvailable();
  void SaveCache(const std::string& text) const;
  void MergeKnownGamesFromText(const std::string& text);

  std::unordered_map<std::string, std::string> known_games_;
  std::chrono::steady_clock::time_point last_remote_refresh_{};
  bool initialized_ = false;
  std::string last_game_key_;
};

}  // namespace ssext
