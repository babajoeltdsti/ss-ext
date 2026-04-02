cmake_minimum_required(VERSION 3.20)
project(ss_ext_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive-)
  add_definitions(-DUNICODE -D_UNICODE)
endif()

add_executable(ss-ext WIN32
  resources/carex-ext.rc
  src/main.cpp
  src/App.cpp
  src/Config.cpp
  src/I18n.cpp
  src/Logger.cpp
  src/GameSenseClient.cpp
  src/PathUtils.cpp
  src/diagnostics/CrashHandler.cpp
  src/diagnostics/Doctor.cpp
  src/monitor/VolumeMonitor.cpp
  src/monitor/SpotifyMonitor.cpp
  src/monitor/NotificationMonitor.cpp
  src/monitor/GameMonitor.cpp
  src/monitor/TemperatureMonitor.cpp
  src/monitor/EmailMonitor.cpp
  src/update/UpdateChecker.cpp
  src/ui/TrayController.cpp
  src/startup/ProcessLauncher.cpp
  src/startup/StartupManager.cpp
  src/security/CredentialStore.cpp
  src/security/SSFileHash.cpp
)

target_include_directories(ss-ext PRIVATE src)

set(WEBVIEW2_SDK_DIR "${CMAKE_SOURCE_DIR}/packages/Microsoft.Web.WebView2/build/native")
if(NOT EXISTS "${WEBVIEW2_SDK_DIR}/include/WebView2.h" AND DEFINED ENV{WEBVIEW2_SDK_DIR})
  set(WEBVIEW2_SDK_DIR "$ENV{WEBVIEW2_SDK_DIR}")
endif()

if(EXISTS "${WEBVIEW2_SDK_DIR}/include/WebView2.h")
  target_include_directories(ss-ext PRIVATE "${WEBVIEW2_SDK_DIR}/include")

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WEBVIEW2_LIB_DIR "${WEBVIEW2_SDK_DIR}/x64")
  else()
    set(WEBVIEW2_LIB_DIR "${WEBVIEW2_SDK_DIR}/x86")
  endif()

  target_link_directories(ss-ext PRIVATE "${WEBVIEW2_LIB_DIR}")
  target_link_libraries(ss-ext PRIVATE WebView2LoaderStatic)
else()
  message(FATAL_ERROR "WebView2 SDK not found. Install Microsoft.Web.WebView2 package under ./packages.")
endif()

target_link_libraries(ss-ext PRIVATE
  winhttp
  shell32
  ole32
  oleaut32
  wbemuuid
  winmm
  ws2_32
  secur32
  crypt32
  bcrypt
  windowsapp
)
