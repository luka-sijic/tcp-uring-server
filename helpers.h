#pragma once
#include <liburing.h>
#include <cstdint>
#include <new> 

enum class Op : uint8_t { ACCEPT = 1, RECV = 2, SEND = 3, CLOSE = 4 };

static inline uint64_t pack_ud(Op op, int fd, uint32_t aux = 0) {
  return (uint64_t(op) << 56) | (uint64_t(uint32_t(fd) & 0x00FFFFFF) << 32) |
         uint64_t(aux);
}
static inline Op unpack_op(uint64_t ud) { return Op((ud >> 56) & 0xFF); }
static inline int unpack_fd(uint64_t ud) {
  return int((ud >> 32) & 0x00FFFFFF);
}
static inline uint32_t unpack_aux(uint64_t ud) {
  return uint32_t(ud & 0xFFFFFFFFu);
}