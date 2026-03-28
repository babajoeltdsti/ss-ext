#pragma once

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace ssext {

struct NotificationItem {
  bool has_value = false;
  std::string app;
  std::string message;
};

class NotificationMonitor {
 public:
  NotificationMonitor();
  ~NotificationMonitor();

  bool Start();
  void Stop();

  NotificationItem PopLatest();

 private:
  static void CALLBACK WinEventCallback(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                                        LONG id_object, LONG id_child, DWORD event_thread,
                                        DWORD event_time);
  void RunLoop();
  void ProcessWindow(HWND hwnd);
  bool IsTrackedApp(const std::string& title, std::string& app_name) const;
  bool HasUnreadPattern(const std::string& title) const;

  static NotificationMonitor* instance_;

  std::atomic<bool> running_;
  std::thread thread_;
  DWORD thread_id_;
  HWINEVENTHOOK hook_;

  std::mutex mutex_;
  NotificationItem latest_;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_app_emit_;
};

}  // namespace ssext
