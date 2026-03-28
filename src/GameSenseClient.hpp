#pragma once

#include <string>

#include "Config.hpp"

namespace ssext {

class GameSenseClient {
 public:
  explicit GameSenseClient(Config config);

  bool DiscoverServer();
  bool RegisterGame();
  bool SetupClockHandler();
  bool SetupSpotifyHandler();
  bool SetupVolumeHandler();
  bool SetupNotificationHandler();
  bool SetupGameModeHandler();
  bool SetupUpdateHandler();
  bool SetupEmailHandler();
  bool SendClockEvent(const std::string& time_text, const std::string& date_text);
  bool SendSpotifyEvent(const std::string& title, const std::string& artist,
                        const std::string& duration);
  bool SendVolumeEvent(const std::string& title, const std::string& level);
  bool SendNotificationEvent(const std::string& app, const std::string& message);
  bool SendGameModeEvent(const std::string& game, const std::string& info);
  bool SendUpdateEvent(const std::string& title, const std::string& status);
  bool SendEmailEvent(const std::string& subject, const std::string& sender);

 private:
  bool PostJson(const std::string& path, const std::string& body);
  static std::string EscapeJson(const std::string& text);
  static std::string ReadFile(const std::string& path);
  static bool ParseAddressFromCoreProps(const std::string& json, std::string& out_address);

  Config config_;
  std::string server_address_;
  std::string server_host_;
  unsigned short server_port_ = 62501;
};

}  // namespace ssext
