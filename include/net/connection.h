#pragma once
#include <string>
#include <cstddef>

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
  void reset_send_state() { out.clear(); out_sent = 0; }
};