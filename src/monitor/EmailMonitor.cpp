#include "monitor/EmailMonitor.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif

#include <security.h>
#include <schannel.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Secur32.lib")

namespace {

class TlsSocket {
 public:
  ~TlsSocket() {
    Close();
  }

  bool Connect(const std::string& host, int port);
  bool SendLine(const std::string& line);
  bool ReadLine(std::string& out_line);
  void Close();

 private:
  bool Handshake();
  bool RecvMore();
  bool DecryptMore();
  bool SendAllRaw(const char* data, std::size_t size);

  std::string host_;
  SOCKET sock_ = INVALID_SOCKET;
  CredHandle cred_{};
  CtxtHandle ctx_{};
  bool cred_valid_ = false;
  bool ctx_valid_ = false;
  SecPkgContext_StreamSizes sizes_{};
  std::vector<char> encrypted_buffer_;
  std::vector<char> plaintext_buffer_;
};

bool TlsSocket::SendAllRaw(const char* data, std::size_t size) {
  std::size_t sent_total = 0;
  while (sent_total < size) {
    const int sent = send(sock_, data + sent_total, static_cast<int>(size - sent_total), 0);
    if (sent <= 0) {
      return false;
    }
    sent_total += static_cast<std::size_t>(sent);
  }
  return true;
}

bool TlsSocket::Connect(const std::string& host, int port) {
  host_ = host;

  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    return false;
  }

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* result = nullptr;
  const std::string port_text = std::to_string(port);
  if (getaddrinfo(host.c_str(), port_text.c_str(), &hints, &result) != 0 || result == nullptr) {
    WSACleanup();
    return false;
  }

  for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
    sock_ = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (sock_ == INVALID_SOCKET) {
      continue;
    }

    if (connect(sock_, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
      break;
    }

    closesocket(sock_);
    sock_ = INVALID_SOCKET;
  }

  freeaddrinfo(result);

  if (sock_ == INVALID_SOCKET) {
    WSACleanup();
    return false;
  }

  return Handshake();
}

