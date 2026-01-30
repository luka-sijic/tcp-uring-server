#pragma once

#include <liburing.h>
#include <unordered_map>

#include "core/helpers.h"
#include "core/router.hpp"
#include "net/connection.h"

class UringDriver {
public:
  UringDriver(Router *r, int fd);
  ~UringDriver();

  bool submit_recv(uint32_t slot);
  bool submit_send(uint32_t slot);
  bool submit_close(int fd);

  void recv(uint32_t slot, int res);
  void send(uint32_t slot, int res);

  void run();

private:
  io_uring ring_{};
  int fd_{-1};
  Router router_;
  static constexpr int kUdpSlots = 2;
  UdpState udp_[kUdpSlots];
};
