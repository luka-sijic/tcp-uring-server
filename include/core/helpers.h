#pragma once
#include <cstdint>
#include <liburing.h>

enum class Op : uint8_t { RECV = 1, SEND = 2, CLOSE = 3 };

static inline uint64_t pack_ud_slot(Op op, uint32_t slot) {
  return (uint64_t(uint32_t(op)) << 32) | uint64_t(slot);
}

static inline Op unpack_op_slot(uint64_t ud) { return Op(uint32_t(ud >> 32)); }

static inline uint32_t unpack_slot(uint64_t ud) {
  return uint32_t(ud & 0xffffffffu);
}
