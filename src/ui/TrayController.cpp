#include "ui/TrayController.hpp"

#include "PathUtils.hpp"

#include <shellapi.h>

#include <WebView2.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_map>

#include <wrl.h>

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
constexpr UINT kMenuLangDeId = 1030;
constexpr UINT kMenuLangFrId = 1031;
constexpr UINT kMenuLangEsId = 1032;
constexpr UINT kMenuMarqueeFastId = 1010;
constexpr UINT kMenuMarqueeNormalId = 1011;
constexpr UINT kMenuMarqueeSlowId = 1012;
constexpr UINT kMenuMarqueeVerySlowId = 1013;
constexpr UINT kMenuGameModeId = 1014;
constexpr UINT kMenuWpNotificationsId = 1021;
constexpr UINT kMenuNotificationWhatsAppId = 1022;
constexpr UINT kMenuNotificationDiscordId = 1023;
constexpr UINT kMenuNotificationInstagramId = 1024;
constexpr UINT kMenuMediaSpotifyId = 1015;
constexpr UINT kMenuMediaYoutubeMusicId = 1016;
constexpr UINT kMenuMediaAppleMusicId = 1017;
constexpr UINT kMenuMediaBrowserId = 1018;
constexpr UINT kMenuClock12hId = 1019;
constexpr UINT kMenuClock24hId = 1020;
constexpr UINT kMenuProfileBaseId = 3000;
constexpr UINT kMenuProfileMaxCount = 200;
constexpr UINT kShowSettingsUiMessage = WM_APP + 21;
constexpr UINT kWebViewStateSyncMessage = WM_APP + 22;
constexpr const wchar_t* kWindowClass = L"SSExtTrayWindowClass";
constexpr const wchar_t* kSettingsWindowClass = L"SSExtSettingsWindowClass";
constexpr UINT kSettingsCloseId = 2101;
constexpr UINT kSettingsOpenSettingsFileId = 2102;
constexpr UINT kSettingsProfileComboId = 2301;
constexpr UINT kSettingsProfileApplyId = 2302;
constexpr UINT kSettingsProfileNameEditId = 2303;
constexpr UINT kSettingsProfileCreateId = 2304;
constexpr UINT kSettingsProfileRenameId = 2305;
constexpr UINT kSettingsProfileDeleteId = 2306;
constexpr UINT kSettingsProfileCopyId = 2307;
constexpr UINT kSettingsProfileExportId = 2308;
constexpr UINT kSettingsProfileImportId = 2309;
constexpr UINT kSettingsProfileLockId = 2310;
constexpr UINT kSettingsProfilePathEditId = 2311;
constexpr UINT kSettingsEmailImapServerId = 2501;
constexpr UINT kSettingsEmailImapPortId = 2502;
constexpr UINT kSettingsEmailImapSslId = 2503;
constexpr UINT kSettingsEmailSmtpServerId = 2504;
constexpr UINT kSettingsEmailSmtpPortId = 2505;
constexpr UINT kSettingsEmailSmtpSslId = 2506;
constexpr UINT kSettingsEmailUsernameId = 2507;
constexpr UINT kSettingsEmailPollIntervalId = 2508;
constexpr UINT kSettingsEmailApplyId = 2509;
constexpr UINT kSettingsEmailTestSmtpId = 2510;
constexpr UINT kSettingsRestartId = 2401;
constexpr UINT kSettingsExitId = 2402;
constexpr UINT kSettingsTabProfileId = 2601;
constexpr UINT kSettingsTabGeneralId = 2602;
constexpr UINT kSettingsTabEmailId = 2603;
constexpr UINT kSettingsTabMediaId = 2604;
constexpr UINT kSettingsTabClockId = 2605;
constexpr UINT kSettingsTabAdvancedId = 2606;
constexpr UINT kSettingsInlineStatusId = 2610;
constexpr UINT kSettingsWebViewLoadingId = 2611;
constexpr UINT kSettingsGroupProfileId = 2701;
constexpr UINT kSettingsLabelActiveProfileId = 2702;
constexpr UINT kSettingsLabelProfileNameId = 2703;
constexpr UINT kSettingsLabelProfilePathId = 2704;
constexpr UINT kSettingsGroupGeneralId = 2710;
constexpr UINT kSettingsGroupEmailId = 2720;
constexpr UINT kSettingsLabelImapServerId = 2721;
constexpr UINT kSettingsLabelImapPortId = 2722;
constexpr UINT kSettingsLabelSmtpServerId = 2723;
constexpr UINT kSettingsLabelSmtpPortId = 2724;
constexpr UINT kSettingsLabelEmailUsernameId = 2725;
constexpr UINT kSettingsLabelEmailIntervalId = 2726;
constexpr UINT kSettingsGroupMediaId = 2730;
constexpr UINT kSettingsGroupClockId = 2740;
constexpr UINT kSettingsGroupLanguageId = 2741;
constexpr UINT kSettingsGroupClockLanguageId = kSettingsGroupClockId;
constexpr UINT kSettingsGroupMarqueeId = 2750;
constexpr int kSettingsWindowWidth = 760;
constexpr int kSettingsWindowHeight = 980;

constexpr COLORREF kSettingsBackgroundColor = RGB(244, 248, 252);
constexpr COLORREF kSettingsTextColor = RGB(52, 64, 84);
constexpr COLORREF kSettingsErrorTextColor = RGB(180, 33, 42);
constexpr COLORREF kSettingsSuccessTextColor = RGB(23, 101, 52);

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

ComPtr<ICoreWebView2Environment> g_webview_environment;
ComPtr<ICoreWebView2Controller> g_webview_controller;
ComPtr<ICoreWebView2> g_webview;

std::wstring Utf8ToWide(const std::string& text);

std::string UrlEncode(const std::string& text) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(text.size() * 3);
  for (unsigned char c : text) {
    const bool safe = (std::isalnum(c) != 0) || c == '-' || c == '_' || c == '.' || c == '~';
    if (safe) {
      out.push_back(static_cast<char>(c));
      continue;
    }

    out.push_back('%');
    out.push_back(kHex[(c >> 4) & 0x0F]);
    out.push_back(kHex[c & 0x0F]);
  }
  return out;
}