bool TlsSocket::Handshake() {
  SCHANNEL_CRED schannel_cred{};
  schannel_cred.dwVersion = SCHANNEL_CRED_VERSION;
  schannel_cred.dwFlags = SCH_USE_STRONG_CRYPTO | SCH_CRED_AUTO_CRED_VALIDATION;
  schannel_cred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT | SP_PROT_TLS1_3_CLIENT;

  TimeStamp expiry{};
  SECURITY_STATUS status = AcquireCredentialsHandleA(
      nullptr, const_cast<SEC_CHAR*>(UNISP_NAME_A), SECPKG_CRED_OUTBOUND,
      nullptr, &schannel_cred, nullptr, nullptr, &cred_, &expiry);
  if (status != SEC_E_OK) {
    return false;
  }
  cred_valid_ = true;

  DWORD context_flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT |
                        ISC_REQ_CONFIDENTIALITY | ISC_REQ_ALLOCATE_MEMORY |
                        ISC_REQ_STREAM;
  DWORD out_flags = 0;

  SecBuffer out_buffers[1]{};
  out_buffers[0].BufferType = SECBUFFER_TOKEN;
  SecBufferDesc out_desc{};
  out_desc.ulVersion = SECBUFFER_VERSION;
  out_desc.cBuffers = 1;
  out_desc.pBuffers = out_buffers;

  status = InitializeSecurityContextA(&cred_, nullptr, const_cast<SEC_CHAR*>(host_.c_str()),
                                      context_flags, 0, SECURITY_NATIVE_DREP, nullptr, 0,
                                      &ctx_, &out_desc, &out_flags, &expiry);
  if (status != SEC_I_CONTINUE_NEEDED && status != SEC_E_OK) {
    return false;
  }
  ctx_valid_ = true;

  if (out_buffers[0].pvBuffer != nullptr && out_buffers[0].cbBuffer > 0) {
    if (!SendAllRaw(reinterpret_cast<const char*>(out_buffers[0].pvBuffer), out_buffers[0].cbBuffer)) {
      FreeContextBuffer(out_buffers[0].pvBuffer);
      return false;
    }
    FreeContextBuffer(out_buffers[0].pvBuffer);
  }

  std::vector<char> in_token;
  in_token.reserve(64 * 1024);

  while (status == SEC_I_CONTINUE_NEEDED || status == SEC_E_INCOMPLETE_MESSAGE) {
    char recv_buf[8192];
    const int n = recv(sock_, recv_buf, static_cast<int>(sizeof(recv_buf)), 0);
    if (n <= 0) {
      return false;
    }
    in_token.insert(in_token.end(), recv_buf, recv_buf + n);

    SecBuffer in_buffers[2]{};
    in_buffers[0].BufferType = SECBUFFER_TOKEN;
    in_buffers[0].pvBuffer = in_token.data();
    in_buffers[0].cbBuffer = static_cast<unsigned long>(in_token.size());
    in_buffers[1].BufferType = SECBUFFER_EMPTY;

    SecBufferDesc in_desc{};
    in_desc.ulVersion = SECBUFFER_VERSION;
    in_desc.cBuffers = 2;
    in_desc.pBuffers = in_buffers;

    SecBuffer out2[1]{};
    out2[0].BufferType = SECBUFFER_TOKEN;
    SecBufferDesc out2_desc{};
    out2_desc.ulVersion = SECBUFFER_VERSION;
    out2_desc.cBuffers = 1;
    out2_desc.pBuffers = out2;

    status = InitializeSecurityContextA(&cred_, &ctx_, const_cast<SEC_CHAR*>(host_.c_str()),
                                        context_flags, 0, SECURITY_NATIVE_DREP,
                                        &in_desc, 0, nullptr, &out2_desc, &out_flags, &expiry);

    if (out2[0].pvBuffer != nullptr && out2[0].cbBuffer > 0) {
      if (!SendAllRaw(reinterpret_cast<const char*>(out2[0].pvBuffer), out2[0].cbBuffer)) {
        FreeContextBuffer(out2[0].pvBuffer);
        return false;
      }
      FreeContextBuffer(out2[0].pvBuffer);
    }

    if (status == SEC_E_OK) {
      if (in_buffers[1].BufferType == SECBUFFER_EXTRA && in_buffers[1].cbBuffer > 0) {
        const auto extra = in_buffers[1].cbBuffer;
        const auto total = in_token.size();
        encrypted_buffer_.insert(encrypted_buffer_.end(), in_token.data() + (total - extra),
                                 in_token.data() + total);
      }
      break;
    }

    if (status == SEC_E_INCOMPLETE_MESSAGE) {
      continue;
    }

    if (status != SEC_I_CONTINUE_NEEDED) {
      return false;
    }

    if (in_buffers[1].BufferType == SECBUFFER_EXTRA && in_buffers[1].cbBuffer > 0) {
      const auto extra = in_buffers[1].cbBuffer;
      const auto total = in_token.size();
      std::vector<char> next(in_token.data() + (total - extra), in_token.data() + total);
      in_token.swap(next);
    } else {
      in_token.clear();
    }
  }

  if (status != SEC_E_OK) {
    return false;
  }

  status = QueryContextAttributesA(&ctx_, SECPKG_ATTR_STREAM_SIZES, &sizes_);
  return status == SEC_E_OK;
}

bool TlsSocket::RecvMore() {
  char recv_buf[8192];
  const int n = recv(sock_, recv_buf, static_cast<int>(sizeof(recv_buf)), 0);
  if (n <= 0) {
    return false;
  }
  encrypted_buffer_.insert(encrypted_buffer_.end(), recv_buf, recv_buf + n);
  return true;
}

