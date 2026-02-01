#pragma once
#include <array>
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

struct SendState {
  static constexpr size_t kMax = 2048;

  bool busy = false;

  sockaddr_storage dst{};
  socklen_t dst_len = 0;

  iovec iov{};
  msghdr msg{};

  alignas(16) std::array<std::byte, kMax> buf{};
  size_t len = 0;
};
