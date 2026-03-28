#include "diagnostics/CrashHandler.hpp"

#include <Windows.h>
#include <DbgHelp.h>

#include <string>

#include "PathUtils.hpp"

#pragma comment(lib, "Dbghelp.lib")

namespace ssext {

std::string CrashHandler::dump_directory_;

void CrashHandler::Initialize(const std::string& dump_directory) {
  dump_directory_ = dump_directory;
  if (!dump_directory_.empty()) {
    EnsureDirectory(dump_directory_);
  }
  SetUnhandledExceptionFilter(CrashHandler::UnhandledExceptionFilterFn);
}

long __stdcall CrashHandler::UnhandledExceptionFilterFn(_EXCEPTION_POINTERS* exception_info) {
  if (dump_directory_.empty()) {
    return EXCEPTION_EXECUTE_HANDLER;
  }

  SYSTEMTIME st{};
  GetLocalTime(&st);

  char file_name[128] = {0};
  wsprintfA(file_name, "crash-%04u%02u%02u-%02u%02u%02u.dmp", st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond);

  const std::string dump_path = JoinPath(dump_directory_, file_name);
  HANDLE file = CreateFileA(dump_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return EXCEPTION_EXECUTE_HANDLER;
  }

  MINIDUMP_EXCEPTION_INFORMATION mei{};
  mei.ThreadId = GetCurrentThreadId();
  mei.ExceptionPointers = exception_info;
  mei.ClientPointers = FALSE;

  MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal,
                    exception_info ? &mei : nullptr, nullptr, nullptr);

  CloseHandle(file);
  return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace ssext