std::string UrlDecode(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '%' && i + 2 < text.size()) {
      const char hi = text[i + 1];
      const char lo = text[i + 2];
      auto hex = [](char v) -> int {
        if (v >= '0' && v <= '9') {
          return v - '0';
        }
        if (v >= 'a' && v <= 'f') {
          return 10 + (v - 'a');
        }
        if (v >= 'A' && v <= 'F') {
          return 10 + (v - 'A');
        }
        return -1;
      };

      const int h = hex(hi);
      const int l = hex(lo);
      if (h >= 0 && l >= 0) {
        out.push_back(static_cast<char>((h << 4) | l));
        i += 2;
        continue;
      }
    }

    if (c == '+') {
      out.push_back(' ');
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::unordered_map<std::string, std::string> ParseMessageFields(const std::string& message,
                                                                std::string& out_action) {
  std::unordered_map<std::string, std::string> fields;
  std::stringstream stream(message);
  std::string part;
  bool first = true;
  while (std::getline(stream, part, '|')) {
    if (first) {
      out_action = UrlDecode(part);
      first = false;
      continue;
    }

    const auto eq = part.find('=');
    if (eq == std::string::npos) {
      continue;
    }

    const std::string key = UrlDecode(part.substr(0, eq));
    const std::string value = UrlDecode(part.substr(eq + 1));
    fields[key] = value;
  }

  return fields;
}

std::wstring EscapeJsString(const std::string& value) {
  const std::wstring source = Utf8ToWide(value);
  std::wstring out;
  out.reserve(source.size() * 2);
  for (wchar_t ch : source) {
    if (ch == L'\\') {
      out += L"\\\\";
    } else if (ch == L'\'') {
      out += L"\\'";
    } else if (ch == L'\r') {
      out += L"\\r";
    } else if (ch == L'\n') {
      out += L"\\n";
    } else if (ch == 0x2028) {
      out += L"\\u2028";
    } else if (ch == 0x2029) {
      out += L"\\u2029";
    } else {
      out.push_back(ch);
    }
  }
  return out;
}

std::wstring ResolveWebViewUserDataFolder() {
  const std::string executable_path = GetExecutablePath();
  if (!executable_path.empty()) {
    const std::filesystem::path legacy_webview_dir =
        std::filesystem::path(executable_path).parent_path() /
        (std::filesystem::path(executable_path).filename().string() + ".WebView2");

    std::error_code exists_error;
    if (std::filesystem::exists(legacy_webview_dir, exists_error) && !exists_error) {
      const std::wstring wide_legacy_dir = Utf8ToWide(legacy_webview_dir.string());
      if (!wide_legacy_dir.empty()) {
        const DWORD legacy_attributes = GetFileAttributesW(wide_legacy_dir.c_str());
        if (legacy_attributes != INVALID_FILE_ATTRIBUTES) {
          SetFileAttributesW(wide_legacy_dir.c_str(),
                             legacy_attributes | FILE_ATTRIBUTE_HIDDEN |
                                 FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
        }
      }
    }
  }

  const std::string app_data_dir = GetAppDataDirectory();
  if (app_data_dir.empty()) {
    return {};
  }

  const std::filesystem::path webview_dir =
      std::filesystem::path(app_data_dir) / ".carex-ext-webview2";
  EnsureDirectory(webview_dir.string());

  const std::wstring wide_dir = Utf8ToWide(webview_dir.string());
  if (wide_dir.empty()) {
    return {};
  }

  const DWORD attributes = GetFileAttributesW(wide_dir.c_str());
  if (attributes != INVALID_FILE_ATTRIBUTES) {
    SetFileAttributesW(wide_dir.c_str(),
                       attributes | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
  }

  return wide_dir;
}

const wchar_t* kSettingsWebHtml = LR"HTML(
<!doctype html>
<html lang="tr">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>Carex-Ext Ayarlar</title>
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css" />
  <link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Manrope:wght@500;600;700;800&family=Rajdhani:wght@500;600;700&display=swap" />
  <style>
    :root {
      --bg-0: #051326;
      --bg-1: #0b1b33;
      --bg-2: #102844;
      --panel: rgba(17, 35, 62, 0.86);
      --panel-soft: rgba(28, 49, 79, 0.82);
      --line: rgba(124, 157, 208, 0.42);
      --line-strong: rgba(255, 111, 75, 0.95);
      --text: #eaf2ff;
      --muted: #a8bddb;
      --accent: #ff6f4b;
      --accent-soft: rgba(255, 111, 75, 0.2);
      --danger: #ff7f95;
      --ok: #58d9b0;
      --glow: 0 0 0 1px rgba(118, 155, 209, 0.24), 0 12px 34px rgba(5, 11, 25, 0.46);
    }
    * { box-sizing: border-box; }
    html, body {
      margin: 0;
      padding: 0;
      width: 100%;
      height: 100%;
      background:
        radial-gradient(120% 100% at 86% -8%, rgba(255, 111, 75, 0.18) 0%, rgba(255, 111, 75, 0) 56%),
        radial-gradient(95% 76% at -12% 100%, rgba(56, 102, 180, 0.35) 0%, rgba(56, 102, 180, 0) 52%),
        linear-gradient(152deg, var(--bg-0) 0%, var(--bg-1) 42%, var(--bg-2) 100%);
      color: var(--text);
    }
    body { font-family: "Manrope", "Segoe UI", sans-serif; letter-spacing: 0.05px; overflow: hidden; }
    .root { display: grid; grid-template-columns: 164px 1fr; height: 100vh; min-width: 0; }
    .sidebar {
      border-right: 1px solid var(--line);
      padding: 12px 10px;
      display: grid;
      grid-template-rows: auto 1fr;
      gap: 10px;
      background: linear-gradient(180deg, rgba(8, 20, 38, 0.88) 0%, rgba(14, 33, 59, 0.82) 100%);
      backdrop-filter: blur(8px);
    }
    .tabs { display: grid; gap: 8px; align-content: start; }
    .tab-btn {
      border: 1px solid var(--line);
      background: linear-gradient(180deg, rgba(21, 40, 71, 0.9) 0%, rgba(12, 26, 48, 0.92) 100%);
      color: var(--text);
      padding: 8px 10px;
      border-radius: 8px;
      cursor: pointer;
      text-align: center;
      font-family: "Rajdhani", "Manrope", sans-serif;
      font-size: 22px;
      line-height: 1.05;
      white-space: normal;
      overflow-wrap: anywhere;
      letter-spacing: 0.2px;
      transition: 120ms ease;
    }
    .tab-btn.active {
      border-color: var(--line-strong);
      background: var(--accent-soft);
      box-shadow: 0 0 0 1px rgba(255, 111, 75, 0.52), 0 0 18px rgba(255, 111, 75, 0.22);
      transform: translateY(-1px);
    }
    .content-wrap {
      display: grid;
      grid-template-rows: auto 1fr auto;
      min-height: 0;
      min-width: 0;
      background: linear-gradient(180deg, rgba(7, 20, 37, 0.2) 0%, rgba(7, 20, 37, 0) 100%);
    }
    .header {
      padding: 20px 22px 8px 22px;
    }
    .title {
      margin: 0;
      font-family: "Rajdhani", "Manrope", sans-serif;
      font-weight: 700;
      letter-spacing: 0.3px;
      font-size: 56px;
      color: #eff6ff;
      text-shadow: 0 8px 24px rgba(0, 0, 0, 0.35);
    }
    .subtitle { margin-top: 2px; color: var(--muted); font-size: 26px; }
    .content {
      overflow: auto;
      overflow-x: hidden;
      padding: 8px 22px 14px 22px;
      min-height: 0;
    }
    .section {
      display: none;
      border: 1px solid var(--line);
      background: linear-gradient(180deg, var(--panel) 0%, var(--panel-soft) 100%);
      border-radius: 12px;
      padding: 14px;
      min-width: 0;
      overflow: hidden;
      box-shadow: var(--glow);
    }
    .section.active { display: block; }
    .section-title {
      font-family: "Rajdhani", "Manrope", sans-serif;
      font-size: 28px;
      margin-bottom: 10px;
      font-weight: 700;
      letter-spacing: 0.2px;
      color: #f2f7ff;
    }
    .grid-2 { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; }
    .field { display: grid; gap: 6px; margin-bottom: 10px; }
    .field label { font-size: 22px; }
    input[type="text"], input[type="password"], select {
      border: 1px solid var(--line);
      border-radius: 8px;
      padding: 8px 10px;
      font-size: 22px;
      height: 42px;
      width: 100%;
      color: var(--text);
      background: rgba(13, 31, 56, 0.86);
    }
    input[type="text"]:focus, input[type="password"]:focus, select:focus {
      border-color: var(--line-strong);
      outline: none;
      box-shadow: 0 0 0 3px rgba(255, 111, 75, 0.2);
    }
    .row { display: grid; gap: 10px; }
    .row-2 { grid-template-columns: repeat(2, minmax(0, 1fr)); }
    .row-3 { grid-template-columns: repeat(3, minmax(0, 1fr)); }
    .row-4 { grid-template-columns: repeat(4, minmax(0, 1fr)); }
    .btn {
      border: 1px solid var(--line);
      background: linear-gradient(180deg, rgba(25, 47, 80, 0.92) 0%, rgba(16, 31, 57, 0.92) 100%);
      border-radius: 8px;
      color: var(--text);
      padding: 8px 10px;
      min-width: 0;
      height: auto;
      min-height: 42px;
      font-size: 19px;
      font-family: "Manrope", "Segoe UI", sans-serif;
      font-weight: 700;
      letter-spacing: 0.1px;
      line-height: 1.15;
      white-space: normal;
      overflow-wrap: anywhere;
      cursor: pointer;
    }
    .btn:hover { border-color: var(--line-strong); box-shadow: 0 8px 18px rgba(6, 12, 24, 0.5); }
    .btn.primary {
      border-color: var(--line-strong);
      background: linear-gradient(90deg, #ff7a45 0%, #ff4b5f 100%);
      color: #fff9f6;
      box-shadow: 0 12px 28px rgba(255, 95, 70, 0.3);
    }
)HTML"
LR"HTML(
    .toggle { display: grid; grid-template-columns: auto minmax(0, 1fr); align-items: center; gap: 8px; margin-bottom: 8px; font-size: 25px; min-width: 0; }
    .toggle span { color: #dbe8ff; overflow-wrap: anywhere; }
    .toggle input { width: 18px; height: 18px; }
    .toggle input[type="checkbox"],
    .toggle input[type="radio"] { accent-color: var(--accent); }
    .cards { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; }
    .card {
      border: 1px solid var(--line);
      border-radius: 10px;
      padding: 10px;
      background: linear-gradient(180deg, rgba(34, 56, 88, 0.88) 0%, rgba(24, 43, 70, 0.9) 100%);
      box-shadow: inset 0 0 0 1px rgba(142, 175, 225, 0.22);
    }
    .card-title {
      font-family: "Rajdhani", "Manrope", sans-serif;
      font-size: 22px;
      margin-bottom: 8px;
      font-weight: 700;
      letter-spacing: 0.18px;
      color: #edf4ff;
    }
    .status {
      min-height: 30px;
      padding: 2px 22px 8px 22px;
      font-size: 22px;
    }
    .status.error { color: var(--danger); }
    .status.ok { color: var(--ok); }
    .footer {
      border-top: 1px solid var(--line);
      padding: 10px 22px;
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
      background: linear-gradient(180deg, rgba(8, 20, 38, 0.78) 0%, rgba(17, 36, 63, 0.84) 100%);
    }
    .muted { color: var(--muted); font-size: 18px; overflow-wrap: anywhere; }
    .disabled { opacity: 0.55; pointer-events: none; }

    @media (max-width: 900px) {
      .root { grid-template-columns: 130px 1fr; }
      .title { font-size: 42px; }
      .subtitle { font-size: 20px; }
      .section-title { font-size: 24px; }
      .field label, .toggle { font-size: 20px; }
      input[type="text"], input[type="password"], select, .btn { font-size: 18px; }
      .grid-2, .cards, .row-4, .row-3, .row-2 { grid-template-columns: 1fr; }
      .footer { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="root">
    <aside class="sidebar">
      <div class="tabs" id="tabs"></div>
    </aside>
    <main class="content-wrap">
      <header class="header">
        <h1 class="title" id="title">Carex-Ext Ayarlar</h1>
        <div class="subtitle" id="subtitle">De&#287;i&#351;iklikler an&#305;nda uygulan&#305;r.</div>
      </header>
      <div class="content" id="content"></div>
      <div>
        <div class="status" id="status"></div>
        <footer class="footer">
          <button class="btn" id="btnRestart"></button>
          <button class="btn" id="btnExit"></button>
        </footer>
      </div>
    </main>
  </div>
)HTML"
LR"HTML(
  <script>
    const state = {
      language: 'tr',
      tab: 'general',
      auto_update: false,
      email_enabled: false,
      auto_start: false,
      game_mode: false,
      wp_notifications: true,
      notif_whatsapp: true,
      notif_discord: true,
      notif_instagram: true,
      media_spotify: true,
      media_youtube_music: true,
      media_apple_music: true,
      media_browser: true,
      clock_24h: false,
      marquee: 140,
      profile_names: ['default'],
      active_profile: 'default',
      active_profile_locked: false,
      email: {
        imap_server: '',
        imap_port: 993,
        imap_ssl: true,
        smtp_server: '',
        smtp_port: 587,
        smtp_ssl: true,
        username: '',
        password: '',
        poll_interval_seconds: 10,
      },
      profile_path: '',
      app_version: '1.0',
      developer: 'OMERBABACO',
      status: '',
      status_error: false,
    };
)HTML"
LR"HTML(

    const i18n = {
      tr: {
        title: 'Carex-Ext Ayarlar',
        subtitle: 'De\u011fi\u015fiklikler an\u0131nda uygulan\u0131r.',
        tabs: { general: 'Genel', notifications: 'Bildirimler', profile: 'Profil', email: 'E-posta', media: 'Medya', clock: 'Saat ve Dil', about: 'Hakk\u0131nda' },
        restart: 'Yeniden ba\u015flat', exit: '\u00c7\u0131k\u0131\u015f',
        profile: {
          section: 'Profil', active: 'Aktif profil', name: 'Profil ad\u0131', path: 'Dosya yolu',
          apply: 'Profili uygula', create: 'Olu\u015ftur', copy: 'Kopyala', rename: 'Yeniden adland\u0131r', del: 'Sil',
          lock: 'Profili kilitle', exp: 'D\u0131\u015fa aktar', imp: '\u0130\u00e7e aktar'
        },
        general: {
          section: 'Genel', autoUpdate: 'Otomatik g\u00fcncelleme', email: 'E-posta izleme',
          autoStart: 'Windows a\u00e7\u0131l\u0131\u015f\u0131nda ba\u015flat', gameMode: 'Oyun modu'
        },
        notifications: {
          section: 'Bildirimler',
          master: 'Bildirim izleme',
          whatsapp: 'WhatsApp bildirimi',
          discord: 'Discord bildirimi',
          instagram: 'Instagram bildirimi',
          hint: 'Discord ve Instagram bildirimleri masa\u00fcst\u00fc uygulamas\u0131 veya taray\u0131c\u0131 sekme ba\u015fl\u0131\u011f\u0131ndan alg\u0131lan\u0131r.'
        },
        email: {
          section: 'E-posta Sunucu Ayarlar\u0131', imapServer: 'IMAP sunucu', imapPort: 'IMAP port', imapSsl: 'IMAP SSL',
          smtpServer: 'SMTP sunucu', smtpPort: 'SMTP port', smtpSsl: 'SMTP SSL', user: 'Kullan\u0131c\u0131 ad\u0131', pass: 'IMAP \u015fifre',
          passHint: '\u0130lk kurulumda \u015fifre girin. Bo\u015f b\u0131rak\u0131rsan\u0131z mevcut \u015fifre korunur.',
          interval: 'Kontrol aral\u0131\u011f\u0131 (sn)', apply: 'E-posta ayarlar\u0131n\u0131 uygula', test: 'SMTP testi'
        },
        media: {
          section: 'Medya Kaynaklar\u0131', spotify: 'Spotify', ytm: 'YouTube Music', apple: 'Apple Music',
          browser: 'Taray\u0131c\u0131 medya', marquee: 'Medya \u0130smi Kayd\u0131rma H\u0131z\u0131',
          fast: '\u00c7ok h\u0131zl\u0131 (80ms)', normal: 'Normal (140ms)', slow: 'Yava\u015f (220ms)', vslow: '\u00c7ok yava\u015f (320ms)'
        },
        clock: {
          section: 'Saat ve Dil', cardClock: 'Saat Ayarlar\u0131', cardLang: 'Dil Ayarlar\u0131',
          h12: '12 saat (AM/PM)', h24: '24 saat', tr: 'T\u00fcrk\u00e7e', en: 'English', de: 'Deutsch', fr: 'Fran\u00e7ais', es: 'Espa\u00f1ol'
        },
        about: {
          section: 'Hakk\u0131nda', maker: 'Yap\u0131mc\u0131', version: 'Mevcut s\u00fcr\u00fcm',
          support: 'Destek', supportHint: 'Buy me a coffee ba\u011flant\u0131s\u0131:',
          coffee: 'BuyMeACoffee a\u00e7', update: 'G\u00fcncelleme kontrol et'
        }
      },
)HTML"
LR"HTML(
      en: {
        title: 'Carex-Ext Settings', subtitle: 'Changes are applied instantly.',
        tabs: { general: 'General', notifications: 'Notifications', profile: 'Profile', email: 'Email', media: 'Media', clock: 'Clock & Language', about: 'About' },
        restart: 'Restart', exit: 'Exit',
        profile: {
          section: 'Profile', active: 'Active profile', name: 'Profile name', path: 'File path',
          apply: 'Apply profile', create: 'Create', copy: 'Copy', rename: 'Rename', del: 'Delete',
          lock: 'Lock profile', exp: 'Export', imp: 'Import'
        },
        general: {
          section: 'General', autoUpdate: 'Auto update', email: 'Email monitoring',
          autoStart: 'Start with Windows', gameMode: 'Game mode'
        },
        notifications: {
          section: 'Notifications',
          master: 'Enable notifications',
          whatsapp: 'WhatsApp notifications',
          discord: 'Discord notifications',
          instagram: 'Instagram notifications',
          hint: 'Discord and Instagram are detected from desktop app windows or browser tab titles.'
        },
        email: {
          section: 'Email Server Settings', imapServer: 'IMAP server', imapPort: 'IMAP port', imapSsl: 'IMAP SSL',
          smtpServer: 'SMTP server', smtpPort: 'SMTP port', smtpSsl: 'SMTP SSL', user: 'Username', pass: 'IMAP password',
          passHint: 'Enter password for first setup. Leave blank to keep the existing password.',
          interval: 'Poll interval (sec)', apply: 'Apply email settings', test: 'Test SMTP'
        },
        media: {
          section: 'Media Sources', spotify: 'Spotify', ytm: 'YouTube Music', apple: 'Apple Music',
          browser: 'Browser media', marquee: 'Media Name Scroll Speed',
          fast: 'Very fast (80ms)', normal: 'Normal (140ms)', slow: 'Slow (220ms)', vslow: 'Very slow (320ms)'
        },
        clock: {
          section: 'Clock & Language', cardClock: 'Clock Settings', cardLang: 'Language Settings',
          h12: '12 hour (AM/PM)', h24: '24 hour', tr: 'Turkish', en: 'English', de: 'German', fr: 'French', es: 'Spanish'
        },
        about: {
          section: 'About', maker: 'Maker', version: 'Current version',
          support: 'Support', supportHint: 'Buy me a coffee link:',
          coffee: 'Open BuyMeACoffee', update: 'Check for updates'
        }
      },
    )HTML"
    LR"HTML(
      de: {
        title: 'Carex-Ext Einstellungen', subtitle: 'Anderungen werden sofort ubernommen.',
        tabs: { general: 'Allgemein', notifications: 'Benachr.', profile: 'Profil', email: 'E-Mail', media: 'Medien', clock: 'Uhr & Sprache', about: 'Info' },
        restart: 'Neu starten', exit: 'Beenden',
        profile: {
          section: 'Profil', active: 'Aktives Profil', name: 'Profilname', path: 'Dateipfad',
          apply: 'Profil anwenden', create: 'Erstellen', copy: 'Kopieren', rename: 'Umbenennen', del: 'Loschen',
          lock: 'Profil sperren', exp: 'Exportieren', imp: 'Importieren'
        },
        general: {
          section: 'Allgemein', autoUpdate: 'Auto-Update', email: 'E-Mail Uberwachung',
          autoStart: 'Mit Windows starten', gameMode: 'Spielmodus'
        },
        notifications: {
          section: 'Benachrichtigungen',
          master: 'Benachrichtigungen aktivieren',
          whatsapp: 'WhatsApp Benachr.',
          discord: 'Discord Benachr.',
          instagram: 'Instagram Benachr.',
          hint: 'Discord und Instagram werden uber App-Fenster oder Browser-Titel erkannt.'
        },
        email: {
          section: 'E-Mail Server Einstellungen', imapServer: 'IMAP Server', imapPort: 'IMAP Port', imapSsl: 'IMAP SSL/TLS',
          smtpServer: 'SMTP Server', smtpPort: 'SMTP Port', smtpSsl: 'SMTP SSL/TLS', user: 'Benutzername', pass: 'IMAP Passwort',
          passHint: 'Beim ersten Setup Passwort eingeben. Leer lassen, um vorhandenes Passwort zu behalten.',
          interval: 'Prufintervall (s)', apply: 'E-Mail Einstellungen speichern', test: 'SMTP testen'
        },
        media: {
          section: 'Medienquellen', spotify: 'Spotify', ytm: 'YouTube Music', apple: 'Apple Music',
          browser: 'Browser Medien', marquee: 'Scrolltempo des Medientitels',
          fast: 'Sehr schnell (80ms)', normal: 'Normal (140ms)', slow: 'Langsam (220ms)', vslow: 'Sehr langsam (320ms)'
        },
        clock: {
          section: 'Uhr & Sprache', cardClock: 'Uhreinstellungen', cardLang: 'Spracheinstellungen',
          h12: '12 Stunden (AM/PM)', h24: '24 Stunden', tr: 'Turkisch', en: 'Englisch', de: 'Deutsch', fr: 'Franzosisch', es: 'Spanisch'
        },
        about: {
          section: 'Info', maker: 'Entwickler', version: 'Aktuelle Version',
          support: 'Support', supportHint: 'Buy me a coffee Link:',
          coffee: 'BuyMeACoffee offnen', update: 'Updates prufen'
        }
      },
    )HTML"
    LR"HTML(
      fr: {
        title: 'Parametres Carex-Ext', subtitle: 'Les changements sont appliques instantanement.',
        tabs: { general: 'General', notifications: 'Notif.', profile: 'Profil', email: 'Email', media: 'Media', clock: 'Heure & Langue', about: 'A propos' },
        restart: 'Redemarrer', exit: 'Quitter',
        profile: {
          section: 'Profil', active: 'Profil actif', name: 'Nom du profil', path: 'Chemin du fichier',
          apply: 'Appliquer le profil', create: 'Creer', copy: 'Copier', rename: 'Renommer', del: 'Supprimer',
          lock: 'Verrouiller le profil', exp: 'Exporter', imp: 'Importer'
        },
        general: {
          section: 'General', autoUpdate: 'Mise a jour auto', email: 'Surveillance email',
          autoStart: 'Demarrer avec Windows', gameMode: 'Mode jeu'
        },
        notifications: {
          section: 'Notifications',
          master: 'Activer les notifications',
          whatsapp: 'Notifications WhatsApp',
          discord: 'Notifications Discord',
          instagram: 'Notifications Instagram',
          hint: 'Discord et Instagram sont detectes via les fenetres app ou les titres d onglet.'
        },
        email: {
          section: 'Parametres Serveur Email', imapServer: 'Serveur IMAP', imapPort: 'Port IMAP', imapSsl: 'IMAP SSL/TLS',
          smtpServer: 'Serveur SMTP', smtpPort: 'Port SMTP', smtpSsl: 'SMTP SSL/TLS', user: 'Nom utilisateur', pass: 'Mot de passe IMAP',
          passHint: 'Saisissez le mot de passe au premier reglage. Laisser vide pour conserver l existant.',
          interval: 'Intervalle (sec)', apply: 'Appliquer les parametres email', test: 'Tester SMTP'
        },
        media: {
          section: 'Sources media', spotify: 'Spotify', ytm: 'YouTube Music', apple: 'Apple Music',
          browser: 'Media navigateur', marquee: 'Vitesse du defilement du titre media',
          fast: 'Tres rapide (80ms)', normal: 'Normal (140ms)', slow: 'Lent (220ms)', vslow: 'Tres lent (320ms)'
        },
        clock: {
          section: 'Heure & Langue', cardClock: 'Parametres heure', cardLang: 'Parametres langue',
          h12: '12 heures (AM/PM)', h24: '24 heures', tr: 'Turc', en: 'Anglais', de: 'Allemand', fr: 'Francais', es: 'Espagnol'
        },
        about: {
          section: 'A propos', maker: 'Createur', version: 'Version actuelle',
          support: 'Support', supportHint: 'Lien Buy me a coffee :',
          coffee: 'Ouvrir BuyMeACoffee', update: 'Verifier les mises a jour'
        }
      },
    )HTML"
    LR"HTML(
      es: {
        title: 'Ajustes de Carex-Ext', subtitle: 'Los cambios se aplican al instante.',
        tabs: { general: 'General', notifications: 'Avisos', profile: 'Perfil', email: 'Correo', media: 'Medios', clock: 'Reloj e Idioma', about: 'Acerca' },
        restart: 'Reiniciar', exit: 'Salir',
        profile: {
          section: 'Perfil', active: 'Perfil activo', name: 'Nombre del perfil', path: 'Ruta del archivo',
          apply: 'Aplicar perfil', create: 'Crear', copy: 'Copiar', rename: 'Renombrar', del: 'Eliminar',
          lock: 'Bloquear perfil', exp: 'Exportar', imp: 'Importar'
        },
        general: {
          section: 'General', autoUpdate: 'Actualizacion automatica', email: 'Monitoreo de correo',
          autoStart: 'Iniciar con Windows', gameMode: 'Modo juego'
        },
        notifications: {
          section: 'Notificaciones',
          master: 'Activar notificaciones',
          whatsapp: 'Notificaciones de WhatsApp',
          discord: 'Notificaciones de Discord',
          instagram: 'Notificaciones de Instagram',
          hint: 'Discord e Instagram se detectan por ventanas de app o titulos de pestanas del navegador.'
        },
        email: {
          section: 'Configuracion de Servidor de Correo', imapServer: 'Servidor IMAP', imapPort: 'Puerto IMAP', imapSsl: 'IMAP SSL/TLS',
          smtpServer: 'Servidor SMTP', smtpPort: 'Puerto SMTP', smtpSsl: 'SMTP SSL/TLS', user: 'Usuario', pass: 'Contrasena IMAP',
          passHint: 'Ingresa la contrasena en la primera configuracion. Dejalo vacio para conservar la existente.',
          interval: 'Intervalo (seg)', apply: 'Aplicar ajustes de correo', test: 'Probar SMTP'
        },
        media: {
          section: 'Fuentes de medios', spotify: 'Spotify', ytm: 'YouTube Music', apple: 'Apple Music',
          browser: 'Medios del navegador', marquee: 'Velocidad de desplazamiento del titulo',
          fast: 'Muy rapido (80ms)', normal: 'Normal (140ms)', slow: 'Lento (220ms)', vslow: 'Muy lento (320ms)'
        },
        clock: {
          section: 'Reloj e Idioma', cardClock: 'Ajustes de reloj', cardLang: 'Ajustes de idioma',
          h12: '12 horas (AM/PM)', h24: '24 horas', tr: 'Turco', en: 'Ingles', de: 'Aleman', fr: 'Frances', es: 'Espanol'
        },
        about: {
          section: 'Acerca de', maker: 'Creador', version: 'Version actual',
          support: 'Soporte', supportHint: 'Enlace Buy me a coffee:',
          coffee: 'Abrir BuyMeACoffee', update: 'Buscar actualizaciones'
        }
      }
    };
)HTML"
LR"HTML(

    function send(action, fields = {}) {
      const parts = [encodeURIComponent(action)];
      Object.keys(fields).forEach((k) => {
        parts.push(`${encodeURIComponent(k)}=${encodeURIComponent(String(fields[k]))}`);
      });
      if (window.chrome && window.chrome.webview) {
        window.chrome.webview.postMessage(parts.join('|'));
      }
    }

    function parseState(payload) {
      if (!payload) {
        return;
      }
      payload.split('|').forEach((part) => {
        const eq = part.indexOf('=');
        if (eq < 0) {
          return;
        }
        const key = decodeURIComponent(part.slice(0, eq));
        const value = decodeURIComponent(part.slice(eq + 1));
        switch (key) {
          case 'language': state.language = ['tr','en','de','fr','es'].includes(value) ? value : 'tr'; break;
          case 'tab': state.tab = value || 'general'; break;
          case 'auto_update': state.auto_update = value === '1'; break;
          case 'email_enabled': state.email_enabled = value === '1'; break;
          case 'auto_start': state.auto_start = value === '1'; break;
          case 'game_mode': state.game_mode = value === '1'; break;
          case 'wp_notifications': state.wp_notifications = value === '1'; break;
          case 'notif_whatsapp': state.notif_whatsapp = value === '1'; break;
          case 'notif_discord': state.notif_discord = value === '1'; break;
          case 'notif_instagram': state.notif_instagram = value === '1'; break;
          case 'media_spotify': state.media_spotify = value === '1'; break;
          case 'media_ytmusic': state.media_youtube_music = value === '1'; break;
          case 'media_apple': state.media_apple_music = value === '1'; break;
          case 'media_browser': state.media_browser = value === '1'; break;
          case 'clock_24h': state.clock_24h = value === '1'; break;
          case 'marquee': state.marquee = parseInt(value || '140', 10); break;
          case 'profiles': state.profile_names = value ? value.split(',').map((v) => decodeURIComponent(v)) : ['default']; break;
          case 'active_profile': state.active_profile = value || 'default'; break;
          case 'profile_locked': state.active_profile_locked = value === '1'; break;
          case 'profile_path': state.profile_path = value || ''; break;
          case 'imap_server': state.email.imap_server = value; break;
          case 'imap_port': state.email.imap_port = parseInt(value || '993', 10); break;
          case 'imap_ssl': state.email.imap_ssl = value === '1'; break;
          case 'smtp_server': state.email.smtp_server = value; break;
          case 'smtp_port': state.email.smtp_port = parseInt(value || '587', 10); break;
          case 'smtp_ssl': state.email.smtp_ssl = value === '1'; break;
          case 'email_user': state.email.username = value; break;
          case 'email_interval': state.email.poll_interval_seconds = parseInt(value || '10', 10); break;
          case 'app_version': state.app_version = value || '1.0'; break;
          case 'developer': state.developer = value || 'OMERBABACO'; break;
        }
      });
    }

    function renderTabs(dict) {
      const tabs = ['general', 'notifications', 'profile', 'email', 'media', 'clock', 'about'];
      const container = document.getElementById('tabs');
      container.innerHTML = '';
      tabs.forEach((tab) => {
        const btn = document.createElement('button');
        btn.className = `tab-btn ${state.tab === tab ? 'active' : ''}`;
        btn.textContent = dict.tabs[tab];
        btn.addEventListener('click', () => {
          state.tab = tab;
          send('tab', { name: tab });
          render();
          const content = document.getElementById('content');
          content.scrollTop = 0;
          window.scrollTo(0, 0);
        });
        container.appendChild(btn);
      });
    }

    function toggle(name, checked) { send('toggle', { name, value: checked ? 1 : 0 }); }

    function cardClockLanguage(dict) {
      return `
      <section class="section ${state.tab === 'clock' ? 'active' : ''}">
        <div class="section-title">${dict.clock.section}</div>
        <div class="cards">
          <div class="card">
            <div class="card-title">${dict.clock.cardClock}</div>
            <label class="toggle"><input type="radio" name="clockfmt" ${!state.clock_24h ? 'checked' : ''} onchange="send('clock',{value:12})"/> <span>${dict.clock.h12}</span></label>
            <label class="toggle"><input type="radio" name="clockfmt" ${state.clock_24h ? 'checked' : ''} onchange="send('clock',{value:24})"/> <span>${dict.clock.h24}</span></label>
          </div>
          <div class="card">
            <div class="card-title">${dict.clock.cardLang}</div>
            <label class="toggle"><input type="radio" name="lang" ${state.language === 'tr' ? 'checked' : ''} onchange="send('language',{value:'tr'})"/> <span>${dict.clock.tr}</span></label>
            <label class="toggle"><input type="radio" name="lang" ${state.language === 'en' ? 'checked' : ''} onchange="send('language',{value:'en'})"/> <span>${dict.clock.en}</span></label>
            <label class="toggle"><input type="radio" name="lang" ${state.language === 'de' ? 'checked' : ''} onchange="send('language',{value:'de'})"/> <span>${dict.clock.de}</span></label>
            <label class="toggle"><input type="radio" name="lang" ${state.language === 'fr' ? 'checked' : ''} onchange="send('language',{value:'fr'})"/> <span>${dict.clock.fr}</span></label>
            <label class="toggle"><input type="radio" name="lang" ${state.language === 'es' ? 'checked' : ''} onchange="send('language',{value:'es'})"/> <span>${dict.clock.es}</span></label>
          </div>
        </div>
      </section>`;
    }

    function render() {
      const dict = i18n[state.language] || i18n.tr;
      document.documentElement.lang = state.language;
      document.getElementById('title').textContent = dict.title;
      document.getElementById('subtitle').textContent = dict.subtitle;
      document.getElementById('btnRestart').textContent = dict.restart;
      document.getElementById('btnExit').textContent = dict.exit;
      renderTabs(dict);

      const profileOptions = state.profile_names.map((name) => `<option value="${name}" ${name === state.active_profile ? 'selected' : ''}>${name}</option>`).join('');
      const speedChecked = (v) => state.marquee === v ? 'checked' : '';

      document.getElementById('content').innerHTML = `
      <section class="section ${state.tab === 'profile' ? 'active' : ''}">
        <div class="section-title">${dict.profile.section}</div>
        <div class="field"><label>${dict.profile.active}</label><div class="row row-2"><select id="activeProfile">${profileOptions}</select><button class="btn primary" id="profileApply">${dict.profile.apply}</button></div></div>
        <div class="field"><label>${dict.profile.name}</label><input id="profileName" type="text" value="" /></div>
        <div class="row row-4" style="margin-bottom:10px;"><button class="btn" id="profileCreate">${dict.profile.create}</button><button class="btn" id="profileCopy">${dict.profile.copy}</button><button class="btn" id="profileRename">${dict.profile.rename}</button><button class="btn" id="profileDelete">${dict.profile.del}</button></div>
        <label class="toggle"><input id="profileLock" type="checkbox" ${state.active_profile_locked ? 'checked' : ''} /> <span>${dict.profile.lock}</span></label>
        <div class="field"><label>${dict.profile.path}</label><div class="row row-2"><input id="profilePath" type="text" value="${state.profile_path || ''}" /><div class="row"><button class="btn" id="profileExport">${dict.profile.exp}</button><button class="btn" id="profileImport">${dict.profile.imp}</button></div></div></div>
      </section>
)HTML"
LR"HTML(

      <section class="section ${state.tab === 'general' ? 'active' : ''}">
        <div class="section-title">${dict.general.section}</div>
        <label class="toggle"><input type="checkbox" id="autoUpdate" ${state.auto_update ? 'checked' : ''}/> <span>${dict.general.autoUpdate}</span></label>
        <label class="toggle"><input type="checkbox" id="emailEnabled" ${state.email_enabled ? 'checked' : ''}/> <span>${dict.general.email}</span></label>
        <label class="toggle"><input type="checkbox" id="autoStart" ${state.auto_start ? 'checked' : ''}/> <span>${dict.general.autoStart}</span></label>
        <label class="toggle"><input type="checkbox" id="gameMode" ${state.game_mode ? 'checked' : ''}/> <span>${dict.general.gameMode}</span></label>
      </section>

      <section class="section ${state.tab === 'notifications' ? 'active' : ''}">
        <div class="section-title">${dict.notifications.section}</div>
        <label class="toggle"><input type="checkbox" id="wpNotifications" ${state.wp_notifications ? 'checked' : ''}/> <span>${dict.notifications.master}</span></label>
        <label class="toggle"><input type="checkbox" id="notifWhatsapp" ${state.notif_whatsapp ? 'checked' : ''}/> <span>${dict.notifications.whatsapp}</span></label>
        <label class="toggle"><input type="checkbox" id="notifDiscord" ${state.notif_discord ? 'checked' : ''}/> <span>${dict.notifications.discord}</span></label>
        <label class="toggle"><input type="checkbox" id="notifInstagram" ${state.notif_instagram ? 'checked' : ''}/> <span>${dict.notifications.instagram}</span></label>
        <div class="muted">${dict.notifications.hint}</div>
      </section>

      <section class="section ${state.tab === 'email' ? 'active' : ''}">
        <div class="section-title">${dict.email.section}</div>
        <div class="grid-2">
          <div class="field"><label>${dict.email.imapServer}</label><input id="imapServer" type="text" value="${state.email.imap_server || ''}" /></div>
          <div class="field"><label>${dict.email.imapPort}</label><input id="imapPort" type="text" value="${state.email.imap_port}" /></div>
          <label class="toggle"><input id="imapSsl" type="checkbox" ${state.email.imap_ssl ? 'checked' : ''}/> <span>${dict.email.imapSsl}</span></label>
          <div></div>
          <div class="field"><label>${dict.email.smtpServer}</label><input id="smtpServer" type="text" value="${state.email.smtp_server || ''}" /></div>
          <div class="field"><label>${dict.email.smtpPort}</label><input id="smtpPort" type="text" value="${state.email.smtp_port}" /></div>
          <label class="toggle"><input id="smtpSsl" type="checkbox" ${state.email.smtp_ssl ? 'checked' : ''}/> <span>${dict.email.smtpSsl}</span></label>
          <div></div>
          <div class="field"><label>${dict.email.user}</label><input id="emailUser" type="text" value="${state.email.username || ''}" /></div>
          <div class="field"><label>${dict.email.pass}</label><input id="emailPassword" type="password" value="" autocomplete="new-password" /></div>
          <div class="field"><label>${dict.email.interval}</label><input id="emailInterval" type="text" value="${state.email.poll_interval_seconds}" /></div>
          <div class="muted">${dict.email.passHint}</div>
        </div>
        <div class="row row-2"><button class="btn primary" id="emailApply">${dict.email.apply}</button><button class="btn" id="emailTest">${dict.email.test}</button></div>
      </section>
)HTML"
LR"HTML(

      <section class="section ${state.tab === 'media' ? 'active' : ''}">
        <div class="section-title">${dict.media.section}</div>
        <div class="grid-2">
          <label class="toggle"><input type="checkbox" id="mediaSpotify" ${state.media_spotify ? 'checked' : ''}/> <span>${dict.media.spotify}</span></label>
          <label class="toggle"><input type="checkbox" id="mediaApple" ${state.media_apple_music ? 'checked' : ''}/> <span>${dict.media.apple}</span></label>
          <label class="toggle"><input type="checkbox" id="mediaYtm" ${state.media_youtube_music ? 'checked' : ''}/> <span>${dict.media.ytm}</span></label>
          <label class="toggle"><input type="checkbox" id="mediaBrowser" ${state.media_browser ? 'checked' : ''}/> <span>${dict.media.browser}</span></label>
        </div>
        <div class="field">
          <label>${dict.media.marquee}</label>
          <div class="row row-4">
            <label class="toggle"><input type="radio" name="marquee" ${speedChecked(80)} onchange="send('marquee',{value:80})"/> <span>${dict.media.fast}</span></label>
            <label class="toggle"><input type="radio" name="marquee" ${speedChecked(140)} onchange="send('marquee',{value:140})"/> <span>${dict.media.normal}</span></label>
            <label class="toggle"><input type="radio" name="marquee" ${speedChecked(220)} onchange="send('marquee',{value:220})"/> <span>${dict.media.slow}</span></label>
            <label class="toggle"><input type="radio" name="marquee" ${speedChecked(320)} onchange="send('marquee',{value:320})"/> <span>${dict.media.vslow}</span></label>
          </div>
        </div>
      </section>

      ${cardClockLanguage(dict)}

      <section class="section ${state.tab === 'about' ? 'active' : ''}">
        <div class="section-title">${dict.about.section}</div>
        <div class="cards" style="margin-bottom:10px;">
          <div class="card">
            <div class="card-title">${dict.about.maker}</div>
            <div class="muted">${state.developer || 'OMERBABACO'}</div>
          </div>
          <div class="card">
            <div class="card-title">${dict.about.version}</div>
            <div class="muted">v${state.app_version || '1.0'}</div>
          </div>
        </div>
        <div class="field">
          <label>${dict.about.support}</label>
          <div class="muted">${dict.about.supportHint} buymeacoffee.com/valentinoo</div>
        </div>
        <div class="row row-2">
          <button class="btn" id="aboutCoffee">${dict.about.coffee}</button>
          <button class="btn primary" id="aboutUpdate">${dict.about.update}</button>
        </div>
      </section>
      `;
    )HTML"
    LR"HTML(

      const wire = (id, event, fn) => { const el = document.getElementById(id); if (el) { el.addEventListener(event, fn); } };

      wire('profileApply', 'click', () => send('profile_apply', { name: document.getElementById('activeProfile').value }));
      wire('profileCreate', 'click', () => send('profile_create', { name: document.getElementById('profileName').value }));
      wire('profileCopy', 'click', () => send('profile_copy', { source: document.getElementById('activeProfile').value, target: document.getElementById('profileName').value }));
      wire('profileRename', 'click', () => send('profile_rename', { source: document.getElementById('activeProfile').value, target: document.getElementById('profileName').value }));
      wire('profileDelete', 'click', () => send('profile_delete', { name: document.getElementById('activeProfile').value }));
      wire('profileLock', 'change', (e) => send('profile_lock', { name: document.getElementById('activeProfile').value, value: e.target.checked ? 1 : 0 }));
      wire('profileExport', 'click', () => send('profile_export', { name: document.getElementById('activeProfile').value, path: document.getElementById('profilePath').value }));
      wire('profileImport', 'click', () => send('profile_import', { path: document.getElementById('profilePath').value, target: document.getElementById('profileName').value }));

      wire('autoUpdate', 'change', (e) => toggle('auto_update', e.target.checked));
      wire('emailEnabled', 'change', (e) => toggle('email_enabled', e.target.checked));
      wire('autoStart', 'change', (e) => toggle('auto_start', e.target.checked));
      wire('gameMode', 'change', (e) => toggle('game_mode', e.target.checked));
      wire('wpNotifications', 'change', (e) => toggle('wp_notifications', e.target.checked));
      wire('notifWhatsapp', 'change', (e) => toggle('notif_whatsapp', e.target.checked));
      wire('notifDiscord', 'change', (e) => toggle('notif_discord', e.target.checked));
      wire('notifInstagram', 'change', (e) => toggle('notif_instagram', e.target.checked));

      wire('emailApply', 'click', () => send('email_apply', {
        imap_server: document.getElementById('imapServer').value,
        imap_port: document.getElementById('imapPort').value,
        imap_ssl: document.getElementById('imapSsl').checked ? 1 : 0,
        smtp_server: document.getElementById('smtpServer').value,
        smtp_port: document.getElementById('smtpPort').value,
        smtp_ssl: document.getElementById('smtpSsl').checked ? 1 : 0,
        username: document.getElementById('emailUser').value,
        password: document.getElementById('emailPassword').value,
        interval: document.getElementById('emailInterval').value,
      }));
      wire('emailTest', 'click', () => send('email_test', {
        smtp_server: document.getElementById('smtpServer').value,
        smtp_port: document.getElementById('smtpPort').value,
        smtp_ssl: document.getElementById('smtpSsl').checked ? 1 : 0,
      }));

      wire('mediaSpotify', 'change', (e) => toggle('media_spotify', e.target.checked));
      wire('mediaYtm', 'change', (e) => toggle('media_ytmusic', e.target.checked));
      wire('mediaApple', 'change', (e) => toggle('media_apple', e.target.checked));
      wire('mediaBrowser', 'change', (e) => toggle('media_browser', e.target.checked));
      wire('aboutCoffee', 'click', () => send('open_link', { target: 'bmac' }));
      wire('aboutUpdate', 'click', () => send('check_update'));

      wire('btnRestart', 'click', () => send('restart'));
      wire('btnExit', 'click', () => send('exit'));

      const emailSection = document.querySelector('.section.active');
      const disableEmailInputs = !state.email_enabled;
      ['imapServer','imapPort','imapSsl','smtpServer','smtpPort','smtpSsl','emailUser','emailPassword','emailInterval','emailApply','emailTest']
        .forEach((id) => {
          const el = document.getElementById(id);
          if (el) el.disabled = disableEmailInputs;
        });

      const disableNotificationInputs = !state.wp_notifications;
      ['notifWhatsapp','notifDiscord','notifInstagram']
        .forEach((id) => {
          const el = document.getElementById(id);
          if (el) el.disabled = disableNotificationInputs;
        });

      const statusEl = document.getElementById('status');
      statusEl.textContent = state.status || '';
      statusEl.className = `status ${state.status ? (state.status_error ? 'error' : 'ok') : ''}`;
    }
)HTML"
LR"HTML(

    window.__nativeApplyState = (payload, statusText, isError) => {
      parseState(payload || '');
      state.status = statusText || '';
      state.status_error = !!isError;
      render();
    };

    render();
    send('ready');
  </script>
