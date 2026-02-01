#pragma once
#include "core/cursor.h"
#include <cstdint>
#include <span>

enum class Op : uint8_t {
  Register = 0,
  Update = 1,
};

struct ParseResult {
  enum class Kind { Register, Update, Invalid } kind;
  uint32_t id;
  float x = 0.f;
  float y = 0.f;
};

class Parser {
public:
  static ParseResult parse(std::span<const std::byte> buf) {
    Cursor c(buf);

    auto op = static_cast<Op>(c.read<uint8_t>());
    auto id = c.read<uint32_t>();

    switch (op) {
    case Op::Register:
      return {.kind = ParseResult::Kind::Register, .id = id};

    case Op::Update: {
      float x = c.read<float>();
      float y = c.read<float>();
      return {.kind = ParseResult::Kind::Update, .id = id, .x = x, .y = y};
    }

    default:
      return {.kind = ParseResult::Kind::Invalid};
    }
  }
};
