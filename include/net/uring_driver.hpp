#pragma once

#include <liburing.h>

#include "core/router.hpp"
#include "net/connection.h"

class UringDriver : public INetOut {
public:
  UringDriver(int fd);
  ~UringDriver();

  bool submit_recv(uint32_t slot);
  bool submit_send(uint32_t slot);
  bool submit_close(int fd);
  void send_to(const sockaddr_storage &dst, socklen_t dst_len, const void *data,
               size_t len) override;

  void recv(uint32_t slot, int res);
  void send(uint32_t slot, int res);

  SendState *acquire_send_slot(uint32_t &idx_out);
  void on_send_complete(uint32_t send_idx, int res);

  void start();

private:
  io_uring ring_{};
  int fd_{-1};
  static constexpr int kUdpSlots = 2;
  static constexpr uint32_t kSendSlots = 256;
  SendState send_[kSendSlots];
  uint32_t send_rr_ = 0;
  UdpState udp_[kUdpSlots];
  Router router_;
};
