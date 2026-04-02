#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "Config.hpp"

namespace ssext {

struct EmailItem {
  bool has_value = false;
  std::string subject;
  std::string sender;
};

class EmailMonitor {
 public:
  explicit EmailMonitor(const Config& config);
  static bool TestSmtpConnection(const std::string& server,
                                 int port,
                                 bool use_ssl,
                                 std::string& error_message);
  void UpdateConfig(const Config& config);
  void SetCredential(std::string username, std::string password);

  EmailItem Poll();

 private:
  EmailItem PollImapPlain();
  EmailItem PollImapSsl();

  Config config_;
  std::chrono::steady_clock::time_point next_poll_{};
  bool test_emitted_ = false;
  std::string username_;
  std::string password_;
  std::uint64_t last_seen_uid_ = 0;
};

}  // namespace ssext