</body>
</html>
)HTML";

HICON CreateTrayIcon() {
  constexpr int kSize = 32;

  HDC screen_dc = GetDC(nullptr);
  if (screen_dc == nullptr) {
    return nullptr;
  }

  HDC color_dc = CreateCompatibleDC(screen_dc);
  HDC mask_dc = CreateCompatibleDC(screen_dc);
  HBITMAP color_bmp = CreateCompatibleBitmap(screen_dc, kSize, kSize);
  HBITMAP mask_bmp = CreateBitmap(kSize, kSize, 1, 1, nullptr);
  ReleaseDC(nullptr, screen_dc);

  if (color_dc == nullptr || mask_dc == nullptr || color_bmp == nullptr || mask_bmp == nullptr) {
    if (color_bmp != nullptr) {
      DeleteObject(color_bmp);
    }
    if (mask_bmp != nullptr) {
      DeleteObject(mask_bmp);
    }
    if (color_dc != nullptr) {
      DeleteDC(color_dc);
    }
    if (mask_dc != nullptr) {
      DeleteDC(mask_dc);
    }
    return nullptr;
  }

  HGDIOBJ old_color = SelectObject(color_dc, color_bmp);
  HGDIOBJ old_mask = SelectObject(mask_dc, mask_bmp);

  RECT rect{0, 0, kSize, kSize};
  HBRUSH bg_brush = CreateSolidBrush(RGB(51, 122, 183));
  FillRect(color_dc, &rect, bg_brush);
  DeleteObject(bg_brush);

  SetBkMode(color_dc, TRANSPARENT);
  SetTextColor(color_dc, RGB(255, 255, 255));
  HFONT font = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
  HGDIOBJ old_font = nullptr;
  if (font != nullptr) {
    old_font = SelectObject(color_dc, font);
  }

  DrawTextW(color_dc, L"CE", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  PatBlt(mask_dc, 0, 0, kSize, kSize, BLACKNESS);

  ICONINFO info{};
  info.fIcon = TRUE;
  info.hbmColor = color_bmp;
  info.hbmMask = mask_bmp;
  HICON icon = CreateIconIndirect(&info);

  if (old_font != nullptr) {
    SelectObject(color_dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
  SelectObject(color_dc, old_color);
  SelectObject(mask_dc, old_mask);
  DeleteObject(color_bmp);
  DeleteObject(mask_bmp);
  DeleteDC(color_dc);
  DeleteDC(mask_dc);

  return icon;
}

std::wstring Utf8ToWide(const std::string& text) {
  if (text.empty()) {
    return {};
  }

  const int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  if (len <= 0) {
    return {};
  }

  std::wstring out;
  out.resize(static_cast<std::size_t>(len - 1));
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), len);
  return out;
}

std::string WideToUtf8(const std::wstring& text) {
  if (text.empty()) {
    return {};
  }

  const int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (len <= 0) {
    return {};
  }

  std::string out;
  out.resize(static_cast<std::size_t>(len - 1));
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, out.data(), len, nullptr, nullptr);
  return out;
}

std::string TrimAscii(const std::string& text) {
  const std::string whitespace = " \t\r\n";
  const auto first = text.find_first_not_of(whitespace);
  if (first == std::string::npos) {
    return {};
  }

  const auto last = text.find_last_not_of(whitespace);
  return text.substr(first, last - first + 1);
}

