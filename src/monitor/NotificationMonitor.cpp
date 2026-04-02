#include "monitor/NotificationMonitor.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace ssext {

NotificationMonitor* NotificationMonitor::instance_ = nullptr;

namespace {

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

}  // namespace

NotificationMonitor::NotificationMonitor()
    : running_(false), thread_id_(0), hook_(nullptr) {}

NotificationMonitor::~NotificationMonitor() {
  Stop();
}

bool NotificationMonitor::Start() {
  if (running_.load()) {
    return true;
  }

  running_.store(true);
  thread_ = std::thread(&NotificationMonitor::RunLoop, this);
  return true;
}

void NotificationMonitor::Stop() {
  if (!running_.load()) {
    return;
  }

  running_.store(false);
  if (thread_id_ != 0) {
    PostThreadMessage(thread_id_, WM_QUIT, 0, 0);
  }

  if (thread_.joinable()) {
    thread_.join();
  }
}

NotificationItem NotificationMonitor::PopLatest() {
  std::lock_guard<std::mutex> lock(mutex_);
  NotificationItem out = latest_;
  latest_ = {};
  return out;
}

void CALLBACK NotificationMonitor::WinEventCallback(HWINEVENTHOOK, DWORD, HWND hwnd,
                                                    LONG, LONG, DWORD, DWORD) {
  if (instance_ == nullptr || hwnd == nullptr) {
    return;
  }
  instance_->ProcessWindow(hwnd);
}

void NotificationMonitor::RunLoop() {
  instance_ = this;
  thread_id_ = GetCurrentThreadId();

  hook_ = SetWinEventHook(EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE, nullptr,
                          NotificationMonitor::WinEventCallback, 0, 0,
                          WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

  MSG msg;
  while (running_.load()) {
    const BOOL ret = GetMessage(&msg, nullptr, 0, 0);
    if (ret <= 0) {
      break;
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  if (hook_ != nullptr) {
    UnhookWinEvent(hook_);
    hook_ = nullptr;
  }

  instance_ = nullptr;
  thread_id_ = 0;
}

bool NotificationMonitor::HasUnreadPattern(const std::string& title) const {
  const auto left = title.find('(');
  const auto right = title.find(')', left == std::string::npos ? 0 : left + 1);
  if (left == std::string::npos || right == std::string::npos || right <= left + 1) {
    return false;
  }

  for (std::size_t i = left + 1; i < right; ++i) {
    if (!std::isdigit(static_cast<unsigned char>(title[i]))) {
      return false;
    }
  }

  return true;
}

bool NotificationMonitor::IsTrackedApp(const std::string& title, std::string& app_name) const {
  static const std::array<std::pair<const char*, const char*>, 4> apps = {
      std::make_pair("whatsapp", "WhatsApp"),
      std::make_pair("discord", "Discord"),
      std::make_pair("instagram", "Instagram"),
      std::make_pair("insta", "Instagram")};

  const std::string lower = ToLower(title);
  for (const auto& [needle, pretty] : apps) {
    if (lower.find(needle) != std::string::npos) {
      app_name = pretty;
      return true;
    }
  }

  return false;
}

void NotificationMonitor::ProcessWindow(HWND hwnd) {
  char class_name[128] = {0};
  const int class_len = GetClassNameA(hwnd, class_name, sizeof(class_name));
  if (class_len > 0) {
    const std::string klass(class_name, class_len);
    if (klass == "Windows.UI.Core.CoreWindow") {
      return;
    }
  }

  char title[512] = {0};
  const int title_len = GetWindowTextA(hwnd, title, sizeof(title));
  if (title_len <= 0) {
    return;
  }

  const std::string text(title, title_len);
  if (!HasUnreadPattern(text)) {
    return;
  }

  std::string app_name;
  if (!IsTrackedApp(text, app_name)) {
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = last_app_emit_.find(app_name);
    if (it != last_app_emit_.end()) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
      if (elapsed < 10) {
        return;
      }
    }

    last_app_emit_[app_name] = now;
    latest_.has_value = true;
    latest_.app = app_name;
    latest_.message = "Yeni bildirim";
  }
}

}  // namespace ssext