bool TlsSocket::DecryptMore() {
  if (encrypted_buffer_.empty()) {
    return RecvMore();
  }

  SecBuffer buffers[4]{};
  buffers[0].BufferType = SECBUFFER_DATA;
  buffers[0].pvBuffer = encrypted_buffer_.data();
  buffers[0].cbBuffer = static_cast<unsigned long>(encrypted_buffer_.size());
  buffers[1].BufferType = SECBUFFER_EMPTY;
  buffers[2].BufferType = SECBUFFER_EMPTY;
  buffers[3].BufferType = SECBUFFER_EMPTY;

  SecBufferDesc desc{};
  desc.ulVersion = SECBUFFER_VERSION;
  desc.cBuffers = 4;
  desc.pBuffers = buffers;

  SECURITY_STATUS status = DecryptMessage(&ctx_, &desc, 0, nullptr);
  if (status == SEC_E_INCOMPLETE_MESSAGE) {
    return RecvMore();
  }
  if (status == SEC_I_CONTEXT_EXPIRED) {
    return false;
  }
  if (status != SEC_E_OK && status != SEC_I_RENEGOTIATE) {
    return false;
  }

  for (auto& b : buffers) {
    if (b.BufferType == SECBUFFER_DATA && b.cbBuffer > 0 && b.pvBuffer != nullptr) {
      const auto* p = reinterpret_cast<const char*>(b.pvBuffer);
      plaintext_buffer_.insert(plaintext_buffer_.end(), p, p + b.cbBuffer);
    }
  }

  std::vector<char> next_encrypted;
  for (auto& b : buffers) {
    if (b.BufferType == SECBUFFER_EXTRA && b.cbBuffer > 0 && b.pvBuffer != nullptr) {
      const auto* p = reinterpret_cast<const char*>(b.pvBuffer);
      next_encrypted.assign(p, p + b.cbBuffer);
      break;
    }
  }
  encrypted_buffer_.swap(next_encrypted);

  return true;
}

bool TlsSocket::ReadLine(std::string& out_line) {
  out_line.clear();
  while (true) {
    auto it = std::find(plaintext_buffer_.begin(), plaintext_buffer_.end(), '\n');
    if (it != plaintext_buffer_.end()) {
      const auto end = static_cast<std::size_t>(std::distance(plaintext_buffer_.begin(), it));
      out_line.assign(plaintext_buffer_.begin(), plaintext_buffer_.begin() + end + 1);
      plaintext_buffer_.erase(plaintext_buffer_.begin(), plaintext_buffer_.begin() + end + 1);
      return true;
    }

    if (!DecryptMore()) {
      return false;
    }
  }
}

bool TlsSocket::SendLine(const std::string& line) {
  if (!ctx_valid_) {
    return false;
  }

  std::size_t offset = 0;
  while (offset < line.size()) {
    const std::size_t chunk = std::min<std::size_t>(line.size() - offset, sizes_.cbMaximumMessage);
    std::vector<char> packet(sizes_.cbHeader + chunk + sizes_.cbTrailer);

    std::memcpy(packet.data() + sizes_.cbHeader, line.data() + offset, chunk);

    SecBuffer buffers[4]{};
    buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
    buffers[0].pvBuffer = packet.data();
    buffers[0].cbBuffer = sizes_.cbHeader;
    buffers[1].BufferType = SECBUFFER_DATA;
    buffers[1].pvBuffer = packet.data() + sizes_.cbHeader;
    buffers[1].cbBuffer = static_cast<unsigned long>(chunk);
    buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
    buffers[2].pvBuffer = packet.data() + sizes_.cbHeader + chunk;
    buffers[2].cbBuffer = sizes_.cbTrailer;
    buffers[3].BufferType = SECBUFFER_EMPTY;

    SecBufferDesc desc{};
    desc.ulVersion = SECBUFFER_VERSION;
    desc.cBuffers = 4;
    desc.pBuffers = buffers;

    SECURITY_STATUS status = EncryptMessage(&ctx_, 0, &desc, 0);
    if (status != SEC_E_OK) {
      return false;
    }

    const std::size_t to_send = buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer;
    if (!SendAllRaw(packet.data(), to_send)) {
      return false;
    }

    offset += chunk;
  }

  return true;
}

void TlsSocket::Close() {
  if (ctx_valid_) {
    DeleteSecurityContext(&ctx_);
    ctx_valid_ = false;
  }
  if (cred_valid_) {
    FreeCredentialHandle(&cred_);
    cred_valid_ = false;
  }
  if (sock_ != INVALID_SOCKET) {
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
  }
  WSACleanup();
}

}  // namespace

namespace ssext {

namespace {

std::string Trim(const std::string& text) {
  auto begin = text.begin();
  while (begin != text.end() && std::isspace(static_cast<unsigned char>(*begin)) != 0) {
    ++begin;
  }

  auto end = text.end();
  while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1))) != 0) {
    --end;
  }

  return std::string(begin, end);
}

