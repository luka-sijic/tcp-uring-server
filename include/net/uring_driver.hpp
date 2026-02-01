#pragma once

#include <liburing.h>

#include "core/router.hpp"
#include "net/connection.h"

class UringDriver : public INetOut {
public:
  UringDriver(int fd);
  ~UringDriver() noexcept override;

  bool submit_recv(uint32_t slot) noexcept;
  bool submit_send(uint32_t slot) noexcept;
  bool submit_close(int fd) noexcept;
  void send_to(const sockaddr_storage &dst, socklen_t dst_len, const void *data,
               size_t len) noexcept override;

  void recv(uint32_t slot, int res) noexcept;
  void send(uint32_t slot, int res) noexcept;

  [[nodiscard]] SendState *acquire_send_slot(uint32_t &idx_out) noexcept;
  void on_send_complete(uint32_t send_idx, int res) noexcept;

  void start() noexcept;

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