bool TryParseBoundedInt(const std::string& text,
                       int min_value,
                       int max_value,
                       int& out_value) {
  try {
    const std::string trimmed = TrimAscii(text);
    if (trimmed.empty()) {
      return false;
    }

    std::size_t pos = 0;
    const int value = std::stoi(trimmed, &pos);
    if (pos != trimmed.size()) {
      return false;
    }

    if (value < min_value || value > max_value) {
      return false;
    }

    out_value = value;
    return true;
  } catch (...) {
    return false;
  }
}

void SetControlVisible(HWND parent, UINT control_id, bool visible) {
  HWND control = GetDlgItem(parent, static_cast<int>(control_id));
  if (control != nullptr) {
    ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
  }
}

TrayController* g_instance = nullptr;

}  // namespace

TrayController::TrayController()
    : running_(false),
      thread_id_(0),
      hwnd_(nullptr),
      settings_hwnd_(nullptr),
      tray_icon_(nullptr),
      settings_font_(nullptr),
      settings_title_font_(nullptr),
      settings_background_brush_(nullptr),
      nid_{} {}

TrayController::~TrayController() {
  Stop();
}

bool TrayController::Start(std::function<void()> on_exit_requested,
                           std::function<void()> on_restart_requested,
                           std::function<void(bool)> on_auto_update_toggled,
                           std::function<void(bool)> on_email_toggled,
                           std::function<void(bool)> on_auto_start_toggled,
                           std::function<void(bool)> on_game_mode_toggled,
                           std::function<void(bool)> on_wp_notifications_toggled,
                           std::function<void(const std::string&, bool)> on_notification_source_toggled,
                           std::function<void(const std::string&, bool)> on_media_source_toggled,
                           std::function<void(bool)> on_clock_24h_toggled,
                           std::function<void(const std::string&)> on_language_changed,
                           std::function<void(int)> on_marquee_speed_changed,
               std::function<TrayActionStatus()> on_manual_update_requested,
                           std::function<void(const TrayEmailSettings&)> on_email_settings_changed,
                           std::function<std::string(const std::string&, const std::string&)>
                               on_email_credential_saved,
                           std::function<std::string(const TrayEmailSettings&)> on_email_smtp_test,
               std::function<TrayProfileSelectionResult(const std::string&)>
                 on_profile_selected,
               std::function<TrayProfileSelectionResult(const std::string&)>
                 on_profile_created,
               std::function<TrayProfileSelectionResult(const std::string&,
                                  const std::string&)>
                 on_profile_copied,
               std::function<TrayProfileSelectionResult(const std::string&,
                                  const std::string&)>
                 on_profile_renamed,
               std::function<TrayProfileSelectionResult(const std::string&)>
                 on_profile_deleted,
               std::function<TrayProfileSelectionResult(const std::string&,
                                  const std::string&)>
                 on_profile_exported,
               std::function<TrayProfileSelectionResult(const std::string&,
                                  const std::string&)>
                 on_profile_imported,
               std::function<TrayProfileSelectionResult(const std::string&, bool)>
                 on_profile_lock_toggled,
                           std::string settings_path,
                           std::string log_path,
                           bool auto_update_enabled,
                           bool email_enabled,
                           bool auto_start_enabled,
                           bool game_mode_enabled,
                           bool wp_notifications_enabled,
                           bool notification_whatsapp_enabled,
                           bool notification_discord_enabled,
                           bool notification_instagram_enabled,
                           bool media_spotify_enabled,
                           bool media_youtube_music_enabled,
                           bool media_apple_music_enabled,
                           bool media_browser_enabled,
                           bool clock_24h_format,
                           std::string language,
                           std::string app_version,
                           std::string developer,
                           int marquee_shift_ms,
                           TrayEmailSettings email_settings,
                           std::vector<std::string> profile_names,
                           std::string active_profile_name,
                           bool active_profile_locked) {
  if (running_.load()) {
    return true;
  }

  on_exit_requested_ = std::move(on_exit_requested);
  on_restart_requested_ = std::move(on_restart_requested);
  on_auto_update_toggled_ = std::move(on_auto_update_toggled);
  on_email_toggled_ = std::move(on_email_toggled);
  on_auto_start_toggled_ = std::move(on_auto_start_toggled);
  on_game_mode_toggled_ = std::move(on_game_mode_toggled);
  on_wp_notifications_toggled_ = std::move(on_wp_notifications_toggled);
  on_notification_source_toggled_ = std::move(on_notification_source_toggled);
  on_media_source_toggled_ = std::move(on_media_source_toggled);
  on_clock_24h_toggled_ = std::move(on_clock_24h_toggled);
  on_language_changed_ = std::move(on_language_changed);
  on_marquee_speed_changed_ = std::move(on_marquee_speed_changed);
  on_manual_update_requested_ = std::move(on_manual_update_requested);
  on_email_settings_changed_ = std::move(on_email_settings_changed);
  on_email_credential_saved_ = std::move(on_email_credential_saved);
  on_email_smtp_test_ = std::move(on_email_smtp_test);
  on_profile_selected_ = std::move(on_profile_selected);
  on_profile_created_ = std::move(on_profile_created);
  on_profile_copied_ = std::move(on_profile_copied);
  on_profile_renamed_ = std::move(on_profile_renamed);
  on_profile_deleted_ = std::move(on_profile_deleted);
  on_profile_exported_ = std::move(on_profile_exported);
  on_profile_imported_ = std::move(on_profile_imported);
  on_profile_lock_toggled_ = std::move(on_profile_lock_toggled);
  settings_path_ = std::move(settings_path);
  log_path_ = std::move(log_path);
  auto_update_enabled_ = auto_update_enabled;
  email_enabled_ = email_enabled;
  auto_start_enabled_ = auto_start_enabled;
  game_mode_enabled_ = game_mode_enabled;
  wp_notifications_enabled_ = wp_notifications_enabled;
  notification_whatsapp_enabled_ = notification_whatsapp_enabled;
  notification_discord_enabled_ = notification_discord_enabled;
  notification_instagram_enabled_ = notification_instagram_enabled;
  media_spotify_enabled_ = media_spotify_enabled;
  media_youtube_music_enabled_ = media_youtube_music_enabled;
  media_apple_music_enabled_ = media_apple_music_enabled;
  media_browser_enabled_ = media_browser_enabled;
  clock_24h_format_ = clock_24h_format;
  language_ = std::move(language);
  app_version_ = std::move(app_version);
  developer_ = std::move(developer);
  marquee_shift_ms_ = marquee_shift_ms;
  email_settings_ = std::move(email_settings);
  profile_names_ = std::move(profile_names);
  active_profile_name_ = std::move(active_profile_name);
  active_profile_locked_ = active_profile_locked;
  active_tab_index_ = 1;
  status_message_.clear();
  status_message_is_error_ = false;
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

  if (tray_icon_ == nullptr) {
    tray_icon_ = CreateTrayIcon();
  }

  HICON class_icon = tray_icon_;
  if (class_icon == nullptr) {
    class_icon = LoadIcon(nullptr, IDI_APPLICATION);
  }

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = TrayController::WindowProc;
  wc.hInstance = hinstance;
  wc.lpszClassName = kWindowClass;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hIcon = class_icon;
  wc.hIconSm = class_icon;
  RegisterClassExW(&wc);

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
  nid_.hIcon = class_icon;
  wcsncpy_s(nid_.szTip, L"Carex-Ext", _TRUNCATE);

  return Shell_NotifyIconW(NIM_ADD, &nid_) == TRUE;
}

void TrayController::RemoveIcon() {
  g_webview = nullptr;
  g_webview_controller = nullptr;
  g_webview_environment = nullptr;
  settings_webview_ready_ = false;

  if (settings_hwnd_ != nullptr) {
    DestroyWindow(settings_hwnd_);
    settings_hwnd_ = nullptr;
  }

  if (settings_font_ != nullptr) {
    DeleteObject(settings_font_);
    settings_font_ = nullptr;
  }

  if (settings_title_font_ != nullptr) {
    DeleteObject(settings_title_font_);
    settings_title_font_ = nullptr;
  }

  if (settings_background_brush_ != nullptr) {
    DeleteObject(settings_background_brush_);
    settings_background_brush_ = nullptr;
  }

  if (hwnd_ != nullptr) {
    Shell_NotifyIconW(NIM_DELETE, &nid_);
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }

  if (tray_icon_ != nullptr) {
    DestroyIcon(tray_icon_);
    tray_icon_ = nullptr;
  }
}

void TrayController::ShowContextMenu() {
  HMENU menu = CreatePopupMenu();
  if (menu == nullptr) {
    return;
  }

  const bool is_tr = (language_ == "tr");

    const wchar_t* profiles_label = is_tr ? L"Profiller" : L"Profiles";
    const wchar_t* open_settings_label = is_tr ? L"Ayarlar" : L"Settings";
    const wchar_t* open_logs_label = is_tr ? L"Loglar\u0131 a\u00e7" : L"Open logs";
    const wchar_t* auto_update_label = is_tr ? L"Otomatik g\u00fcncelleme" : L"Auto update";
    const wchar_t* email_label = is_tr ? L"E-posta izleme" : L"Email monitoring";
    const wchar_t* auto_start_label =
        is_tr ? L"Windows a\u00e7\u0131l\u0131\u015f\u0131nda ba\u015flat" : L"Start with Windows";
    const wchar_t* game_mode_label = is_tr ? L"Oyun modu" : L"Game mode";
    const wchar_t* wp_notifications_label =
      is_tr ? L"Bildirim izleme" : L"Notifications";
    const wchar_t* notification_sources_label =
      is_tr ? L"Bildirim kaynaklari" : L"Notification sources";
    const wchar_t* notif_whatsapp_label = L"WhatsApp";
    const wchar_t* notif_discord_label = L"Discord";
    const wchar_t* notif_instagram_label = L"Instagram";
    const wchar_t* media_sources_label = is_tr ? L"Medya kaynaklar\u0131" : L"Media sources";
    const wchar_t* media_spotify_label = L"Spotify";
    const wchar_t* media_ytmusic_label = L"YouTube Music";
    const wchar_t* media_apple_label = L"Apple Music";
    const wchar_t* media_browser_label = is_tr ? L"Taray\u0131c\u0131 medya" : L"Browser media";
    const wchar_t* lang_tr_label = is_tr ? L"Dil: T\u00fcrk\u00e7e" : L"Language: Turkish";
  const wchar_t* lang_en_label = is_tr ? L"Dil: English" : L"Language: English";
  const wchar_t* lang_de_label = is_tr ? L"Dil: Deutsch" : L"Language: German";
  const wchar_t* lang_fr_label = is_tr ? L"Dil: Francais" : L"Language: French";
  const wchar_t* lang_es_label = is_tr ? L"Dil: Espanol" : L"Language: Spanish";
  const wchar_t* speed_fast_label =
      is_tr ? L"Kayma h\u0131z\u0131: \u00c7ok h\u0131zl\u0131 (80ms)" : L"Scroll speed: Very fast (80ms)";
  const wchar_t* speed_normal_label =
      is_tr ? L"Kayma h\u0131z\u0131: Normal (140ms)" : L"Scroll speed: Normal (140ms)";
  const wchar_t* speed_slow_label =
      is_tr ? L"Kayma h\u0131z\u0131: Yava\u015f (220ms)" : L"Scroll speed: Slow (220ms)";
  const wchar_t* speed_very_slow_label =
      is_tr ? L"Kayma h\u0131z\u0131: \u00c7ok yava\u015f (320ms)" : L"Scroll speed: Very slow (320ms)";
    const wchar_t* speed_menu_label =
      is_tr ? L"Medya ismi kaydirma hizi" : L"Media title scroll speed";
    const wchar_t* restart_label = is_tr ? L"Yeniden ba\u015flat" : L"Restart";
    const wchar_t* exit_label = is_tr ? L"\u00c7\u0131k\u0131\u015f" : L"Exit";

  if (!profile_names_.empty()) {
    HMENU profile_menu = CreatePopupMenu();
    if (profile_menu != nullptr) {
      const std::size_t max_profiles = (std::min)(profile_names_.size(),
                                                  static_cast<std::size_t>(kMenuProfileMaxCount));
      for (std::size_t i = 0; i < max_profiles; ++i) {
        std::wstring profile_label = Utf8ToWide(profile_names_[i]);
        if (profile_label.empty()) {
          profile_label = L"default";
        }

        AppendMenuW(profile_menu,
                    MF_STRING |
                        (profile_names_[i] == active_profile_name_ ? MF_CHECKED : MF_UNCHECKED),
                    kMenuProfileBaseId + static_cast<UINT>(i), profile_label.c_str());
      }

      AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(profile_menu), profiles_label);
      AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
  }

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
  AppendMenuW(menu, MF_STRING | (wp_notifications_enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kMenuWpNotificationsId, wp_notifications_label);

  HMENU notification_menu = CreatePopupMenu();
  if (notification_menu != nullptr) {
    AppendMenuW(notification_menu,
                MF_STRING | (notification_whatsapp_enabled_ ? MF_CHECKED : MF_UNCHECKED),
                kMenuNotificationWhatsAppId, notif_whatsapp_label);
    AppendMenuW(notification_menu,
                MF_STRING | (notification_discord_enabled_ ? MF_CHECKED : MF_UNCHECKED),
                kMenuNotificationDiscordId, notif_discord_label);
    AppendMenuW(notification_menu,
                MF_STRING | (notification_instagram_enabled_ ? MF_CHECKED : MF_UNCHECKED),
                kMenuNotificationInstagramId, notif_instagram_label);

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(notification_menu),
                notification_sources_label);
  }

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
  AppendMenuW(menu, MF_STRING | (language_ == "de" ? MF_CHECKED : MF_UNCHECKED), kMenuLangDeId,
              lang_de_label);
  AppendMenuW(menu, MF_STRING | (language_ == "fr" ? MF_CHECKED : MF_UNCHECKED), kMenuLangFrId,
              lang_fr_label);
  AppendMenuW(menu, MF_STRING | (language_ == "es" ? MF_CHECKED : MF_UNCHECKED), kMenuLangEsId,
              lang_es_label);

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
  SyncSettingsWindowState();
}

bool TrayController::InitializeSettingsWebView() {
  if (settings_hwnd_ == nullptr || !IsWindow(settings_hwnd_)) {
    return false;
  }

  if (g_webview != nullptr && g_webview_controller != nullptr) {
    RECT rect{};
    GetClientRect(settings_hwnd_, &rect);
    g_webview_controller->put_Bounds(rect);
    g_webview_controller->put_IsVisible(TRUE);
    HWND loading = GetDlgItem(settings_hwnd_, static_cast<int>(kSettingsWebViewLoadingId));
    if (loading != nullptr) {
      ShowWindow(loading, SW_HIDE);
    }
    settings_webview_ready_ = true;
    return true;
  }

  settings_webview_ready_ = false;
  const std::wstring webview_user_data = ResolveWebViewUserDataFolder();
  const wchar_t* user_data_folder =
      webview_user_data.empty() ? nullptr : webview_user_data.c_str();
  const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
      nullptr, user_data_folder, nullptr,
      Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
            if (FAILED(result) || environment == nullptr || settings_hwnd_ == nullptr ||
                !IsWindow(settings_hwnd_)) {
              if (settings_hwnd_ != nullptr && IsWindow(settings_hwnd_)) {
                HWND loading =
                    GetDlgItem(settings_hwnd_, static_cast<int>(kSettingsWebViewLoadingId));
                if (loading != nullptr) {
                  SetWindowTextW(loading,
                                 L"WebView aray\u00fcz\u00fc ba\u015flat\u0131lamad\u0131.");
                }
              }
              return S_OK;
            }

            g_webview_environment = environment;
            g_webview_environment->CreateCoreWebView2Controller(
                settings_hwnd_,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [this](HRESULT controller_result,
                           ICoreWebView2Controller* controller) -> HRESULT {
                      if (FAILED(controller_result) || controller == nullptr ||
                          settings_hwnd_ == nullptr || !IsWindow(settings_hwnd_)) {
                        return S_OK;
                      }

                      g_webview_controller = controller;
                      g_webview_controller->get_CoreWebView2(&g_webview);

                      RECT rect{};
                      GetClientRect(settings_hwnd_, &rect);
                      g_webview_controller->put_Bounds(rect);
                      g_webview_controller->put_IsVisible(FALSE);

                      if (g_webview != nullptr) {
                        EventRegistrationToken token{};
                        g_webview->add_WebMessageReceived(
                            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                [this](ICoreWebView2*,
                                       ICoreWebView2WebMessageReceivedEventArgs* args)
                                    -> HRESULT {
                                  if (args == nullptr) {
                                    return S_OK;
                                  }

                                  LPWSTR raw_message = nullptr;
                                  if (SUCCEEDED(args->TryGetWebMessageAsString(&raw_message)) &&
                                      raw_message != nullptr) {
                                    const std::string message =
                                        WideToUtf8(std::wstring(raw_message));
                                    CoTaskMemFree(raw_message);
                                    HandleWebViewMessage(message);
                                  }

                                  return S_OK;
                                })
                                .Get(),
                            &token);

                        EventRegistrationToken navigation_token{};
                        g_webview->add_NavigationCompleted(
                            Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                [this](ICoreWebView2*,
                                       ICoreWebView2NavigationCompletedEventArgs* args)
                                    -> HRESULT {
                                  if (settings_hwnd_ == nullptr || !IsWindow(settings_hwnd_)) {
                                    return S_OK;
                                  }

                                  BOOL succeeded = FALSE;
                                  if (args != nullptr) {
                                    args->get_IsSuccess(&succeeded);
                                  }

                                  HWND loading =
                                      GetDlgItem(settings_hwnd_, static_cast<int>(kSettingsWebViewLoadingId));

                                  if (!succeeded) {
                                    if (loading != nullptr) {
                                      SetWindowTextW(loading,
                                                     L"Aray\u00fcz i\u00e7eri\u011fi y\u00fcklenemedi.");
                                    }
                                    return S_OK;
                                  }

                                  if (g_webview_controller != nullptr) {
                                    RECT rect{};
                                    GetClientRect(settings_hwnd_, &rect);
                                    g_webview_controller->put_Bounds(rect);
                                    g_webview_controller->put_IsVisible(TRUE);
                                  }

                                  if (loading != nullptr) {
                                    ShowWindow(loading, SW_HIDE);
                                  }

                                  settings_webview_ready_ = true;
                                  PostMessage(settings_hwnd_, kWebViewStateSyncMessage, 0, 0);
                                  return S_OK;
                                })
                                .Get(),
                            &navigation_token);

                        g_webview->NavigateToString(kSettingsWebHtml);
                      }

                      return S_OK;
                    })
                    .Get());

            return S_OK;
          })
          .Get());

  return SUCCEEDED(hr);
}

