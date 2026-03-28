#include <Windows.h>

#include <atomic>
#include <string>

#include "App.hpp"
#include "diagnostics/CrashHandler.hpp"
#include "diagnostics/Doctor.hpp"
#include "security/CredentialStore.hpp"
#include "Logger.hpp"
#include "PathUtils.hpp"

namespace {
std::atomic_bool g_stop_requested{false};
ssext::App* g_app = nullptr;

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrl_type) {
  switch (ctrl_type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
      g_stop_requested.store(true);
      if (g_app != nullptr) {
        g_app->RequestStop();
      }
      return TRUE;
    default:
      return FALSE;
  }
}

}  // namespace

int RunApplication() {
  const std::string app_data_dir = ssext::GetAppDataDirectory();
  if (!app_data_dir.empty()) {
    ssext::CrashHandler::Initialize(ssext::JoinPath(app_data_dir, "dumps"));
  }

  ssext::CredentialStore credential_store;

  if (__argc > 1) {
    const std::string arg1 = __argv[1];
    if (arg1 == "--doctor") {
      return ssext::RunDoctor();
    }

    if (arg1 == "--set-email-credential") {
      if (__argc < 4) {
        ssext::Logger::Instance().Log(ssext::LogLevel::Error,
                                      "Kullanim: --set-email-credential <username> <password>");
        return 1;
      }

      const std::string username = __argv[2];
      const std::string password = __argv[3];
      if (!credential_store.SaveGeneric("SSEXT_CPP_IMAP", username, password)) {
        ssext::Logger::Instance().Log(ssext::LogLevel::Error,
                                      "Credential kaydedilemedi.");
        return 1;
      }

      ssext::Logger::Instance().Log(ssext::LogLevel::Info,
                                    "Email credential kaydedildi.");
      return 0;
    }

    if (arg1 == "--clear-email-credential") {
      credential_store.DeleteGeneric("SSEXT_CPP_IMAP");
      ssext::Logger::Instance().Log(ssext::LogLevel::Info,
                                    "Email credential silindi.");
      return 0;
    }
  }

  const HANDLE instance_mutex = CreateMutexA(nullptr, FALSE, "Local\\SSEXT_CPP_INSTANCE_MUTEX");
  const bool already_running = (GetLastError() == ERROR_ALREADY_EXISTS);

  HANDLE stop_event = OpenEventA(EVENT_MODIFY_STATE, FALSE, "Local\\SSEXT_CPP_STOP_EVENT");

  if (__argc > 1) {
    const std::string arg1 = __argv[1];
    if (arg1 == "--stop") {
      if (stop_event != nullptr) {
        SetEvent(stop_event);
        CloseHandle(stop_event);
      }
      if (instance_mutex != nullptr) {
        CloseHandle(instance_mutex);
      }
      return 0;
    }
  }

  if (already_running) {
    ssext::Logger::Instance().Log(ssext::LogLevel::Warning,
                                  "Uygulama zaten calisiyor. --stop ile kapatabilirsin.");
    if (stop_event != nullptr) {
      CloseHandle(stop_event);
    }
    if (instance_mutex != nullptr) {
      CloseHandle(instance_mutex);
    }
    return 0;
  }

  if (stop_event == nullptr) {
    stop_event = CreateEventA(nullptr, TRUE, FALSE, "Local\\SSEXT_CPP_STOP_EVENT");
  }

  if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE)) {
    ssext::Logger::Instance().Log(ssext::LogLevel::Warning,
                                  "ConsoleCtrlHandler ayarlanamadi.");
  }

  ssext::App app(stop_event);
  g_app = &app;

  if (!app.Initialize()) {
    ssext::Logger::Instance().Log(ssext::LogLevel::Error,
                                  "Initialize basarisiz, cikiliyor.");
    return 1;
  }

  app.Run();

  ssext::Logger::Instance().Log(ssext::LogLevel::Info, "Uygulama kapandi.");

  if (stop_event != nullptr) {
    CloseHandle(stop_event);
  }
  if (instance_mutex != nullptr) {
    CloseHandle(instance_mutex);
  }
  return 0;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  return RunApplication();
}
