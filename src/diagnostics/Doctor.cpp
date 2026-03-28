#include "diagnostics/Doctor.hpp"

#include <cstdlib>
#include <fstream>
#include <string>

#include "PathUtils.hpp"

namespace ssext {

namespace {

bool Exists(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return f.good();
}

}  // namespace

int RunDoctor() {
  int issues = 0;

  const std::string app_data = GetAppDataDirectory();
  if (app_data.empty()) {
    ++issues;
    std::printf("[FAIL] AppData yolu alinamadi\n");
  } else {
    std::printf("[OK] AppData: %s\n", app_data.c_str());
  }

  const std::string exe = GetExecutablePath();
  if (exe.empty()) {
    ++issues;
    std::printf("[FAIL] Exe yolu alinamadi\n");
  } else {
    std::printf("[OK] Exe: %s\n", exe.c_str());
  }

  const char* program_data = std::getenv("PROGRAMDATA");
  if (program_data == nullptr) {
    ++issues;
    std::printf("[FAIL] PROGRAMDATA env yok\n");
  } else {
    const std::string core_props = std::string(program_data) +
                                   "\\SteelSeries\\SteelSeries Engine 3\\coreProps.json";
    if (!Exists(core_props)) {
      ++issues;
      std::printf("[FAIL] coreProps.json bulunamadi: %s\n", core_props.c_str());
    } else {
      std::printf("[OK] coreProps.json bulundu\n");
    }
  }

  if (issues == 0) {
    std::printf("[OK] Doctor tamam, kritik sorun yok\n");
    return 0;
  }

  std::printf("[WARN] Doctor %d sorun buldu\n", issues);
  return 1;
}

}  // namespace ssext