std::string TrayController::BuildWebViewStatePayload() const {
  std::string payload;
  auto append = [&](const std::string& key, const std::string& value) {
    if (!payload.empty()) {
      payload += "|";
    }
    payload += UrlEncode(key);
    payload += "=";
    payload += UrlEncode(value);
  };

  auto bool_text = [](bool value) { return value ? "1" : "0"; };

  int tab_index = active_tab_index_;
  if (tab_index < 0 || tab_index > 6) {
    tab_index = 1;
  }

  const char* active_tab = "general";
  if (tab_index == 0) {
    active_tab = "profile";
  } else if (tab_index == 1) {
    active_tab = "general";
  } else if (tab_index == 2) {
    active_tab = "email";
  } else if (tab_index == 3) {
    active_tab = "media";
  } else if (tab_index == 4) {
    active_tab = "clock";
  } else if (tab_index == 5) {
    active_tab = "notifications";
  } else if (tab_index == 6) {
    active_tab = "about";
  }

  append("language", language_);
  append("tab", active_tab);
  append("auto_update", bool_text(auto_update_enabled_));
  append("email_enabled", bool_text(email_enabled_));
  append("auto_start", bool_text(auto_start_enabled_));
  append("game_mode", bool_text(game_mode_enabled_));
  append("wp_notifications", bool_text(wp_notifications_enabled_));
  append("notif_whatsapp", bool_text(notification_whatsapp_enabled_));
  append("notif_discord", bool_text(notification_discord_enabled_));
  append("notif_instagram", bool_text(notification_instagram_enabled_));
  append("media_spotify", bool_text(media_spotify_enabled_));
  append("media_ytmusic", bool_text(media_youtube_music_enabled_));
  append("media_apple", bool_text(media_apple_music_enabled_));
  append("media_browser", bool_text(media_browser_enabled_));
  append("clock_24h", bool_text(clock_24h_format_));
  append("marquee", std::to_string(marquee_shift_ms_));

  std::string profiles;
  for (std::size_t i = 0; i < profile_names_.size(); ++i) {
    if (i != 0) {
      profiles += ",";
    }
    profiles += UrlEncode(profile_names_[i]);
  }
  append("profiles", profiles);
  append("active_profile", active_profile_name_);
  append("profile_locked", bool_text(active_profile_locked_));

  std::string profile_path;
  if (!settings_path_.empty()) {
    std::filesystem::path settings_fs(settings_path_);
    const std::filesystem::path exports_dir =
        settings_fs.parent_path().parent_path() / "profile_exports";
    profile_path = (exports_dir / (active_profile_name_ + "-export.ini")).string();
  }
  append("profile_path", profile_path);

  append("imap_server", email_settings_.imap_server);
  append("imap_port", std::to_string(email_settings_.imap_port));
  append("imap_ssl", bool_text(email_settings_.imap_ssl));
  append("smtp_server", email_settings_.smtp_server);
  append("smtp_port", std::to_string(email_settings_.smtp_port));
  append("smtp_ssl", bool_text(email_settings_.smtp_ssl));
  append("email_user", email_settings_.username);
  append("email_interval", std::to_string(email_settings_.poll_interval_seconds));
  append("app_version", app_version_);
  append("developer", developer_);

  return payload;
}

void TrayController::SyncWebViewState() {
  if (!settings_use_webview_ || settings_hwnd_ == nullptr || !IsWindow(settings_hwnd_) ||
      g_webview == nullptr || !settings_webview_ready_) {
    return;
  }

  const std::wstring payload = EscapeJsString(BuildWebViewStatePayload());
  const std::wstring status = EscapeJsString(status_message_);
  const wchar_t* error_flag = status_message_is_error_ ? L"true" : L"false";

  std::wstring script = L"window.__nativeApplyState && window.__nativeApplyState('";
  script += payload;
  script += L"','";
  script += status;
  script += L"',";
  script += error_flag;
  script += L");";

  g_webview->ExecuteScript(script.c_str(), nullptr);
}

void TrayController::HandleWebViewMessage(const std::string& message) {
  std::string action;
  const auto fields = ParseMessageFields(message, action);

  auto get = [&](const std::string& key) -> std::string {
    const auto it = fields.find(key);
    if (it == fields.end()) {
      return {};
    }
    return it->second;
  };

  if (action == "ready") {
    SyncWebViewState();
    return;
  }

  if (action == "tab") {
    const std::string tab = get("name");
    if (tab == "profile") {
      active_tab_index_ = 0;
    } else if (tab == "general") {
      active_tab_index_ = 1;
    } else if (tab == "notifications") {
      active_tab_index_ = 5;
    } else if (tab == "email") {
      active_tab_index_ = 2;
    } else if (tab == "media") {
      active_tab_index_ = 3;
    } else if (tab == "clock") {
      active_tab_index_ = 4;
    } else if (tab == "about") {
      active_tab_index_ = 6;
    }
    SyncWebViewState();
    return;
  }

  if (action == "toggle") {
    const std::string name = get("name");
    if (name == "auto_update") {
      HandleMenuCommand(kMenuAutoUpdateId);
    } else if (name == "email_enabled") {
      HandleMenuCommand(kMenuEmailId);
    } else if (name == "auto_start") {
      HandleMenuCommand(kMenuAutoStartId);
    } else if (name == "game_mode") {
      HandleMenuCommand(kMenuGameModeId);
    } else if (name == "wp_notifications") {
      HandleMenuCommand(kMenuWpNotificationsId);
    } else if (name == "notif_whatsapp") {
      HandleMenuCommand(kMenuNotificationWhatsAppId);
    } else if (name == "notif_discord") {
      HandleMenuCommand(kMenuNotificationDiscordId);
    } else if (name == "notif_instagram") {
      HandleMenuCommand(kMenuNotificationInstagramId);
    } else if (name == "media_spotify") {
      HandleMenuCommand(kMenuMediaSpotifyId);
    } else if (name == "media_ytmusic") {
      HandleMenuCommand(kMenuMediaYoutubeMusicId);
    } else if (name == "media_apple") {
      HandleMenuCommand(kMenuMediaAppleMusicId);
    } else if (name == "media_browser") {
      HandleMenuCommand(kMenuMediaBrowserId);
    }
    SyncWebViewState();
    return;
  }

  if (action == "language") {
    const std::string value = get("value");
    if (value == "en") {
      HandleMenuCommand(kMenuLangEnId);
    } else if (value == "de") {
      HandleMenuCommand(kMenuLangDeId);
    } else if (value == "fr") {
      HandleMenuCommand(kMenuLangFrId);
    } else if (value == "es") {
      HandleMenuCommand(kMenuLangEsId);
    } else {
      HandleMenuCommand(kMenuLangTrId);
    }
    SyncWebViewState();
    return;
  }

  if (action == "open_link") {
    if (get("target") == "bmac") {
      ShellExecuteA(nullptr,
                    "open",
                    "https://buymeacoffee.com/valentinoo",
                    nullptr,
                    nullptr,
                    SW_SHOWNORMAL);
    }
    return;
  }

  if (action == "check_update" && on_manual_update_requested_) {
    const TrayActionStatus status = on_manual_update_requested_();
    SetStatusMessage(status.message, !status.success);
    SyncWebViewState();
    return;
  }

  if (action == "clock") {
    const std::string value = get("value");
    if (value == "24") {
      HandleMenuCommand(kMenuClock24hId);
    } else {
      HandleMenuCommand(kMenuClock12hId);
    }
    SyncWebViewState();
    return;
  }

  if (action == "marquee") {
    int value = 140;
    if (TryParseBoundedInt(get("value"), 50, 2000, value)) {
      if (value <= 100) {
        HandleMenuCommand(kMenuMarqueeFastId);
      } else if (value <= 170) {
        HandleMenuCommand(kMenuMarqueeNormalId);
      } else if (value <= 260) {
        HandleMenuCommand(kMenuMarqueeSlowId);
      } else {
        HandleMenuCommand(kMenuMarqueeVerySlowId);
      }
    }
    SyncWebViewState();
    return;
  }

  if (action == "profile_apply" && on_profile_selected_) {
    const std::string profile = get("name");
    if (!profile.empty()) {
      ApplyProfileResult(on_profile_selected_(profile), profile);
    }
    SyncWebViewState();
    return;
  }

  if (action == "profile_create" && on_profile_created_) {
    const std::string profile = TrimAscii(get("name"));
    if (profile.empty()) {
      SetStatusMessage(language_ == "en" ? "Enter a profile name first."
                                           : "Önce bir profil adı girin.",
                       true);
    } else {
      ApplyProfileResult(on_profile_created_(profile), profile);
    }
    SyncWebViewState();
    return;
  }

  if (action == "profile_copy" && on_profile_copied_) {
    const std::string source = TrimAscii(get("source"));
    const std::string target = TrimAscii(get("target"));
    if (source.empty() || target.empty()) {
      SetStatusMessage(language_ == "en" ? "Source and target profile names are required."
                                           : "Kaynak ve hedef profil adları gerekli.",
                       true);
    } else {
      ApplyProfileResult(on_profile_copied_(source, target), target);
    }
    SyncWebViewState();
    return;
  }

  if (action == "profile_rename" && on_profile_renamed_) {
    const std::string source = TrimAscii(get("source"));
    const std::string target = TrimAscii(get("target"));
    if (source.empty() || target.empty()) {
      SetStatusMessage(language_ == "en" ? "Source and target profile names are required."
                                           : "Kaynak ve hedef profil adları gerekli.",
                       true);
    } else {
      ApplyProfileResult(on_profile_renamed_(source, target), target);
    }
    SyncWebViewState();
    return;
  }

  if (action == "profile_delete" && on_profile_deleted_) {
    const std::string profile = TrimAscii(get("name"));
    if (profile.empty()) {
      SetStatusMessage(language_ == "en" ? "Select a profile first."
                                           : "Önce bir profil seçin.",
                       true);
    } else {
      ApplyProfileResult(on_profile_deleted_(profile), {});
    }
    SyncWebViewState();
    return;
  }

  if (action == "profile_lock" && on_profile_lock_toggled_) {
    const std::string profile = TrimAscii(get("name"));
    const bool should_lock = get("value") == "1";
    if (!profile.empty()) {
      ApplyProfileResult(on_profile_lock_toggled_(profile, should_lock), profile);
    }
    SyncWebViewState();
    return;
  }

  if (action == "profile_export" && on_profile_exported_) {
    const std::string profile = TrimAscii(get("name"));
    const std::string path = TrimAscii(get("path"));
    if (!profile.empty()) {
      ApplyProfileResult(on_profile_exported_(profile, path), profile);
    }
    SyncWebViewState();
    return;
  }

  if (action == "profile_import" && on_profile_imported_) {
    const std::string path = TrimAscii(get("path"));
    const std::string target = TrimAscii(get("target"));
    ApplyProfileResult(on_profile_imported_(path, target), target);
    SyncWebViewState();
    return;
  }

  if (action == "email_apply" && on_email_settings_changed_) {
    TrayEmailSettings updated = email_settings_;
    updated.imap_server = TrimAscii(get("imap_server"));
    updated.smtp_server = TrimAscii(get("smtp_server"));
    updated.username = TrimAscii(get("username"));
    updated.imap_ssl = get("imap_ssl") == "1";
    updated.smtp_ssl = get("smtp_ssl") == "1";

    int parsed = 0;
    if (!TryParseBoundedInt(get("imap_port"), 1, 65535, parsed)) {
      SetStatusMessage(language_ == "en" ? "IMAP port must be between 1 and 65535."
                                           : "IMAP portu 1 ile 65535 arasında olmalıdır.",
                       true);
      SyncWebViewState();
      return;
    }
    updated.imap_port = parsed;

    if (!TryParseBoundedInt(get("smtp_port"), 1, 65535, parsed)) {
      SetStatusMessage(language_ == "en" ? "SMTP port must be between 1 and 65535."
                                           : "SMTP portu 1 ile 65535 arasında olmalıdır.",
                       true);
      SyncWebViewState();
      return;
    }
    updated.smtp_port = parsed;

    if (!TryParseBoundedInt(get("interval"), 5, 600, parsed)) {
      SetStatusMessage(language_ == "en"
                           ? "Poll interval must be between 5 and 600 seconds."
                           : "Kontrol aralığı 5 ile 600 saniye arasında olmalıdır.",
                       true);
      SyncWebViewState();
      return;
    }
    updated.poll_interval_seconds = parsed;

    if (!updated.imap_ssl || !updated.smtp_ssl) {
      SetStatusMessage(language_ == "en"
                           ? "IMAP and SMTP SSL/TLS must be enabled."
                   : "IMAP ve SMTP i\u00e7in SSL/TLS etkin olmal\u0131d\u0131r.",
                       true);
      SyncWebViewState();
      return;
    }

    if (updated.imap_port == 993 && !updated.imap_ssl) {
      SetStatusMessage(language_ == "en"
                           ? "IMAP SSL must be enabled when port is 993."
                   : "IMAP portu 993 iken SSL a\u00e7\u0131k olmal\u0131d\u0131r.",
                       true);
      SyncWebViewState();
      return;
    }

    if (on_email_credential_saved_) {
      const std::string credential_error =
          on_email_credential_saved_(updated.username, get("password"));
      if (!credential_error.empty()) {
        SetStatusMessage(credential_error, true);
        SyncWebViewState();
        return;
      }
    }

    on_email_settings_changed_(updated);
    email_settings_ = updated;
    SetStatusMessage(language_ == "en" ? "Email settings saved."
                                         : "E-posta ayarları kaydedildi.",
                     false);
    SyncWebViewState();
    return;
  }

  if (action == "email_test" && on_email_smtp_test_) {
    TrayEmailSettings probe = email_settings_;
    probe.smtp_server = TrimAscii(get("smtp_server"));
    probe.smtp_ssl = get("smtp_ssl") == "1";
    int parsed = 0;
    if (!TryParseBoundedInt(get("smtp_port"), 1, 65535, parsed)) {
      SetStatusMessage(language_ == "en" ? "SMTP port must be between 1 and 65535."
                                           : "SMTP portu 1 ile 65535 arasında olmalıdır.",
                       true);
      SyncWebViewState();
      return;
    }
    probe.smtp_port = parsed;

    if (!probe.smtp_ssl) {
      SetStatusMessage(language_ == "en"
                           ? "SMTP SSL/TLS must be enabled for connection test."
                   : "Ba\u011flant\u0131 testi i\u00e7in SMTP SSL/TLS etkin olmal\u0131d\u0131r.",
                       true);
      SyncWebViewState();
      return;
    }

    const std::string smtp_error = on_email_smtp_test_(probe);
    if (!smtp_error.empty()) {
      SetStatusMessage(smtp_error, true);
    } else {
      SetStatusMessage(language_ == "en" ? "SMTP connection is healthy."
                                           : "SMTP bağlantısı başarılı.",
                       false);
    }
    SyncWebViewState();
    return;
  }

  if (action == "restart") {
    HandleMenuCommand(kMenuRestartId);
    SyncWebViewState();
    return;
  }

  if (action == "exit") {
    HandleMenuCommand(kMenuExitId);
    return;
  }
}

