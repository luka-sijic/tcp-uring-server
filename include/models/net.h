#pragma once
#include <cstdint>

struct Players {
  std::uint32_t op; // 0 register 1 player update
  std::uint32_t id;
  float x;
  float y;
  std::uint32_t color;
  std::uint32_t size;
};