bool SendAll(SOCKET sock, const std::string& data) {
  std::size_t sent_total = 0;
  while (sent_total < data.size()) {
    const int sent = send(sock, data.data() + sent_total,
                          static_cast<int>(data.size() - sent_total), 0);
    if (sent <= 0) {
      return false;
    }
    sent_total += static_cast<std::size_t>(sent);
  }
  return true;
}

template <typename ReadLineFn>
bool ReadUntilTaggedGeneric(ReadLineFn&& read_line, const std::string& tag,
                            std::vector<std::string>& lines) {
  lines.clear();
  std::string line;
  while (read_line(line)) {
    lines.push_back(line);
    if (line.rfind(tag + " ", 0) == 0) {
      return true;
    }
  }
  return false;
}

bool ReadLine(SOCKET sock, std::string& out) {
  out.clear();
  char ch = 0;
  while (true) {
    const int n = recv(sock, &ch, 1, 0);
    if (n <= 0) {
      return false;
    }
    out.push_back(ch);
    if (out.size() >= 2 && out[out.size() - 2] == '\r' && out[out.size() - 1] == '\n') {
      return true;
    }
  }
}

bool TaggedOk(const std::vector<std::string>& lines, const std::string& tag) {
  for (const auto& line : lines) {
    if (line.rfind(tag + " OK", 0) == 0) {
      return true;
    }
  }
  return false;
}

std::uint64_t ParseLatestSearchId(const std::vector<std::string>& lines) {
  std::uint64_t latest = 0;
  for (const auto& line : lines) {
    if (line.rfind("* SEARCH", 0) != 0) {
      continue;
    }
    std::stringstream ss(line.substr(8));
    std::uint64_t id = 0;
    while (ss >> id) {
      if (id > latest) {
        latest = id;
      }
    }
  }
  return latest;
}

std::string ExtractHeaderValue(const std::vector<std::string>& lines, const std::string& key) {
  const std::string prefix = key + ":";
  for (const auto& line : lines) {
    if (line.size() >= prefix.size() &&
        std::equal(prefix.begin(), prefix.end(), line.begin(),
                   [](char a, char b) { return std::tolower(a) == std::tolower(b); })) {
      return Trim(line.substr(prefix.size()));
    }
  }
  return {};
}

}  // namespace

EmailMonitor::EmailMonitor(const Config& config) : config_(config) {}

bool EmailMonitor::TestSmtpConnection(const std::string& server,
                                      int port,
                                      bool use_ssl,
                                      std::string& error_message) {
  error_message.clear();

  if (server.empty()) {
    error_message = "SMTP sunucu bos.";
    return false;
  }

  if (port <= 0 || port > 65535) {
    error_message = "SMTP port gecersiz.";
    return false;
  }

  auto read_smtp_response = [&](auto&& read_line_fn, const std::string& expected_prefix) {
    std::string line;
    while (read_line_fn(line)) {
      const std::string trimmed = Trim(line);
      if (trimmed.size() < 4) {
        continue;
      }

      if (trimmed.rfind(expected_prefix, 0) != 0) {
        error_message = "SMTP cevap beklenenden farkli: " + trimmed;
        return false;
      }

      if (trimmed[3] == ' ') {
        return true;
      }
    }

    error_message = "SMTP cevap okunamadi.";
    return false;
  };

  if (use_ssl) {
    TlsSocket tls;
    if (!tls.Connect(server, port)) {
      error_message = "SMTP SSL baglantisi kurulamadi.";
      return false;
    }

    if (!read_smtp_response([&](std::string& line) { return tls.ReadLine(line); }, "220")) {
      return false;
    }

    if (!tls.SendLine("EHLO carex-ext.local\r\n")) {
      error_message = "SMTP EHLO komutu gonderilemedi.";
      return false;
    }

    if (!read_smtp_response([&](std::string& line) { return tls.ReadLine(line); }, "250")) {
      return false;
    }

    tls.SendLine("QUIT\r\n");
    return true;
  }

  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    error_message = "Winsock baslatilamadi.";
    return false;
  }

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* result = nullptr;
  const std::string port_text = std::to_string(port);
  if (getaddrinfo(server.c_str(), port_text.c_str(), &hints, &result) != 0 || result == nullptr) {
    WSACleanup();
    error_message = "SMTP DNS cozumleme basarisiz.";
    return false;
  }

  SOCKET sock = INVALID_SOCKET;
  for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
    sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (sock == INVALID_SOCKET) {
      continue;
    }

    if (connect(sock, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
      break;
    }

    closesocket(sock);
    sock = INVALID_SOCKET;
  }

  freeaddrinfo(result);

  if (sock == INVALID_SOCKET) {
    WSACleanup();
    error_message = "SMTP soket baglantisi kurulamadi.";
    return false;
  }

  const bool welcome_ok = read_smtp_response(
      [&](std::string& line) { return ReadLine(sock, line); }, "220");
  if (!welcome_ok) {
    closesocket(sock);
    WSACleanup();
    return false;
  }

  if (!SendAll(sock, "EHLO carex-ext.local\r\n")) {
    closesocket(sock);
    WSACleanup();
    error_message = "SMTP EHLO komutu gonderilemedi.";
    return false;
  }

  const bool ehlo_ok = read_smtp_response(
      [&](std::string& line) { return ReadLine(sock, line); }, "250");
  SendAll(sock, "QUIT\r\n");
  closesocket(sock);
  WSACleanup();
  return ehlo_ok;
}

