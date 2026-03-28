#include "security/CredentialStore.hpp"

#include <Windows.h>
#include <wincred.h>

#include <string>

#pragma comment(lib, "Advapi32.lib")

namespace ssext {

namespace {

std::wstring ToWide(const std::string& text) {
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

std::string ToUtf8(const std::wstring& text) {
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

}  // namespace

bool CredentialStore::SaveGeneric(const std::string& target, const std::string& username,
                                  const std::string& secret) const {
  const std::wstring wtarget = ToWide(target);
  const std::wstring wuser = ToWide(username);

  CREDENTIALW cred{};
  cred.Type = CRED_TYPE_GENERIC;
  cred.TargetName = const_cast<LPWSTR>(wtarget.c_str());
  cred.UserName = const_cast<LPWSTR>(wuser.c_str());
  cred.CredentialBlobSize = static_cast<DWORD>(secret.size());
  cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(secret.data()));
  cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

  return CredWriteW(&cred, 0) == TRUE;
}

bool CredentialStore::ReadGeneric(const std::string& target, std::string& username,
                                  std::string& secret) const {
  const std::wstring wtarget = ToWide(target);
  PCREDENTIALW cred = nullptr;
  if (CredReadW(wtarget.c_str(), CRED_TYPE_GENERIC, 0, &cred) != TRUE || cred == nullptr) {
    return false;
  }

  username = cred->UserName ? ToUtf8(cred->UserName) : std::string();
  secret.assign(reinterpret_cast<const char*>(cred->CredentialBlob),
                static_cast<std::size_t>(cred->CredentialBlobSize));
  CredFree(cred);
  return true;
}

bool CredentialStore::DeleteGeneric(const std::string& target) const {
  const std::wstring wtarget = ToWide(target);
  return CredDeleteW(wtarget.c_str(), CRED_TYPE_GENERIC, 0) == TRUE;
}

}  // namespace ssext