void TrayController::ShowSettingsUi() {
  if (settings_hwnd_ != nullptr && IsWindow(settings_hwnd_)) {
    if (settings_use_webview_ && !settings_webview_ready_) {
      InitializeSettingsWebView();
    }
    SyncSettingsWindowState();
    ShowWindow(settings_hwnd_, SW_SHOWNORMAL);
    SetForegroundWindow(settings_hwnd_);
    return;
  }

  const bool is_tr = (language_ == "tr");

  const wchar_t* window_title = is_tr ? L"Carex-Ext Ayarlar" : L"Carex-Ext Settings";
  const wchar_t* subtitle_text =
      is_tr ? L"De\u011fi\u015fiklikler an\u0131nda uygulan\u0131r." : L"Changes are applied instantly.";
  const wchar_t* tab_profile = is_tr ? L"Profil" : L"Profile";
  const wchar_t* tab_general = is_tr ? L"Genel" : L"General";
  const wchar_t* tab_email = is_tr ? L"E-posta" : L"Email";
  const wchar_t* tab_media = is_tr ? L"Medya" : L"Media";
  const wchar_t* tab_clock = is_tr ? L"Saat" : L"Clock";
  const wchar_t* tab_advanced = is_tr ? L"Gelismis" : L"Advanced";

  const wchar_t* group_general = is_tr ? L"Genel" : L"General";
  const wchar_t* group_profile = is_tr ? L"Profil" : L"Profile";
    const wchar_t* group_email =
      is_tr ? L"E-posta Sunucu Ayarlar\u0131" : L"Email Server Settings";
  const wchar_t* group_media = is_tr ? L"Medya Kaynaklar\u0131" : L"Media Sources";
  const wchar_t* group_clock_language = is_tr ? L"Saat ve Dil" : L"Time and Language";
  const wchar_t* group_marquee =
      is_tr ? L"Medya Ismi Kayd\u0131rma H\u0131z\u0131" : L"Media Title Scroll Speed";

  const wchar_t* label_auto_update = is_tr ? L"Otomatik g\u00fcncelleme" : L"Auto update";
  const wchar_t* label_email = is_tr ? L"E-posta izleme" : L"Email monitoring";
  const wchar_t* label_autostart =
      is_tr ? L"Windows a\u00e7\u0131l\u0131\u015f\u0131nda ba\u015flat" : L"Start with Windows";
  const wchar_t* label_game_mode = is_tr ? L"Oyun modu" : L"Game mode";

  const wchar_t* label_spotify = L"Spotify";
  const wchar_t* label_ytmusic = L"YouTube Music";
  const wchar_t* label_apple = L"Apple Music";
  const wchar_t* label_browser = is_tr ? L"Taray\u0131c\u0131 medya" : L"Browser media";

  const wchar_t* label_clock_12h = is_tr ? L"12 saat (AM/PM)" : L"12 hour (AM/PM)";
  const wchar_t* label_clock_24h = is_tr ? L"24 saat" : L"24 hour";
  const wchar_t* label_language_tr = is_tr ? L"Dil: T\u00fcrk\u00e7e" : L"Language: Turkish";
  const wchar_t* label_language_en = is_tr ? L"Dil: English" : L"Language: English";
  const wchar_t* label_active_profile = is_tr ? L"Aktif profil" : L"Active profile";
  const wchar_t* label_profile_name = is_tr ? L"Profil ad\u0131" : L"Profile name";
  const wchar_t* label_imap_server = is_tr ? L"IMAP sunucu" : L"IMAP server";
  const wchar_t* label_imap_port = is_tr ? L"IMAP port" : L"IMAP port";
  const wchar_t* label_imap_ssl = is_tr ? L"IMAP SSL" : L"IMAP SSL";
  const wchar_t* label_smtp_server = is_tr ? L"SMTP sunucu" : L"SMTP server";
  const wchar_t* label_smtp_port = is_tr ? L"SMTP port" : L"SMTP port";
  const wchar_t* label_smtp_ssl = is_tr ? L"SMTP SSL" : L"SMTP SSL";
  const wchar_t* label_email_username = is_tr ? L"Kullan\u0131c\u0131 ad\u0131" : L"Username";
  const wchar_t* label_email_interval =
      is_tr ? L"Kontrol aral\u0131\u011f\u0131 (sn)" : L"Poll interval (sec)";

  const wchar_t* label_speed_fast =
      is_tr ? L"\u00c7ok h\u0131zl\u0131 (80ms)" : L"Very fast (80ms)";
  const wchar_t* label_speed_normal = is_tr ? L"Normal (140ms)" : L"Normal (140ms)";
  const wchar_t* label_speed_slow = is_tr ? L"Yava\u015f (220ms)" : L"Slow (220ms)";
  const wchar_t* label_speed_very_slow =
      is_tr ? L"\u00c7ok yava\u015f (320ms)" : L"Very slow (320ms)";

  const wchar_t* button_settings =
      is_tr ? L"Ayar dosyas\u0131n\u0131 a\u00e7" : L"Open settings file";
  const wchar_t* button_apply_profile = is_tr ? L"Profili uygula" : L"Apply profile";
  const wchar_t* button_create_profile = is_tr ? L"Olu\u015ftur" : L"Create";
  const wchar_t* button_copy_profile = is_tr ? L"Kopyala" : L"Copy";
  const wchar_t* button_rename_profile = is_tr ? L"Yeniden adland\u0131r" : L"Rename";
  const wchar_t* button_delete_profile = is_tr ? L"Sil" : L"Delete";
  const wchar_t* button_profile_lock = is_tr ? L"Profili kilitle" : L"Lock profile";
  const wchar_t* button_profile_export = is_tr ? L"Dışa aktar" : L"Export";
  const wchar_t* button_profile_import = is_tr ? L"İçe aktar" : L"Import";
  const wchar_t* label_profile_path = is_tr ? L"Dosya yolu" : L"File path";
  const wchar_t* button_apply_email =
      is_tr ? L"E-posta ayarlar\u0131n\u0131 uygula" : L"Apply email settings";
  const wchar_t* button_test_smtp = is_tr ? L"SMTP testi" : L"Test SMTP";
  const wchar_t* button_restart = is_tr ? L"Yeniden ba\u015flat" : L"Restart";
  const wchar_t* button_exit = is_tr ? L"\u00c7\u0131k\u0131\u015f" : L"Exit";

  HINSTANCE hinstance = GetModuleHandle(nullptr);

  if (settings_background_brush_ == nullptr) {
    settings_background_brush_ = CreateSolidBrush(kSettingsBackgroundColor);
  }

  HICON class_icon = tray_icon_;
  if (class_icon == nullptr) {
    class_icon = LoadIcon(nullptr, IDI_APPLICATION);
  }

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = TrayController::WindowProc;
  wc.hInstance = hinstance;
  wc.lpszClassName = kSettingsWindowClass;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hIcon = class_icon;
  wc.hIconSm = class_icon;
  wc.hbrBackground = settings_background_brush_;
  RegisterClassExW(&wc);

  settings_hwnd_ = CreateWindowExW(
      WS_EX_APPWINDOW, kSettingsWindowClass, window_title,
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
      CW_USEDEFAULT, CW_USEDEFAULT, kSettingsWindowWidth, kSettingsWindowHeight,
      nullptr, nullptr, hinstance, nullptr);
  if (settings_hwnd_ == nullptr) {
    return;
  }

  const LONG_PTR ex_style = GetWindowLongPtrW(settings_hwnd_, GWL_EXSTYLE);
  SetWindowLongPtrW(settings_hwnd_, GWL_EXSTYLE,
                    (ex_style | WS_EX_APPWINDOW) & (~static_cast<LONG_PTR>(WS_EX_TOOLWINDOW)));
  SetWindowPos(settings_hwnd_, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);

  if (settings_font_ == nullptr) {
    settings_font_ = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
  }

  if (settings_title_font_ == nullptr) {
    settings_title_font_ = CreateFontW(-28, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                       DEFAULT_PITCH | FF_DONTCARE, L"Bahnschrift SemiBold");
  }

  if (tray_icon_ != nullptr) {
    SendMessageW(settings_hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(tray_icon_));
    SendMessageW(settings_hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(tray_icon_));
    SetClassLongPtrW(settings_hwnd_, GCLP_HICON, reinterpret_cast<LONG_PTR>(tray_icon_));
    SetClassLongPtrW(settings_hwnd_, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(tray_icon_));
  }

  auto apply_font = [&](HWND control, bool title) {
    if (control != nullptr) {
      SendMessageW(control, WM_SETFONT,
                   reinterpret_cast<WPARAM>(title ? settings_title_font_ : settings_font_),
                   TRUE);
    }
  };

  if (settings_use_webview_) {
    HWND loading_label = CreateWindowW(
        L"STATIC",
        is_tr ? L"Carex-Ext a\u00e7\u0131l\u0131yor..." : L"Carex-Ext is starting...",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        0, 0, kSettingsWindowWidth, kSettingsWindowHeight,
        settings_hwnd_,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsWebViewLoadingId)),
        hinstance, nullptr);
    apply_font(loading_label, false);

    if (InitializeSettingsWebView()) {
      ShowWindow(settings_hwnd_, SW_SHOWNORMAL);
      UpdateWindow(settings_hwnd_);
      SetForegroundWindow(settings_hwnd_);
      return;
    }

    if (loading_label != nullptr) {
      DestroyWindow(loading_label);
    }
  }

  HWND title_label = CreateWindowW(L"STATIC", window_title, WS_CHILD | WS_VISIBLE,
                     132, 14, 470, 36, settings_hwnd_, nullptr, hinstance, nullptr);
  apply_font(title_label, true);

  HWND subtitle_label = CreateWindowW(
      L"STATIC", subtitle_text, WS_CHILD | WS_VISIBLE,
      132, 52, 520, 22, settings_hwnd_, nullptr, hinstance, nullptr);
  apply_font(subtitle_label, false);

  auto add_tab_button = [&](UINT id, const wchar_t* text, int index) {
    DWORD style = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | BS_PUSHLIKE | BS_FLAT;
    if (index == 0) {
      style |= WS_GROUP;
    }

    HWND button = CreateWindowW(
        L"BUTTON", text, style,
        18, 84 + (index * 34), 104, 30, settings_hwnd_,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), hinstance, nullptr);
    apply_font(button, false);
  };

  add_tab_button(kSettingsTabProfileId, tab_profile, 0);
  add_tab_button(kSettingsTabGeneralId, tab_general, 1);
  add_tab_button(kSettingsTabEmailId, tab_email, 2);
  add_tab_button(kSettingsTabMediaId, tab_media, 3);
  add_tab_button(kSettingsTabClockId, tab_clock, 4);
  add_tab_button(kSettingsTabAdvancedId, tab_advanced, 5);

  const int group_left = 16;
    const int group_width = 620;

  auto add_group = [&](UINT id, const wchar_t* text, int y, int height) {
    HWND group = CreateWindowW(L"BUTTON", text,
                               WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                               group_left, y, group_width, height,
                               settings_hwnd_,
                               reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)),
                               hinstance, nullptr);
    apply_font(group, false);
  };

  auto add_toggle = [&](UINT id, const wchar_t* text, bool radio, bool group, int x, int y) {
    DWORD style = WS_CHILD | WS_VISIBLE;
    style |= radio ? BS_AUTORADIOBUTTON : BS_AUTOCHECKBOX;
    if (group) {
      style |= WS_GROUP;
    }

    HWND control = CreateWindowW(
        L"BUTTON", text, style, x, y, 286, 24, settings_hwnd_,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), hinstance, nullptr);
    apply_font(control, false);
  };

  add_group(kSettingsGroupProfileId, group_profile, 84, 210);

  HWND profile_label = CreateWindowW(L"STATIC", label_active_profile, WS_CHILD | WS_VISIBLE,
                                     30, 112, 120, 22, settings_hwnd_,
                                     reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsLabelActiveProfileId)),
                                     hinstance, nullptr);
  apply_font(profile_label, false);

  HWND profile_combo = CreateWindowW(
      L"COMBOBOX", L"",
      WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
      162, 108, 286, 320, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsProfileComboId)),
      hinstance, nullptr);
  apply_font(profile_combo, false);

  if (profile_combo != nullptr) {
    for (const auto& profile_name : profile_names_) {
      std::wstring wide_name = Utf8ToWide(profile_name);
      if (wide_name.empty()) {
        wide_name = L"default";
      }
      SendMessageW(profile_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide_name.c_str()));
    }
  }

  HWND apply_profile_button = CreateWindowW(
      L"BUTTON", button_apply_profile,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
      460, 108, 156, 30, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsProfileApplyId)),
      hinstance, nullptr);
  apply_font(apply_profile_button, false);

  HWND profile_name_label = CreateWindowW(L"STATIC", label_profile_name,
                                          WS_CHILD | WS_VISIBLE,
                                          30, 146, 120, 22,
                                          settings_hwnd_,
                                          reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsLabelProfileNameId)),
                                          hinstance, nullptr);
  apply_font(profile_name_label, false);

  HWND profile_name_edit = CreateWindowExW(
      WS_EX_CLIENTEDGE,
      L"EDIT", L"",
      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      162, 142, 286, 28,
      settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsProfileNameEditId)),
      hinstance, nullptr);
  apply_font(profile_name_edit, false);

  HWND create_profile_button = CreateWindowW(
      L"BUTTON", button_create_profile,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
      162, 176, 110, 30, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsProfileCreateId)),
      hinstance, nullptr);
  apply_font(create_profile_button, false);

    HWND copy_profile_button = CreateWindowW(
      L"BUTTON", button_copy_profile,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
      278, 176, 110, 30, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsProfileCopyId)),
      hinstance, nullptr);
    apply_font(copy_profile_button, false);

  HWND rename_profile_button = CreateWindowW(
      L"BUTTON", button_rename_profile,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
      394, 176, 110, 30, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsProfileRenameId)),
      hinstance, nullptr);
  apply_font(rename_profile_button, false);

  HWND delete_profile_button = CreateWindowW(
      L"BUTTON", button_delete_profile,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
      510, 176, 106, 30, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsProfileDeleteId)),
      hinstance, nullptr);
  apply_font(delete_profile_button, false);

    HWND profile_lock_toggle = CreateWindowW(
      L"BUTTON", button_profile_lock,
      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
      162, 210, 180, 24, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsProfileLockId)),
      hinstance, nullptr);
    apply_font(profile_lock_toggle, false);

    HWND profile_path_label = CreateWindowW(
      L"STATIC", label_profile_path,
      WS_CHILD | WS_VISIBLE,
      30, 238, 120, 22,
      settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsLabelProfilePathId)),
      hinstance, nullptr);
    apply_font(profile_path_label, false);

    HWND profile_path_edit = CreateWindowExW(
      WS_EX_CLIENTEDGE,
      L"EDIT", L"",
      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      162, 234, 300, 28,
      settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsProfilePathEditId)),
      hinstance, nullptr);
    apply_font(profile_path_edit, false);

    HWND export_profile_button = CreateWindowW(
      L"BUTTON", button_profile_export,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
      470, 234, 146, 28, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsProfileExportId)),
      hinstance, nullptr);
    apply_font(export_profile_button, false);

    HWND import_profile_button = CreateWindowW(
      L"BUTTON", button_profile_import,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
      470, 266, 146, 28, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsProfileImportId)),
      hinstance, nullptr);
    apply_font(import_profile_button, false);

    add_group(kSettingsGroupGeneralId, group_general, 232, 116);
    add_toggle(kMenuAutoUpdateId, label_auto_update, false, true, 30, 258);
    add_toggle(kMenuEmailId, label_email, false, false, 30, 286);
    add_toggle(kMenuAutoStartId, label_autostart, false, false, 30, 314);
    add_toggle(kMenuGameModeId, label_game_mode, false, false, 30, 342);

    add_group(kSettingsGroupEmailId, group_email, 354, 176);

    HWND imap_server_label = CreateWindowW(
      L"STATIC", label_imap_server, WS_CHILD | WS_VISIBLE,
      30, 382, 124, 22, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsLabelImapServerId)), hinstance, nullptr);
    apply_font(imap_server_label, false);

    HWND imap_server_edit = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      162, 378, 286, 28, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsEmailImapServerId)),
      hinstance, nullptr);
    apply_font(imap_server_edit, false);

    HWND imap_port_label = CreateWindowW(
      L"STATIC", label_imap_port, WS_CHILD | WS_VISIBLE,
      460, 382, 80, 22, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsLabelImapPortId)), hinstance, nullptr);
    apply_font(imap_port_label, false);

    HWND imap_port_edit = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      542, 378, 74, 28, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsEmailImapPortId)),
      hinstance, nullptr);
    apply_font(imap_port_edit, false);

    HWND imap_ssl_toggle = CreateWindowW(
      L"BUTTON", label_imap_ssl,
      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
      162, 410, 160, 24, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsEmailImapSslId)),
      hinstance, nullptr);
    apply_font(imap_ssl_toggle, false);

    HWND smtp_server_label = CreateWindowW(
      L"STATIC", label_smtp_server, WS_CHILD | WS_VISIBLE,
      30, 438, 124, 22, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsLabelSmtpServerId)), hinstance, nullptr);
    apply_font(smtp_server_label, false);

    HWND smtp_server_edit = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      162, 434, 286, 28, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsEmailSmtpServerId)),
      hinstance, nullptr);
    apply_font(smtp_server_edit, false);

    HWND smtp_port_label = CreateWindowW(
      L"STATIC", label_smtp_port, WS_CHILD | WS_VISIBLE,
      460, 438, 80, 22, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsLabelSmtpPortId)), hinstance, nullptr);
    apply_font(smtp_port_label, false);

    HWND smtp_port_edit = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      542, 434, 74, 28, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsEmailSmtpPortId)),
      hinstance, nullptr);
    apply_font(smtp_port_edit, false);

    HWND smtp_ssl_toggle = CreateWindowW(
      L"BUTTON", label_smtp_ssl,
      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
      162, 466, 160, 24, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsEmailSmtpSslId)),
      hinstance, nullptr);
    apply_font(smtp_ssl_toggle, false);

    HWND email_username_label = CreateWindowW(
      L"STATIC", label_email_username, WS_CHILD | WS_VISIBLE,
      30, 494, 124, 22, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsLabelEmailUsernameId)), hinstance, nullptr);
    apply_font(email_username_label, false);

    HWND email_username_edit = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      162, 490, 202, 28, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsEmailUsernameId)),
      hinstance, nullptr);
    apply_font(email_username_edit, false);

    HWND email_interval_label = CreateWindowW(
      L"STATIC", label_email_interval, WS_CHILD | WS_VISIBLE,
      374, 494, 146, 22, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsLabelEmailIntervalId)), hinstance, nullptr);
    apply_font(email_interval_label, false);

    HWND email_interval_edit = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      522, 490, 94, 28, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsEmailPollIntervalId)),
      hinstance, nullptr);
    apply_font(email_interval_edit, false);

    HWND email_apply_button = CreateWindowW(
      L"BUTTON", button_apply_email,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
      362, 458, 148, 30, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsEmailApplyId)),
      hinstance, nullptr);
    apply_font(email_apply_button, false);

    HWND email_test_button = CreateWindowW(
      L"BUTTON", button_test_smtp,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
      516, 458, 100, 30, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsEmailTestSmtpId)),
      hinstance, nullptr);
    apply_font(email_test_button, false);

    add_group(kSettingsGroupMediaId, group_media, 526, 130);
    add_toggle(kMenuMediaSpotifyId, label_spotify, false, true, 30, 546);
    add_toggle(kMenuMediaYoutubeMusicId, label_ytmusic, false, false, 30, 574);
    add_toggle(kMenuMediaAppleMusicId, label_apple, false, false, 30, 602);
    add_toggle(kMenuMediaBrowserId, label_browser, false, false, 30, 630);

    add_group(kSettingsGroupClockLanguageId, group_clock_language, 662, 96);
    add_toggle(kMenuClock12hId, label_clock_12h, true, true, 30, 688);
    add_toggle(kMenuClock24hId, label_clock_24h, true, false, 30, 716);
    add_toggle(kMenuLangTrId, label_language_tr, true, true, 336, 688);
    add_toggle(kMenuLangEnId, label_language_en, true, false, 336, 716);

    add_group(kSettingsGroupMarqueeId, group_marquee, 764, 86);
    add_toggle(kMenuMarqueeFastId, label_speed_fast, true, true, 30, 790);
    add_toggle(kMenuMarqueeNormalId, label_speed_normal, true, false, 30, 818);
    add_toggle(kMenuMarqueeSlowId, label_speed_slow, true, false, 336, 790);
    add_toggle(kMenuMarqueeVerySlowId, label_speed_very_slow, true, false, 336, 818);

  HWND inline_status = CreateWindowW(
      L"STATIC", L"", WS_CHILD | WS_VISIBLE,
      24, 832, 592, 22, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsInlineStatusId)),
      hinstance, nullptr);
  apply_font(inline_status, false);

  HWND settings_button = CreateWindowW(
        L"BUTTON", button_settings,
      WS_CHILD | BS_PUSHBUTTON | BS_FLAT,
        24, 860, 146, 32, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsOpenSettingsFileId)),
      hinstance, nullptr);
  apply_font(settings_button, false);

  HWND restart_button = CreateWindowW(
        L"BUTTON", button_restart,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
        178, 860, 146, 32, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsRestartId)), hinstance, nullptr);
  apply_font(restart_button, false);

  HWND exit_button = CreateWindowW(
        L"BUTTON", button_exit,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
        332, 860, 130, 32, settings_hwnd_,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsExitId)), hinstance, nullptr);
  apply_font(exit_button, false);

  // Keep the content pane clear from the tab strip by shifting all content controls right.
  const int content_shift_x = 108;
  auto shift_control_x = [&](UINT id) {
    HWND control = GetDlgItem(settings_hwnd_, static_cast<int>(id));
    if (control == nullptr) {
      return;
    }

    RECT rect{};
    if (!GetWindowRect(control, &rect)) {
      return;
    }

    MapWindowPoints(nullptr, settings_hwnd_, reinterpret_cast<POINT*>(&rect), 2);
    SetWindowPos(control, nullptr, rect.left + content_shift_x, rect.top, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  };

  const UINT content_controls_to_shift[] = {
      kSettingsGroupProfileId,
      kSettingsLabelActiveProfileId,
      kSettingsLabelProfileNameId,
      kSettingsLabelProfilePathId,
      kSettingsProfileComboId,
      kSettingsProfileApplyId,
      kSettingsProfileNameEditId,
      kSettingsProfileCreateId,
      kSettingsProfileCopyId,
      kSettingsProfileRenameId,
      kSettingsProfileDeleteId,
      kSettingsProfileLockId,
      kSettingsProfilePathEditId,
      kSettingsProfileExportId,
      kSettingsProfileImportId,
      kSettingsGroupGeneralId,
      kMenuAutoUpdateId,
      kMenuEmailId,
      kMenuAutoStartId,
      kMenuGameModeId,
      kSettingsGroupEmailId,
      kSettingsLabelImapServerId,
      kSettingsLabelImapPortId,
      kSettingsLabelSmtpServerId,
      kSettingsLabelSmtpPortId,
      kSettingsLabelEmailUsernameId,
      kSettingsLabelEmailIntervalId,
      kSettingsEmailImapServerId,
      kSettingsEmailImapPortId,
      kSettingsEmailImapSslId,
      kSettingsEmailSmtpServerId,
      kSettingsEmailSmtpPortId,
      kSettingsEmailSmtpSslId,
      kSettingsEmailUsernameId,
      kSettingsEmailPollIntervalId,
      kSettingsEmailApplyId,
      kSettingsEmailTestSmtpId,
      kSettingsGroupMediaId,
      kMenuMediaSpotifyId,
      kMenuMediaYoutubeMusicId,
      kMenuMediaAppleMusicId,
      kMenuMediaBrowserId,
      kSettingsGroupClockLanguageId,
      kMenuClock12hId,
      kMenuClock24hId,
      kMenuLangTrId,
      kMenuLangEnId,
      kSettingsGroupMarqueeId,
      kMenuMarqueeFastId,
      kMenuMarqueeNormalId,
      kMenuMarqueeSlowId,
      kMenuMarqueeVerySlowId,
      kSettingsInlineStatusId,
      kSettingsOpenSettingsFileId,
      kMenuLogsId,
      kSettingsRestartId,
      kSettingsExitId,
  };

  for (const UINT id : content_controls_to_shift) {
    shift_control_x(id);
  }

  if (settings_path_.empty() && settings_button != nullptr) {
    EnableWindow(settings_button, FALSE);
  }

  if (profile_path_edit != nullptr && !settings_path_.empty()) {
    std::filesystem::path settings_fs(settings_path_);
    const std::filesystem::path exports_dir = settings_fs.parent_path().parent_path() / "profile_exports";
    std::wstring default_path = exports_dir.wstring() + L"\\" + Utf8ToWide(active_profile_name_);
    default_path += L"-export.ini";
    SetWindowTextW(profile_path_edit, default_path.c_str());
  }

  SyncSettingsWindowState();
  ShowWindow(settings_hwnd_, SW_SHOWNORMAL);
  UpdateWindow(settings_hwnd_);
  SetForegroundWindow(settings_hwnd_);
}