void EmailMonitor::UpdateConfig(const Config& config) {
  config_ = config;
  next_poll_ = std::chrono::steady_clock::now();
}

void EmailMonitor::SetCredential(std::string username, std::string password) {
  username_ = std::move(username);
  password_ = std::move(password);
}

EmailItem EmailMonitor::PollImapPlain() {
  EmailItem item;

  if (config_.email_imap_server.empty() || username_.empty() || password_.empty()) {
    return item;
  }

  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    return item;
  }

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* result = nullptr;
  const std::string port = std::to_string(config_.email_imap_port);
  if (getaddrinfo(config_.email_imap_server.c_str(), port.c_str(), &hints, &result) != 0 ||
      result == nullptr) {
    WSACleanup();
    return item;
  }

  SOCKET sock = INVALID_SOCKET;
  for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
    sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (sock == INVALID_SOCKET) {
      continue;
    }
    if (connect(sock, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
      break;
    }
    closesocket(sock);
    sock = INVALID_SOCKET;
  }
  freeaddrinfo(result);

  if (sock == INVALID_SOCKET) {
    WSACleanup();
    return item;
  }

  std::vector<std::string> lines;
  std::string greeting;
  if (!ReadLine(sock, greeting)) {
    closesocket(sock);
    WSACleanup();
    return item;
  }

  const std::string login_cmd = "a1 LOGIN \"" + username_ + "\" \"" + password_ + "\"\r\n";
  if (!SendAll(sock, login_cmd) ||
      !ReadUntilTaggedGeneric([&](std::string& l) { return ReadLine(sock, l); }, "a1", lines) ||
      !TaggedOk(lines, "a1")) {
    closesocket(sock);
    WSACleanup();
    return item;
  }

  if (!SendAll(sock, "a2 SELECT INBOX\r\n") ||
      !ReadUntilTaggedGeneric([&](std::string& l) { return ReadLine(sock, l); }, "a2", lines) ||
      !TaggedOk(lines, "a2")) {
    closesocket(sock);
    WSACleanup();
    return item;
  }

  if (!SendAll(sock, "a3 UID SEARCH UNSEEN\r\n") ||
      !ReadUntilTaggedGeneric([&](std::string& l) { return ReadLine(sock, l); }, "a3", lines) ||
      !TaggedOk(lines, "a3")) {
    closesocket(sock);
    WSACleanup();
    return item;
  }

  const std::uint64_t latest_uid = ParseLatestSearchId(lines);
  if (latest_uid > 0 && latest_uid > last_seen_uid_) {
    const std::string fetch_cmd =
        "a4 UID FETCH " + std::to_string(latest_uid) +
        " BODY.PEEK[HEADER.FIELDS (SUBJECT FROM)]\r\n";
    if (SendAll(sock, fetch_cmd) &&
      ReadUntilTaggedGeneric([&](std::string& l) { return ReadLine(sock, l); }, "a4", lines) &&
      TaggedOk(lines, "a4")) {
      item.has_value = true;
      item.subject = ExtractHeaderValue(lines, "Subject");
      item.sender = ExtractHeaderValue(lines, "From");
      if (item.subject.empty()) {
        item.subject = "Yeni E-posta";
      }
      if (item.sender.empty()) {
        item.sender = "Bilinmeyen";
      }
    }
    last_seen_uid_ = latest_uid;
  }

  SendAll(sock, "a9 LOGOUT\r\n");
  closesocket(sock);
  WSACleanup();
  return item;
}

