#pragma once
#include <cstddef>
#include <netinet/in.h>
#include <string>

struct UdpState {
  static constexpr size_t kBufSize = 2048;
  alignas(16) char buf[kBufSize];

  sockaddr_storage peer{};
  socklen_t peer_len = sizeof(peer);
  sockaddr_storage send_peer{};
  socklen_t send_peer_len = 0;

  iovec iov{};
  msghdr msg{};

  iovec siov{};
  msghdr smsg{};

  std::string out;
};

struct Conn {
  int fd = -1;

  // Input accumulation for newline-delimited protocol
  std::string in;

  // Output buffer for partial sends
  std::string out;
  std::size_t out_sent = 0;

  // Temporary recv buffer used by io_uring recv
  static constexpr std::size_t kBufSize = 4096;
  char buf[kBufSize];

  bool has_pending_send() const { return !out.empty(); }
  void reset_send_state() {
    out.clear();
    out_sent = 0;
  }
};