void TrayController::SyncSettingsWindowState() {
  if (settings_hwnd_ == nullptr || !IsWindow(settings_hwnd_)) {
    return;
  }

  if (settings_use_webview_) {
    SyncWebViewState();
    return;
  }

  auto set_check = [&](UINT id, bool checked) {
    HWND control = GetDlgItem(settings_hwnd_, static_cast<int>(id));
    if (control != nullptr) {
      SendMessageW(control, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }
  };

  set_check(kMenuAutoUpdateId, auto_update_enabled_);
  set_check(kMenuEmailId, email_enabled_);
  set_check(kMenuAutoStartId, auto_start_enabled_);
  set_check(kMenuGameModeId, game_mode_enabled_);
  set_check(kMenuMediaSpotifyId, media_spotify_enabled_);
  set_check(kMenuMediaYoutubeMusicId, media_youtube_music_enabled_);
  set_check(kMenuMediaAppleMusicId, media_apple_music_enabled_);
  set_check(kMenuMediaBrowserId, media_browser_enabled_);
  set_check(kMenuClock12hId, !clock_24h_format_);
  set_check(kMenuClock24hId, clock_24h_format_);
  set_check(kMenuLangTrId, language_ == "tr");
  set_check(kMenuLangEnId, language_ == "en");
  set_check(kMenuLangDeId, language_ == "de");
  set_check(kMenuLangFrId, language_ == "fr");
  set_check(kMenuLangEsId, language_ == "es");
  set_check(kMenuMarqueeFastId, marquee_shift_ms_ == 80);
  set_check(kMenuMarqueeNormalId, marquee_shift_ms_ == 140);
  set_check(kMenuMarqueeSlowId, marquee_shift_ms_ == 220);
  set_check(kMenuMarqueeVerySlowId, marquee_shift_ms_ == 320);
  set_check(kSettingsEmailImapSslId, email_settings_.imap_ssl);
  set_check(kSettingsEmailSmtpSslId, email_settings_.smtp_ssl);
  set_check(kSettingsProfileLockId, active_profile_locked_);

  auto set_text = [&](UINT id, const std::wstring& text) {
    HWND control = GetDlgItem(settings_hwnd_, static_cast<int>(id));
    if (control != nullptr) {
      SetWindowTextW(control, text.c_str());
    }
  };

  set_text(kSettingsEmailImapServerId, Utf8ToWide(email_settings_.imap_server));
  set_text(kSettingsEmailImapPortId, std::to_wstring(email_settings_.imap_port));
  set_text(kSettingsEmailSmtpServerId, Utf8ToWide(email_settings_.smtp_server));
  set_text(kSettingsEmailSmtpPortId, std::to_wstring(email_settings_.smtp_port));
  set_text(kSettingsEmailUsernameId, Utf8ToWide(email_settings_.username));
  set_text(kSettingsEmailPollIntervalId,
           std::to_wstring(email_settings_.poll_interval_seconds));
  set_text(kSettingsInlineStatusId, Utf8ToWide(status_message_));

  auto set_enabled = [&](UINT id, bool enabled) {
    HWND control = GetDlgItem(settings_hwnd_, static_cast<int>(id));
    if (control != nullptr) {
      EnableWindow(control, enabled ? TRUE : FALSE);
    }
  };

  set_enabled(kSettingsProfileRenameId, !active_profile_locked_);
  set_enabled(kSettingsProfileDeleteId, !active_profile_locked_);
  set_enabled(kSettingsProfileCopyId, !active_profile_locked_);

  SetEmailControlsEnabled(email_enabled_);

  HWND combo = GetDlgItem(settings_hwnd_, static_cast<int>(kSettingsProfileComboId));
  if (combo != nullptr) {
    const LRESULT current_count = SendMessageW(combo, CB_GETCOUNT, 0, 0);
    if (current_count != static_cast<LRESULT>(profile_names_.size())) {
      SendMessageW(combo, CB_RESETCONTENT, 0, 0);
      for (const auto& profile_name : profile_names_) {
        std::wstring wide_name = Utf8ToWide(profile_name);
        if (wide_name.empty()) {
          wide_name = L"default";
        }
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide_name.c_str()));
      }
    }

    int active_index = 0;
    for (std::size_t i = 0; i < profile_names_.size(); ++i) {
      if (profile_names_[i] == active_profile_name_) {
        active_index = static_cast<int>(i);
        break;
      }
    }

    SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(active_index), 0);
  }

  SetActiveTab(active_tab_index_);
}

void TrayController::ApplyProfileResult(const TrayProfileSelectionResult& result,
                                        const std::string& fallback_profile_name) {
  if (!result.success) {
    SetStatusMessage(result.error_message.empty()
                         ? (language_ == "en" ? "Operation failed." : "İşlem başarısız oldu.")
                         : result.error_message,
                     true);

    SyncSettingsWindowState();
    return;
  }

  const std::string previous_language = language_;

  if (!result.settings_path.empty()) {
    settings_path_ = result.settings_path;
  }
  auto_update_enabled_ = result.auto_update_enabled;
  email_enabled_ = result.email_enabled;
  auto_start_enabled_ = result.auto_start_enabled;
  game_mode_enabled_ = result.game_mode_enabled;
  wp_notifications_enabled_ = result.wp_notifications_enabled;
  notification_whatsapp_enabled_ = result.notification_whatsapp_enabled;
  notification_discord_enabled_ = result.notification_discord_enabled;
  notification_instagram_enabled_ = result.notification_instagram_enabled;
  media_spotify_enabled_ = result.media_spotify_enabled;
  media_youtube_music_enabled_ = result.media_youtube_music_enabled;
  media_apple_music_enabled_ = result.media_apple_music_enabled;
  media_browser_enabled_ = result.media_browser_enabled;
  clock_24h_format_ = result.clock_24h_format;
  if (!result.language.empty()) {
    language_ = result.language;
  }
  marquee_shift_ms_ = result.marquee_shift_ms;
  email_settings_ = result.email_settings;
  if (!result.profile_names.empty()) {
    profile_names_ = result.profile_names;
  }

  if (!result.active_profile_name.empty()) {
    active_profile_name_ = result.active_profile_name;
  } else if (!fallback_profile_name.empty()) {
    active_profile_name_ = fallback_profile_name;
  }

  active_profile_locked_ = result.active_profile_locked;

  if (previous_language != language_ && settings_hwnd_ != nullptr && IsWindow(settings_hwnd_)) {
    DestroyWindow(settings_hwnd_);
    settings_hwnd_ = nullptr;
    ShowSettingsUi();
    return;
  }

  if (!result.info_message.empty()) {
    SetStatusMessage(result.info_message, false);
  } else {
    SetStatusMessage({}, false);
  }

  SyncSettingsWindowState();
}

void TrayController::SetStatusMessage(const std::string& message, bool is_error) {
  status_message_ = message;
  status_message_is_error_ = is_error;

  if (settings_use_webview_) {
    SyncWebViewState();
    return;
  }

  if (settings_hwnd_ == nullptr || !IsWindow(settings_hwnd_)) {
    return;
  }

  HWND status_label = GetDlgItem(settings_hwnd_, static_cast<int>(kSettingsInlineStatusId));
  if (status_label != nullptr) {
    const std::wstring text = Utf8ToWide(message);
    SetWindowTextW(status_label, text.c_str());
    InvalidateRect(status_label, nullptr, TRUE);
  }
}

