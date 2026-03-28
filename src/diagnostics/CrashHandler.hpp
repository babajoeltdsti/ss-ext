#pragma once

#include <string>

namespace ssext {

class CrashHandler {
 public:
  static void Initialize(const std::string& dump_directory);

 private:
  static long __stdcall UnhandledExceptionFilterFn(struct _EXCEPTION_POINTERS* exception_info);
  static std::string dump_directory_;
};

}  // namespace ssext
