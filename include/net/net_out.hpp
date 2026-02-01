#pragma once
#include <cstdint>
#include <sys/socket.h>

struct INetOut {
  virtual void send_to(const sockaddr_storage &dst, socklen_t dst_len,
                       const void *data, size_t len) = 0;
  virtual ~INetOut() = default;
};