EmailItem EmailMonitor::PollImapSsl() {
  EmailItem item;
  if (config_.email_imap_server.empty() || username_.empty() || password_.empty()) {
    return item;
  }

  TlsSocket tls;
  if (!tls.Connect(config_.email_imap_server, config_.email_imap_port)) {
    return item;
  }

  std::vector<std::string> lines;
  std::string greeting;
  if (!tls.ReadLine(greeting)) {
    return item;
  }

  const std::string login_cmd = "a1 LOGIN \"" + username_ + "\" \"" + password_ + "\"\r\n";
  if (!tls.SendLine(login_cmd) ||
      !ReadUntilTaggedGeneric([&](std::string& l) { return tls.ReadLine(l); }, "a1", lines) ||
      !TaggedOk(lines, "a1")) {
    return item;
  }

  if (!tls.SendLine("a2 SELECT INBOX\r\n") ||
      !ReadUntilTaggedGeneric([&](std::string& l) { return tls.ReadLine(l); }, "a2", lines) ||
      !TaggedOk(lines, "a2")) {
    return item;
  }

  if (!tls.SendLine("a3 UID SEARCH UNSEEN\r\n") ||
      !ReadUntilTaggedGeneric([&](std::string& l) { return tls.ReadLine(l); }, "a3", lines) ||
      !TaggedOk(lines, "a3")) {
    return item;
  }

  const std::uint64_t latest_uid = ParseLatestSearchId(lines);
  if (latest_uid > 0 && latest_uid > last_seen_uid_) {
    const std::string fetch_cmd =
        "a4 UID FETCH " + std::to_string(latest_uid) +
        " BODY.PEEK[HEADER.FIELDS (SUBJECT FROM)]\r\n";
    if (tls.SendLine(fetch_cmd) &&
        ReadUntilTaggedGeneric([&](std::string& l) { return tls.ReadLine(l); }, "a4", lines) &&
        TaggedOk(lines, "a4")) {
      item.has_value = true;
      item.subject = ExtractHeaderValue(lines, "Subject");
      item.sender = ExtractHeaderValue(lines, "From");
      if (item.subject.empty()) {
        item.subject = "Yeni E-posta";
      }
      if (item.sender.empty()) {
        item.sender = "Bilinmeyen";
      }
    }
    last_seen_uid_ = latest_uid;
  }

  tls.SendLine("a9 LOGOUT\r\n");
  return item;
}

EmailItem EmailMonitor::Poll() {
  EmailItem item;
  if (!config_.email_enabled) {
    return item;
  }

  const auto now = std::chrono::steady_clock::now();
  if (next_poll_ == std::chrono::steady_clock::time_point{}) {
    next_poll_ = now;
  }

  if (now < next_poll_) {
    return item;
  }

  next_poll_ = now + std::chrono::seconds(config_.email_check_interval_seconds);

  if (config_.email_imap_ssl) {
    item = PollImapSsl();
    if (item.has_value) {
      return item;
    }

    // Common server setup: port 143 is plain IMAP without implicit TLS.
    if (config_.email_imap_port == 143) {
      item = PollImapPlain();
      if (item.has_value) {
        return item;
      }
    }
  } else {
    item = PollImapPlain();
    if (item.has_value) {
      return item;
    }

    // Common server setup: port 993 expects implicit TLS.
    if (config_.email_imap_port == 993) {
      item = PollImapSsl();
      if (item.has_value) {
        return item;
      }
    }
  }

  // Pipeline dogrulamasi icin tek seferlik test bildirimi desteklenir.
  if (!config_.email_test_subject.empty() && !test_emitted_) {
    item.has_value = true;
    item.subject = config_.email_test_subject;
    item.sender = config_.email_test_sender.empty() ? "Email" : config_.email_test_sender;
    test_emitted_ = true;
  }

  return item;
}

}  // namespace ssext
