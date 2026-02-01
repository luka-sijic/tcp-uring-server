#pragma once

#include <liburing.h>
#include <unordered_map>

#include "net/connection.h"

struct PeerInfo {
  sockaddr_storage addr{};
  socklen_t len{};
};

class UringDriver {
public:
  UringDriver(int fd);
  ~UringDriver();

  bool submit_recv(uint32_t slot);
  bool submit_send(uint32_t slot);
  bool submit_close(int fd);
  void submit_send_to(uint32_t slot, const sockaddr_storage &dst,
                      socklen_t dst_len, const void *data, size_t len);

  void recv(uint32_t slot, int res);
  void send(uint32_t slot, int res);
  void broadcast_add_player(uint32_t id);
  void send_existing_players_to(uint32_t new_id);

  void start();

private:
  io_uring ring_{};
  int fd_{-1};
  std::unordered_map<uint32_t, PeerInfo> players_;
  static constexpr int kUdpSlots = 2;
  UdpState udp_[kUdpSlots];
};
