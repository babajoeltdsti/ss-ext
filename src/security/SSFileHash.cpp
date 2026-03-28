#include "SSFileHash.hpp"

#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace ssext {

bool SSFileHash::Sha256Hex(const std::string& file_path, std::string& out_hex) {
  out_hex.clear();

  std::ifstream in(file_path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }

  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  DWORD obj_len = 0;
  DWORD data = 0;
  DWORD hash_len = 0;
  std::vector<unsigned char> hash_obj;
  std::vector<unsigned char> hash_bytes;

  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
    return false;
  }

  if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&obj_len),
                        sizeof(obj_len), &data, 0) != 0) {
    BCryptCloseAlgorithmProvider(alg, 0);
    return false;
  }

  if (BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_len),
                        sizeof(hash_len), &data, 0) != 0) {
    BCryptCloseAlgorithmProvider(alg, 0);
    return false;
  }

  hash_obj.resize(obj_len);
  hash_bytes.resize(hash_len);

  if (BCryptCreateHash(alg, &hash, hash_obj.data(), obj_len, nullptr, 0, 0) != 0) {
    BCryptCloseAlgorithmProvider(alg, 0);
    return false;
  }

  std::vector<char> buffer(8192);
  while (in) {
    in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto got = static_cast<std::size_t>(in.gcount());
    if (got == 0) {
      break;
    }
    if (BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(got), 0) != 0) {
      BCryptDestroyHash(hash);
      BCryptCloseAlgorithmProvider(alg, 0);
      return false;
    }
  }

  if (BCryptFinishHash(hash, hash_bytes.data(), static_cast<ULONG>(hash_bytes.size()), 0) != 0) {
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    return false;
  }

  BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(alg, 0);

  std::ostringstream oss;
  oss.setf(std::ios::hex, std::ios::basefield);
  for (unsigned char b : hash_bytes) {
    const char* hex = "0123456789abcdef";
    oss << hex[(b >> 4) & 0xF] << hex[b & 0xF];
  }

  out_hex = oss.str();
  return true;
}

bool SSFileHash::ParseSha256Text(const std::string& text, std::string& out_hex) {
  out_hex.clear();
  std::string token;

  for (char c : text) {
    if (std::isxdigit(static_cast<unsigned char>(c)) != 0) {
      token.push_back(static_cast<char>(std::tolower(c)));
      if (token.size() == 64) {
        out_hex = token;
        return true;
      }
    } else {
      token.clear();
    }
  }

  return false;
}

}  // namespace ssext
