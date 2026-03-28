#include "ui/TrayController.hpp"

#include <shellapi.h>

#include <cstring>
#include <string>

namespace ssext {

namespace {

constexpr UINT kTrayMessage = WM_APP + 7;
constexpr UINT kMenuExitId = 1001;
constexpr UINT kMenuSettingsId = 1002;
constexpr UINT kMenuLogsId = 1003;
constexpr UINT kMenuRestartId = 1004;
constexpr UINT kMenuAutoUpdateId = 1005;
constexpr UINT kMenuEmailId = 1006;
constexpr UINT kMenuAutoStartId = 1007;
constexpr UINT kMenuLangTrId = 1008;
constexpr UINT kMenuLangEnId = 1009;
constexpr UINT kMenuMarqueeFastId = 1010;
constexpr UINT kMenuMarqueeNormalId = 1011;
constexpr UINT kMenuMarqueeSlowId = 1012;
constexpr UINT kMenuMarqueeVerySlowId = 1013;
constexpr UINT kMenuGameModeId = 1014;
constexpr UINT kMenuMediaSpotifyId = 1015;
constexpr UINT kMenuMediaYoutubeMusicId = 1016;
constexpr UINT kMenuMediaAppleMusicId = 1017;
constexpr UINT kMenuMediaBrowserId = 1018;
constexpr UINT kMenuClock12hId = 1019;
constexpr UINT kMenuClock24hId = 1020;
constexpr UINT kShowSettingsUiMessage = WM_APP + 21;
constexpr const wchar_t* kWindowClass = L"SSExtTrayWindowClass";

TrayController* g_instance = nullptr;

}  // namespace

TrayController::TrayController() : running_(false), thread_id_(0), hwnd_(nullptr), nid_{} {}

TrayController::~TrayController() {
  Stop();
}

bool TrayController::Start(std::function<void()> on_exit_requested,
                           std::function<void()> on_restart_requested,
                           std::function<void(bool)> on_auto_update_toggled,
                           std::function<void(bool)> on_email_toggled,
                           std::function<void(bool)> on_auto_start_toggled,
                           std::function<void(bool)> on_game_mode_toggled,
                           std::function<void(const std::string&, bool)> on_media_source_toggled,
                           std::function<void(bool)> on_clock_24h_toggled,
                           std::function<void(const std::string&)> on_language_changed,
                           std::function<void(int)> on_marquee_speed_changed,
                           std::string settings_path,
                           std::string log_path,
                           bool auto_update_enabled,
                           bool email_enabled,
                           bool auto_start_enabled,
                           bool game_mode_enabled,
                           bool media_spotify_enabled,
                           bool media_youtube_music_enabled,
                           bool media_apple_music_enabled,
                           bool media_browser_enabled,
                           bool clock_24h_format,
                           std::string language,
                           int marquee_shift_ms) {
  if (running_.load()) {
    return true;
  }

  on_exit_requested_ = std::move(on_exit_requested);
  on_restart_requested_ = std::move(on_restart_requested);
  on_auto_update_toggled_ = std::move(on_auto_update_toggled);
  on_email_toggled_ = std::move(on_email_toggled);
  on_auto_start_toggled_ = std::move(on_auto_start_toggled);
  on_game_mode_toggled_ = std::move(on_game_mode_toggled);
  on_media_source_toggled_ = std::move(on_media_source_toggled);
  on_clock_24h_toggled_ = std::move(on_clock_24h_toggled);
  on_language_changed_ = std::move(on_language_changed);
  on_marquee_speed_changed_ = std::move(on_marquee_speed_changed);
  settings_path_ = std::move(settings_path);
  log_path_ = std::move(log_path);
  auto_update_enabled_ = auto_update_enabled;
  email_enabled_ = email_enabled;
  auto_start_enabled_ = auto_start_enabled;
  game_mode_enabled_ = game_mode_enabled;
  media_spotify_enabled_ = media_spotify_enabled;
  media_youtube_music_enabled_ = media_youtube_music_enabled;
  media_apple_music_enabled_ = media_apple_music_enabled;
  media_browser_enabled_ = media_browser_enabled;
  clock_24h_format_ = clock_24h_format;
  language_ = std::move(language);
  marquee_shift_ms_ = marquee_shift_ms;
  running_.store(true);
  thread_ = std::thread(&TrayController::RunLoop, this);
  return true;
}

void TrayController::Stop() {
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

bool TrayController::CreateWindowAndIcon() {
  HINSTANCE hinstance = GetModuleHandle(nullptr);

  WNDCLASSW wc{};
  wc.lpfnWndProc = TrayController::WindowProc;
  wc.hInstance = hinstance;
  wc.lpszClassName = kWindowClass;

  RegisterClassW(&wc);

  hwnd_ = CreateWindowExW(0, kWindowClass, L"Carex-Ext Tray", 0, 0, 0, 0, 0, nullptr, nullptr,
                          hinstance, nullptr);
  if (hwnd_ == nullptr) {
    return false;
  }

  std::memset(&nid_, 0, sizeof(nid_));
  nid_.cbSize = sizeof(nid_);
  nid_.hWnd = hwnd_;
  nid_.uID = 1;
  nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  nid_.uCallbackMessage = kTrayMessage;
  nid_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  wcsncpy_s(nid_.szTip, L"Carex-Ext", _TRUNCATE);

  return Shell_NotifyIconW(NIM_ADD, &nid_) == TRUE;
}

void TrayController::RemoveIcon() {
  if (hwnd_ != nullptr) {
    Shell_NotifyIconW(NIM_DELETE, &nid_);
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
}

void TrayController::ShowContextMenu() {
  HMENU menu = CreatePopupMenu();
  if (menu == nullptr) {
    return;
  }

  const bool is_tr = (language_ == "tr");

    const wchar_t* open_settings_label = is_tr ? L"Ayarlar" : L"Settings";
    const wchar_t* open_logs_label = is_tr ? L"Loglar\u0131 a\u00e7" : L"Open logs";
    const wchar_t* auto_update_label = is_tr ? L"Otomatik g\u00fcncelleme" : L"Auto update";
    const wchar_t* email_label = is_tr ? L"E-posta izleme" : L"Email monitor";
    const wchar_t* auto_start_label = is_tr ? L"Windows a\u00e7\u0131l\u0131\u015f\u0131nda ba\u015flat" : L"Start with Windows";
    const wchar_t* game_mode_label = is_tr ? L"Oyun modu" : L"Game mode";
    const wchar_t* media_sources_label = is_tr ? L"Medya kaynaklar\u0131" : L"Media sources";
    const wchar_t* media_spotify_label = L"Spotify";
    const wchar_t* media_ytmusic_label = L"YouTube Music";
    const wchar_t* media_apple_label = L"Apple Music";
    const wchar_t* media_browser_label = is_tr ? L"Taray\u0131c\u0131 medya" : L"Browser media";
    const wchar_t* lang_tr_label = is_tr ? L"Dil: T\u00fcrk\u00e7e" : L"Language: Turkish";
  const wchar_t* lang_en_label = is_tr ? L"Dil: English" : L"Language: English";
  const wchar_t* speed_fast_label =
      is_tr ? L"Kayma h\u0131z\u0131: \u00c7ok h\u0131zl\u0131 (80ms)" : L"Scroll speed: Very fast (80ms)";
  const wchar_t* speed_normal_label =
      is_tr ? L"Kayma h\u0131z\u0131: Normal (140ms)" : L"Scroll speed: Normal (140ms)";
  const wchar_t* speed_slow_label =
      is_tr ? L"Kayma h\u0131z\u0131: Yava\u015f (220ms)" : L"Scroll speed: Slow (220ms)";
  const wchar_t* speed_very_slow_label =
      is_tr ? L"Kayma h\u0131z\u0131: \u00c7ok yava\u015f (320ms)" : L"Scroll speed: Very slow (320ms)";
    const wchar_t* speed_menu_label = is_tr ? L"Spotify kayma h\u0131z\u0131" : L"Spotify scroll speed";
    const wchar_t* restart_label = is_tr ? L"Yeniden ba\u015flat" : L"Restart";
    const wchar_t* exit_label = is_tr ? L"\u00c7\u0131k\u0131\u015f" : L"Exit";

  AppendMenuW(menu, MF_STRING, kMenuSettingsId, open_settings_label);
  AppendMenuW(menu, MF_STRING, kMenuLogsId, open_logs_label);
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

  AppendMenuW(menu, MF_STRING | (auto_update_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuAutoUpdateId, auto_update_label);
  AppendMenuW(menu, MF_STRING | (email_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuEmailId, email_label);
  AppendMenuW(menu, MF_STRING | (auto_start_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuAutoStartId, auto_start_label);
  AppendMenuW(menu, MF_STRING | (game_mode_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuGameModeId, game_mode_label);

  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, media_sources_label);
  AppendMenuW(menu,
              MF_STRING | (media_spotify_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuMediaSpotifyId, media_spotify_label);
  AppendMenuW(menu,
              MF_STRING | (media_youtube_music_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuMediaYoutubeMusicId, media_ytmusic_label);
  AppendMenuW(menu,
              MF_STRING | (media_apple_music_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuMediaAppleMusicId, media_apple_label);
  AppendMenuW(menu,
              MF_STRING | (media_browser_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuMediaBrowserId, media_browser_label);

  HMENU speed_menu = CreatePopupMenu();
  if (speed_menu != nullptr) {
    AppendMenuW(speed_menu,
                MF_STRING | (marquee_shift_ms_ == 80 ? MF_CHECKED : MF_UNCHECKED),
                kMenuMarqueeFastId, speed_fast_label);
    AppendMenuW(speed_menu,
                MF_STRING | (marquee_shift_ms_ == 140 ? MF_CHECKED : MF_UNCHECKED),
                kMenuMarqueeNormalId, speed_normal_label);
    AppendMenuW(speed_menu,
                MF_STRING | (marquee_shift_ms_ == 220 ? MF_CHECKED : MF_UNCHECKED),
                kMenuMarqueeSlowId, speed_slow_label);
    AppendMenuW(speed_menu,
                MF_STRING | (marquee_shift_ms_ == 320 ? MF_CHECKED : MF_UNCHECKED),
                kMenuMarqueeVerySlowId, speed_very_slow_label);

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(speed_menu),
                speed_menu_label);
  }

  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | (language_ == "tr" ? MF_CHECKED : MF_UNCHECKED), kMenuLangTrId,
              lang_tr_label);
  AppendMenuW(menu, MF_STRING | (language_ == "en" ? MF_CHECKED : MF_UNCHECKED), kMenuLangEnId,
              lang_en_label);

  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuRestartId, restart_label);
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuExitId, exit_label);

  POINT pt;
  GetCursorPos(&pt);
  SetForegroundWindow(hwnd_);
  const UINT clicked = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd_, nullptr);
  DestroyMenu(menu);

  HandleMenuCommand(clicked);
}

void TrayController::ShowSettingsUi() {
  HMENU menu = CreatePopupMenu();
  if (menu == nullptr) {
    return;
  }

  const bool is_tr = (language_ == "tr");
  const wchar_t* auto_update_label = is_tr ? L"Otomatik g\u00fcncelleme" : L"Auto update";
  const wchar_t* email_label = is_tr ? L"E-posta izleme" : L"Email monitor";
  const wchar_t* auto_start_label = is_tr ? L"Windows a\u00e7\u0131l\u0131\u015f\u0131nda ba\u015flat" : L"Start with Windows";
  const wchar_t* game_mode_label = is_tr ? L"Oyun modu" : L"Game mode";
  const wchar_t* media_sources_label = is_tr ? L"Medya kaynaklar\u0131" : L"Media sources";
  const wchar_t* media_spotify_label = L"Spotify";
  const wchar_t* media_ytmusic_label = L"YouTube Music";
  const wchar_t* media_apple_label = L"Apple Music";
  const wchar_t* media_browser_label = is_tr ? L"Taray\u0131c\u0131 medya" : L"Browser media";
  const wchar_t* clock_label = is_tr ? L"Saat format\u0131" : L"Clock format";
  const wchar_t* clock_12h_label = is_tr ? L"12 saat (AM/PM)" : L"12 hour (AM/PM)";
  const wchar_t* clock_24h_label = is_tr ? L"24 saat" : L"24 hour";
  const wchar_t* lang_tr_label = is_tr ? L"Dil: T\u00fcrk\u00e7e" : L"Language: Turkish";
  const wchar_t* lang_en_label = is_tr ? L"Dil: English" : L"Language: English";

  AppendMenuW(menu, MF_STRING | (auto_update_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuAutoUpdateId, auto_update_label);
  AppendMenuW(menu, MF_STRING | (email_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuEmailId, email_label);
  AppendMenuW(menu, MF_STRING | (auto_start_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuAutoStartId, auto_start_label);
  AppendMenuW(menu, MF_STRING | (game_mode_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuGameModeId, game_mode_label);
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, media_sources_label);
  AppendMenuW(menu, MF_STRING | (media_spotify_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuMediaSpotifyId, media_spotify_label);
  AppendMenuW(menu, MF_STRING | (media_youtube_music_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuMediaYoutubeMusicId, media_ytmusic_label);
  AppendMenuW(menu, MF_STRING | (media_apple_music_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuMediaAppleMusicId, media_apple_label);
  AppendMenuW(menu, MF_STRING | (media_browser_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuMediaBrowserId, media_browser_label);
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, clock_label);
  AppendMenuW(menu, MF_STRING | (!clock_24h_format_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuClock12hId, clock_12h_label);
  AppendMenuW(menu, MF_STRING | (clock_24h_format_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuClock24hId, clock_24h_label);
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | (language_ == "tr" ? MF_CHECKED : MF_UNCHECKED), kMenuLangTrId,
              lang_tr_label);
  AppendMenuW(menu, MF_STRING | (language_ == "en" ? MF_CHECKED : MF_UNCHECKED), kMenuLangEnId,
              lang_en_label);

  POINT pt;
  GetCursorPos(&pt);
  SetForegroundWindow(hwnd_);
  const UINT clicked = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd_, nullptr);
  DestroyMenu(menu);

  HandleMenuCommand(clicked);
}

void TrayController::HandleMenuCommand(UINT clicked) {
  if (clicked == 0) {
    return;
  }

  if (clicked == kMenuSettingsId) {
    PostMessage(hwnd_, kShowSettingsUiMessage, 0, 0);
  } else if (clicked == kMenuAutoUpdateId) {
    auto_update_enabled_ = !auto_update_enabled_;
    if (on_auto_update_toggled_) {
      on_auto_update_toggled_(auto_update_enabled_);
    }
  } else if (clicked == kMenuEmailId) {
    email_enabled_ = !email_enabled_;
    if (on_email_toggled_) {
      on_email_toggled_(email_enabled_);
    }
  } else if (clicked == kMenuAutoStartId) {
    auto_start_enabled_ = !auto_start_enabled_;
    if (on_auto_start_toggled_) {
      on_auto_start_toggled_(auto_start_enabled_);
    }
  } else if (clicked == kMenuGameModeId) {
    game_mode_enabled_ = !game_mode_enabled_;
    if (on_game_mode_toggled_) {
      on_game_mode_toggled_(game_mode_enabled_);
    }
  } else if (clicked == kMenuMediaSpotifyId) {
    media_spotify_enabled_ = !media_spotify_enabled_;
    if (on_media_source_toggled_) {
      on_media_source_toggled_("spotify", media_spotify_enabled_);
    }
  } else if (clicked == kMenuMediaYoutubeMusicId) {
    media_youtube_music_enabled_ = !media_youtube_music_enabled_;
    if (on_media_source_toggled_) {
      on_media_source_toggled_("youtube_music", media_youtube_music_enabled_);
    }
  } else if (clicked == kMenuMediaAppleMusicId) {
    media_apple_music_enabled_ = !media_apple_music_enabled_;
    if (on_media_source_toggled_) {
      on_media_source_toggled_("apple_music", media_apple_music_enabled_);
    }
  } else if (clicked == kMenuMediaBrowserId) {
    media_browser_enabled_ = !media_browser_enabled_;
    if (on_media_source_toggled_) {
      on_media_source_toggled_("browser", media_browser_enabled_);
    }
  } else if (clicked == kMenuClock12hId) {
    clock_24h_format_ = false;
    if (on_clock_24h_toggled_) {
      on_clock_24h_toggled_(clock_24h_format_);
    }
  } else if (clicked == kMenuClock24hId) {
    clock_24h_format_ = true;
    if (on_clock_24h_toggled_) {
      on_clock_24h_toggled_(clock_24h_format_);
    }
  } else if (clicked == kMenuLangTrId) {
    language_ = "tr";
    if (on_language_changed_) {
      on_language_changed_(language_);
    }
  } else if (clicked == kMenuLangEnId) {
    language_ = "en";
    if (on_language_changed_) {
      on_language_changed_(language_);
    }
  } else if (clicked == kMenuMarqueeFastId) {
    marquee_shift_ms_ = 80;
    if (on_marquee_speed_changed_) {
      on_marquee_speed_changed_(marquee_shift_ms_);
    }
  } else if (clicked == kMenuMarqueeNormalId) {
    marquee_shift_ms_ = 140;
    if (on_marquee_speed_changed_) {
      on_marquee_speed_changed_(marquee_shift_ms_);
    }
  } else if (clicked == kMenuMarqueeSlowId) {
    marquee_shift_ms_ = 220;
    if (on_marquee_speed_changed_) {
      on_marquee_speed_changed_(marquee_shift_ms_);
    }
  } else if (clicked == kMenuMarqueeVerySlowId) {
    marquee_shift_ms_ = 320;
    if (on_marquee_speed_changed_) {
      on_marquee_speed_changed_(marquee_shift_ms_);
    }
  } else if (clicked == kMenuLogsId) {
    if (!log_path_.empty()) {
      ShellExecuteA(nullptr, "open", log_path_.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
  } else if (clicked == kMenuRestartId) {
    if (on_restart_requested_) {
      on_restart_requested_();
    }
  } else if (clicked == kMenuExitId) {
    if (on_exit_requested_) {
      on_exit_requested_();
    }
  }
}

LRESULT CALLBACK TrayController::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (g_instance == nullptr) {
    return DefWindowProc(hwnd, msg, wparam, lparam);
  }

  if (msg == kShowSettingsUiMessage) {
    g_instance->ShowSettingsUi();
    return 0;
  }

  if (msg == kTrayMessage) {
    if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU || lparam == WM_LBUTTONUP) {
      g_instance->ShowContextMenu();
      return 0;
    }
  }

  return DefWindowProc(hwnd, msg, wparam, lparam);
}

void TrayController::RunLoop() {
  g_instance = this;
  thread_id_ = GetCurrentThreadId();

  if (!CreateWindowAndIcon()) {
    running_.store(false);
    g_instance = nullptr;
    thread_id_ = 0;
    return;
  }

  MSG msg;
  while (running_.load()) {
    const BOOL ret = GetMessage(&msg, nullptr, 0, 0);
    if (ret <= 0) {
      break;
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  RemoveIcon();

  g_instance = nullptr;
  thread_id_ = 0;
}

}  // namespace ssext