void TrayController::SetActiveTab(int tab_index) {
  if (tab_index < 0 || tab_index > 6) {
    tab_index = 0;
  }

  active_tab_index_ = tab_index;
  if (settings_use_webview_) {
    SyncWebViewState();
    return;
  }

  if (settings_hwnd_ == nullptr || !IsWindow(settings_hwnd_)) {
    return;
  }

  auto set_tab_checked = [&](UINT id, bool checked) {
    HWND button = GetDlgItem(settings_hwnd_, static_cast<int>(id));
    if (button != nullptr) {
      SendMessageW(button, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }
  };

  set_tab_checked(kSettingsTabProfileId, active_tab_index_ == 0);
  set_tab_checked(kSettingsTabGeneralId, active_tab_index_ == 1);
  set_tab_checked(kSettingsTabEmailId, active_tab_index_ == 2);
  set_tab_checked(kSettingsTabMediaId, active_tab_index_ == 3);
  set_tab_checked(kSettingsTabClockId, active_tab_index_ == 4);
  set_tab_checked(kSettingsTabAdvancedId, active_tab_index_ == 5);

  const UINT profile_controls[] = {
      kSettingsGroupProfileId,
      kSettingsLabelActiveProfileId,
      kSettingsLabelProfileNameId,
      kSettingsLabelProfilePathId,
      kSettingsProfileComboId,
      kSettingsProfileApplyId,
      kSettingsProfileNameEditId,
      kSettingsProfileCreateId,
      kSettingsProfileCopyId,
      kSettingsProfileRenameId,
      kSettingsProfileDeleteId,
      kSettingsProfileLockId,
      kSettingsProfilePathEditId,
      kSettingsProfileExportId,
      kSettingsProfileImportId,
  };
  const UINT general_controls[] = {
      kSettingsGroupGeneralId,
      kMenuAutoUpdateId,
      kMenuEmailId,
      kMenuAutoStartId,
      kMenuGameModeId,
  };
  const UINT email_controls[] = {
      kSettingsGroupEmailId,
      kSettingsLabelImapServerId,
      kSettingsLabelImapPortId,
      kSettingsLabelSmtpServerId,
      kSettingsLabelSmtpPortId,
      kSettingsLabelEmailUsernameId,
      kSettingsLabelEmailIntervalId,
      kSettingsEmailImapServerId,
      kSettingsEmailImapPortId,
      kSettingsEmailImapSslId,
      kSettingsEmailSmtpServerId,
      kSettingsEmailSmtpPortId,
      kSettingsEmailSmtpSslId,
      kSettingsEmailUsernameId,
      kSettingsEmailPollIntervalId,
      kSettingsEmailApplyId,
      kSettingsEmailTestSmtpId,
  };
  const UINT media_controls[] = {
      kSettingsGroupMediaId,
      kSettingsGroupMarqueeId,
      kMenuMediaSpotifyId,
      kMenuMediaYoutubeMusicId,
      kMenuMediaAppleMusicId,
      kMenuMediaBrowserId,
      kMenuMarqueeFastId,
      kMenuMarqueeNormalId,
      kMenuMarqueeSlowId,
      kMenuMarqueeVerySlowId,
  };
  const UINT clock_controls[] = {
      kSettingsGroupClockLanguageId,
      kMenuClock12hId,
      kMenuClock24hId,
      kMenuLangTrId,
      kMenuLangEnId,
      kMenuLangDeId,
      kMenuLangFrId,
      kMenuLangEsId,
  };
  const UINT advanced_controls[] = {
      kSettingsOpenSettingsFileId,
      kSettingsRestartId,
      kSettingsExitId,
  };

  auto apply_visible = [&](const UINT* controls, std::size_t count, bool visible) {
    for (std::size_t i = 0; i < count; ++i) {
      SetControlVisible(settings_hwnd_, controls[i], visible);
    }
  };

  apply_visible(profile_controls, sizeof(profile_controls) / sizeof(profile_controls[0]),
                active_tab_index_ == 0);
  apply_visible(general_controls, sizeof(general_controls) / sizeof(general_controls[0]),
                active_tab_index_ == 1);
  apply_visible(email_controls, sizeof(email_controls) / sizeof(email_controls[0]),
                active_tab_index_ == 2);
  apply_visible(media_controls, sizeof(media_controls) / sizeof(media_controls[0]),
                active_tab_index_ == 3);
  apply_visible(clock_controls, sizeof(clock_controls) / sizeof(clock_controls[0]),
                active_tab_index_ == 4);
  apply_visible(advanced_controls, sizeof(advanced_controls) / sizeof(advanced_controls[0]),
                active_tab_index_ == 5);

  // Bottom action buttons should stay available regardless of active tab.
  SetControlVisible(settings_hwnd_, kSettingsOpenSettingsFileId, false);
  SetControlVisible(settings_hwnd_, kMenuLogsId, false);
  SetControlVisible(settings_hwnd_, kSettingsRestartId, true);
  SetControlVisible(settings_hwnd_, kSettingsExitId, true);
  SetControlVisible(settings_hwnd_, kSettingsInlineStatusId, true);
}

void TrayController::SetEmailControlsEnabled(bool enabled) {
  if (settings_hwnd_ == nullptr || !IsWindow(settings_hwnd_)) {
    return;
  }

  const UINT email_control_ids[] = {
      kSettingsEmailImapServerId,
      kSettingsEmailImapPortId,
      kSettingsEmailImapSslId,
      kSettingsEmailSmtpServerId,
      kSettingsEmailSmtpPortId,
      kSettingsEmailSmtpSslId,
      kSettingsEmailUsernameId,
      kSettingsEmailPollIntervalId,
      kSettingsEmailApplyId,
        kSettingsEmailTestSmtpId,
  };

  for (const UINT id : email_control_ids) {
    HWND control = GetDlgItem(settings_hwnd_, static_cast<int>(id));
    if (control != nullptr) {
      EnableWindow(control, enabled ? TRUE : FALSE);
    }
  }
}

void TrayController::HandleMenuCommand(UINT clicked) {
  if (clicked == 0) {
    return;
  }

  const UINT profile_count = static_cast<UINT>((std::min)(
      profile_names_.size(), static_cast<std::size_t>(kMenuProfileMaxCount)));
  if (clicked >= kMenuProfileBaseId && clicked < kMenuProfileBaseId + profile_count) {
    const std::size_t index = static_cast<std::size_t>(clicked - kMenuProfileBaseId);
    if (index < profile_names_.size() && on_profile_selected_) {
      const std::string selected_profile = profile_names_[index];
      const TrayProfileSelectionResult result = on_profile_selected_(selected_profile);
      ApplyProfileResult(result, selected_profile);
    }
    return;
  }

  bool rebuild_settings_window = false;

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
  } else if (clicked == kMenuWpNotificationsId) {
    wp_notifications_enabled_ = !wp_notifications_enabled_;
    if (on_wp_notifications_toggled_) {
      on_wp_notifications_toggled_(wp_notifications_enabled_);
    }
  } else if (clicked == kMenuNotificationWhatsAppId) {
    notification_whatsapp_enabled_ = !notification_whatsapp_enabled_;
    if (on_notification_source_toggled_) {
      on_notification_source_toggled_("whatsapp", notification_whatsapp_enabled_);
    }
  } else if (clicked == kMenuNotificationDiscordId) {
    notification_discord_enabled_ = !notification_discord_enabled_;
    if (on_notification_source_toggled_) {
      on_notification_source_toggled_("discord", notification_discord_enabled_);
    }
  } else if (clicked == kMenuNotificationInstagramId) {
    notification_instagram_enabled_ = !notification_instagram_enabled_;
    if (on_notification_source_toggled_) {
      on_notification_source_toggled_("instagram", notification_instagram_enabled_);
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
    rebuild_settings_window = true;
  } else if (clicked == kMenuLangEnId) {
    language_ = "en";
    if (on_language_changed_) {
      on_language_changed_(language_);
    }
    rebuild_settings_window = true;
  } else if (clicked == kMenuLangDeId) {
    language_ = "de";
    if (on_language_changed_) {
      on_language_changed_(language_);
    }
    rebuild_settings_window = true;
  } else if (clicked == kMenuLangFrId) {
    language_ = "fr";
    if (on_language_changed_) {
      on_language_changed_(language_);
    }
    rebuild_settings_window = true;
  } else if (clicked == kMenuLangEsId) {
    language_ = "es";
    if (on_language_changed_) {
      on_language_changed_(language_);
    }
    rebuild_settings_window = true;
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

  if (rebuild_settings_window && !settings_use_webview_ && settings_hwnd_ != nullptr &&
      IsWindow(settings_hwnd_)) {
    DestroyWindow(settings_hwnd_);
    settings_hwnd_ = nullptr;
    ShowSettingsUi();
    return;
  }

  SyncSettingsWindowState();
}

LRESULT CALLBACK TrayController::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (g_instance == nullptr) {
    return DefWindowProc(hwnd, msg, wparam, lparam);
  }

  if (hwnd == g_instance->settings_hwnd_) {
    if (g_instance->settings_use_webview_) {
      if (msg == kWebViewStateSyncMessage) {
        g_instance->SyncWebViewState();
        return 0;
      }

      if (msg == WM_SIZE) {
        if (wparam == SIZE_MINIMIZED) {
          DestroyWindow(hwnd);
          return 0;
        }

        RECT rect{};
        GetClientRect(hwnd, &rect);

        HWND loading = GetDlgItem(hwnd, static_cast<int>(kSettingsWebViewLoadingId));
        if (loading != nullptr) {
          SetWindowPos(loading, nullptr, rect.left, rect.top,
                       rect.right - rect.left, rect.bottom - rect.top,
                       SWP_NOZORDER | SWP_NOACTIVATE);
        }

        if (g_webview_controller != nullptr) {
          g_webview_controller->put_Bounds(rect);
        }
        return 0;
      }

      if (msg == WM_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
      }

      if (msg == WM_DESTROY) {
        g_webview = nullptr;
        g_webview_controller = nullptr;
        g_webview_environment = nullptr;
        g_instance->settings_webview_ready_ = false;
        g_instance->settings_hwnd_ = nullptr;
        return 0;
      }

      return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    if (msg == WM_SIZE && wparam == SIZE_MINIMIZED) {
      DestroyWindow(hwnd);
      return 0;
    }

    if ((msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLORBTN) &&
        g_instance->settings_background_brush_ != nullptr) {
      HDC hdc = reinterpret_cast<HDC>(wparam);
      SetBkColor(hdc, kSettingsBackgroundColor);
      if (msg == WM_CTLCOLORSTATIC) {
        HWND control = reinterpret_cast<HWND>(lparam);
        const int control_id = GetDlgCtrlID(control);
        if (control_id == static_cast<int>(kSettingsInlineStatusId)) {
          SetTextColor(hdc, g_instance->status_message_is_error_ ? kSettingsErrorTextColor
                                                                  : kSettingsSuccessTextColor);
        } else {
          SetTextColor(hdc, kSettingsTextColor);
        }
      } else {
        SetTextColor(hdc, kSettingsTextColor);
      }
      return reinterpret_cast<LRESULT>(g_instance->settings_background_brush_);
    }

    if (msg == WM_ERASEBKGND && g_instance->settings_background_brush_ != nullptr) {
      RECT rect{};
      GetClientRect(hwnd, &rect);
      FillRect(reinterpret_cast<HDC>(wparam), &rect, g_instance->settings_background_brush_);
      return 1;
    }

    if (msg == WM_COMMAND && HIWORD(wparam) == BN_CLICKED) {
      const UINT clicked = LOWORD(wparam);

      auto report_profile_error = [&](const std::string& message) {
        TrayProfileSelectionResult result;
        result.success = false;
        result.error_message = message;
        g_instance->ApplyProfileResult(result, {});
      };

      auto read_selected_profile_name = [&]() -> std::string {
        HWND combo = GetDlgItem(g_instance->settings_hwnd_, static_cast<int>(kSettingsProfileComboId));
        if (combo != nullptr) {
          const LRESULT selection = SendMessageW(combo, CB_GETCURSEL, 0, 0);
          if (selection != CB_ERR) {
            const std::size_t index = static_cast<std::size_t>(selection);
            if (index < g_instance->profile_names_.size()) {
              return g_instance->profile_names_[index];
            }
          }
        }

        return g_instance->active_profile_name_;
      };

      auto read_profile_input_name = [&]() -> std::string {
        HWND edit = GetDlgItem(g_instance->settings_hwnd_, static_cast<int>(kSettingsProfileNameEditId));
        if (edit == nullptr) {
          return {};
        }

        std::array<wchar_t, 260> buffer{};
        GetWindowTextW(edit, buffer.data(), static_cast<int>(buffer.size()));
        std::wstring value(buffer.data());
        const std::wstring whitespace = L" \t\r\n";
        const auto first = value.find_first_not_of(whitespace);
        if (first == std::wstring::npos) {
          return {};
        }

        const auto last = value.find_last_not_of(whitespace);
        value = value.substr(first, last - first + 1);
        return WideToUtf8(value);
      };

      auto read_profile_path_input = [&]() -> std::string {
        HWND edit = GetDlgItem(g_instance->settings_hwnd_, static_cast<int>(kSettingsProfilePathEditId));
        if (edit == nullptr) {
          return {};
        }

        std::array<wchar_t, 512> buffer{};
        GetWindowTextW(edit, buffer.data(), static_cast<int>(buffer.size()));
        return TrimAscii(WideToUtf8(std::wstring(buffer.data())));
      };

      auto read_edit_utf8 = [&](UINT control_id) -> std::string {
        HWND edit = GetDlgItem(g_instance->settings_hwnd_, static_cast<int>(control_id));
        if (edit == nullptr) {
          return {};
        }

        std::array<wchar_t, 512> buffer{};
        GetWindowTextW(edit, buffer.data(), static_cast<int>(buffer.size()));
        return TrimAscii(WideToUtf8(std::wstring(buffer.data())));
      };

      auto read_checkbox = [&](UINT control_id) {
        HWND control = GetDlgItem(g_instance->settings_hwnd_, static_cast<int>(control_id));
        if (control == nullptr) {
          return false;
        }

        return SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
      };

      auto clear_profile_input = [&]() {
        HWND edit = GetDlgItem(g_instance->settings_hwnd_, static_cast<int>(kSettingsProfileNameEditId));
        if (edit != nullptr) {
          SetWindowTextW(edit, L"");
        }
      };

      if (clicked == kSettingsCloseId) {
        DestroyWindow(hwnd);
        return 0;
      }

      if (clicked == kSettingsOpenSettingsFileId) {
        if (!g_instance->settings_path_.empty()) {
          ShellExecuteA(nullptr, "open", g_instance->settings_path_.c_str(), nullptr, nullptr,
                        SW_SHOWNORMAL);
        }
        return 0;
      }

      if (clicked == kSettingsTabProfileId) {
        g_instance->SetActiveTab(0);
        return 0;
      }

      if (clicked == kSettingsTabGeneralId) {
        g_instance->SetActiveTab(1);
        return 0;
      }

      if (clicked == kSettingsTabEmailId) {
        g_instance->SetActiveTab(2);
        return 0;
      }

      if (clicked == kSettingsTabMediaId) {
        g_instance->SetActiveTab(3);
        return 0;
      }

      if (clicked == kSettingsTabClockId) {
        g_instance->SetActiveTab(4);
        return 0;
      }

      if (clicked == kSettingsTabAdvancedId) {
        g_instance->SetActiveTab(5);
        return 0;
      }

      if (clicked == kSettingsProfileApplyId) {
        HWND combo =
            GetDlgItem(g_instance->settings_hwnd_, static_cast<int>(kSettingsProfileComboId));
        if (combo != nullptr) {
          const LRESULT selection = SendMessageW(combo, CB_GETCURSEL, 0, 0);
          if (selection != CB_ERR) {
            g_instance->HandleMenuCommand(kMenuProfileBaseId + static_cast<UINT>(selection));
          }
        }
        return 0;
      }

      if (clicked == kSettingsEmailApplyId) {
        if (!g_instance->on_email_settings_changed_) {
          return 0;
        }

        if (!g_instance->email_enabled_) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "Enable email monitoring first."
                                   : "Önce e-posta izlemeyi açın.");
          return 0;
        }

        TrayEmailSettings updated = g_instance->email_settings_;
        updated.imap_server = read_edit_utf8(kSettingsEmailImapServerId);
        updated.smtp_server = read_edit_utf8(kSettingsEmailSmtpServerId);
        updated.username = read_edit_utf8(kSettingsEmailUsernameId);

        int imap_port = 0;
        if (!TryParseBoundedInt(read_edit_utf8(kSettingsEmailImapPortId), 1, 65535, imap_port)) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "IMAP port must be between 1 and 65535."
                                   : "IMAP portu 1 ile 65535 arasında olmalıdır.");
          return 0;
        }

        int smtp_port = 0;
        if (!TryParseBoundedInt(read_edit_utf8(kSettingsEmailSmtpPortId), 1, 65535, smtp_port)) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "SMTP port must be between 1 and 65535."
                                   : "SMTP portu 1 ile 65535 arasında olmalıdır.");
          return 0;
        }

        int poll_interval_seconds = 0;
        if (!TryParseBoundedInt(read_edit_utf8(kSettingsEmailPollIntervalId), 5, 600,
                                poll_interval_seconds)) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "Poll interval must be between 5 and 600 seconds."
                                   : "Kontrol aralığı 5 ile 600 saniye arasında olmalıdır.");
          return 0;
        }

        updated.imap_port = imap_port;
        updated.smtp_port = smtp_port;
        updated.poll_interval_seconds = poll_interval_seconds;
        updated.imap_ssl = read_checkbox(kSettingsEmailImapSslId);
        updated.smtp_ssl = read_checkbox(kSettingsEmailSmtpSslId);

        if (!updated.imap_ssl || !updated.smtp_ssl) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "IMAP and SMTP SSL/TLS must be enabled."
                                   : "IMAP ve SMTP i\u00e7in SSL/TLS etkin olmal\u0131d\u0131r.");
          return 0;
        }

        if (updated.imap_port == 993 && !updated.imap_ssl) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "IMAP SSL must be enabled when port is 993."
                                   : "IMAP portu 993 iken SSL a\u00e7\u0131k olmal\u0131d\u0131r.");
          return 0;
        }

        if (g_instance->on_email_credential_saved_) {
          const std::string credential_error =
              g_instance->on_email_credential_saved_(updated.username, {});
          if (!credential_error.empty()) {
            report_profile_error(credential_error);
            return 0;
          }
        }

        g_instance->on_email_settings_changed_(updated);
        g_instance->email_settings_ = updated;
        g_instance->SetStatusMessage(g_instance->language_ == "en"
                                         ? "Email settings saved."
                         : "E-posta ayarları kaydedildi.",
                                     false);
        g_instance->SyncSettingsWindowState();
        return 0;
      }

      if (clicked == kSettingsEmailTestSmtpId) {
        if (!g_instance->on_email_smtp_test_) {
          return 0;
        }

        TrayEmailSettings probe = g_instance->email_settings_;
        probe.smtp_server = read_edit_utf8(kSettingsEmailSmtpServerId);

        int smtp_port = 0;
        if (!TryParseBoundedInt(read_edit_utf8(kSettingsEmailSmtpPortId), 1, 65535, smtp_port)) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "SMTP port must be between 1 and 65535."
                                   : "SMTP portu 1 ile 65535 arasında olmalıdır.");
          return 0;
        }

        probe.smtp_port = smtp_port;
        probe.smtp_ssl = read_checkbox(kSettingsEmailSmtpSslId);

        if (!probe.smtp_ssl) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "SMTP SSL/TLS must be enabled for connection test."
                                   : "Ba\u011flant\u0131 testi i\u00e7in SMTP SSL/TLS etkin olmal\u0131d\u0131r.");
          return 0;
        }

        const std::string smtp_error = g_instance->on_email_smtp_test_(probe);
        if (!smtp_error.empty()) {
          g_instance->SetStatusMessage(smtp_error, true);
        } else {
          g_instance->SetStatusMessage(g_instance->language_ == "en"
                                           ? "SMTP connection is healthy."
                                           : "SMTP bağlantısı başarılı.",
                                       false);
        }
        g_instance->SyncSettingsWindowState();
        return 0;
      }

      if (clicked == kSettingsProfileCreateId) {
        if (!g_instance->on_profile_created_) {
          return 0;
        }

        const std::string input_name = read_profile_input_name();
        if (input_name.empty()) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "Enter a profile name first."
                                   : "Önce bir profil adı girin.");
          return 0;
        }

        const TrayProfileSelectionResult result = g_instance->on_profile_created_(input_name);
        g_instance->ApplyProfileResult(result, {});
        if (result.success) {
          clear_profile_input();
        }
        return 0;
      }

      if (clicked == kSettingsProfileCopyId) {
        if (!g_instance->on_profile_copied_) {
          return 0;
        }

        const std::string selected_profile = read_selected_profile_name();
        const std::string input_name = read_profile_input_name();
        if (selected_profile.empty()) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "Select a profile to copy."
                                   : "Kopyalamak için bir profil seçin.");
          return 0;
        }

        if (input_name.empty()) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "Enter target profile name."
                                   : "Hedef profil adını girin.");
          return 0;
        }

        const TrayProfileSelectionResult result =
            g_instance->on_profile_copied_(selected_profile, input_name);
        g_instance->ApplyProfileResult(result, input_name);
        if (result.success) {
          clear_profile_input();
        }
        return 0;
      }

      if (clicked == kSettingsProfileRenameId) {
        if (!g_instance->on_profile_renamed_) {
          return 0;
        }

        const std::string selected_profile = read_selected_profile_name();
        const std::string input_name = read_profile_input_name();
        if (selected_profile.empty()) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "Select a profile to rename."
                                   : "Yeniden adlandırmak için bir profil seçin.");
          return 0;
        }

        if (input_name.empty()) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "Enter the new profile name."
                                   : "Yeni profil adını girin.");
          return 0;
        }

        const TrayProfileSelectionResult result =
            g_instance->on_profile_renamed_(selected_profile, input_name);
        g_instance->ApplyProfileResult(result, input_name);
        if (result.success) {
          clear_profile_input();
        }
        return 0;
      }

      if (clicked == kSettingsProfileDeleteId) {
        if (!g_instance->on_profile_deleted_) {
          return 0;
        }

        const std::string selected_profile = read_selected_profile_name();
        if (selected_profile.empty()) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "Select a profile to delete."
                                   : "Silmek için bir profil seçin.");
          return 0;
        }

        const bool is_tr = (g_instance->language_ == "tr");
        const std::wstring profile_wide = Utf8ToWide(selected_profile);
        const std::wstring confirm_message =
            (is_tr ? L"\"" + profile_wide + L"\" profili silinsin mi?"
                   : L"Delete profile \"" + profile_wide + L"\"?");
        const int answer = MessageBoxW(
            g_instance->settings_hwnd_, confirm_message.c_str(),
            is_tr ? L"Profili sil" : L"Delete profile",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
        if (answer != IDYES) {
          return 0;
        }

        const TrayProfileSelectionResult result = g_instance->on_profile_deleted_(selected_profile);
        g_instance->ApplyProfileResult(result, {});
        if (result.success) {
          clear_profile_input();
        }
        return 0;
      }

      if (clicked == kSettingsProfileExportId) {
        if (!g_instance->on_profile_exported_) {
          return 0;
        }

        const std::string selected_profile = read_selected_profile_name();
        if (selected_profile.empty()) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "Select a profile to export."
                                   : "Dışa aktarmak için bir profil seçin.");
          return 0;
        }

        const std::string export_path = read_profile_path_input();
        const TrayProfileSelectionResult result =
            g_instance->on_profile_exported_(selected_profile, export_path);
        g_instance->ApplyProfileResult(result, selected_profile);
        return 0;
      }

      if (clicked == kSettingsProfileImportId) {
        if (!g_instance->on_profile_imported_) {
          return 0;
        }

        const std::string import_path = read_profile_path_input();
        const std::string preferred_name = read_profile_input_name();
        const TrayProfileSelectionResult result =
            g_instance->on_profile_imported_(import_path, preferred_name);
        g_instance->ApplyProfileResult(result, preferred_name);
        if (result.success) {
          clear_profile_input();
        }
        return 0;
      }

      if (clicked == kSettingsProfileLockId) {
        if (!g_instance->on_profile_lock_toggled_) {
          return 0;
        }

        const std::string selected_profile = read_selected_profile_name();
        if (selected_profile.empty()) {
          report_profile_error(g_instance->language_ == "en"
                                   ? "Select a profile first."
                                   : "Önce bir profil seçin.");
          return 0;
        }

        const bool should_lock = read_checkbox(kSettingsProfileLockId);
        const TrayProfileSelectionResult result =
            g_instance->on_profile_lock_toggled_(selected_profile, should_lock);
        g_instance->ApplyProfileResult(result, selected_profile);
        return 0;
      }

      if (clicked == kSettingsRestartId) {
        g_instance->HandleMenuCommand(kMenuRestartId);
        return 0;
      }

      if (clicked == kSettingsExitId) {
        g_instance->HandleMenuCommand(kMenuExitId);
        return 0;
      }

      g_instance->HandleMenuCommand(clicked);
      return 0;
    }

    if (msg == WM_CLOSE) {
      DestroyWindow(hwnd);
      return 0;
    }

    if (msg == WM_DESTROY) {
      g_instance->settings_hwnd_ = nullptr;
      return 0;
    }
  }

  if (hwnd == g_instance->hwnd_) {
    if (msg == kShowSettingsUiMessage) {
      g_instance->ShowSettingsUi();
      return 0;
    }

    if (msg == kTrayMessage) {
      if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU ||
          lparam == WM_LBUTTONUP || lparam == WM_LBUTTONDBLCLK) {
        PostMessage(hwnd, kShowSettingsUiMessage, 0, 0);
        return 0;
      }
    }
  }

  return DefWindowProc(hwnd, msg, wparam, lparam);
}

void TrayController::RunLoop() {
  g_instance = this;
  thread_id_ = GetCurrentThreadId();

  const HRESULT com_init_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  const bool com_initialized = SUCCEEDED(com_init_hr) || com_init_hr == S_FALSE;

  if (!CreateWindowAndIcon()) {
    if (com_initialized) {
      CoUninitialize();
    }
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

  if (com_initialized) {
    CoUninitialize();
  }

  g_instance = nullptr;
  thread_id_ = 0;
}

}  // namespace ssext
