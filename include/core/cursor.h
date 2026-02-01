#pragma once
#include <cstring>
#include <span>
#include <stdexcept>

class Cursor {
public:
  Cursor(std::span<const std::byte> buf) : buf_(buf) {}

  template <typename T> T read() {
    if (off_ + sizeof(T) > buf_.size())
      throw std::runtime_error("short read");
    T v;
    std::memcpy(&v, buf_.data() + off_, sizeof(T));
    off_ += sizeof(T);
    return v;
  }

  size_t remaining() const { return buf_.size() - off_; }

private:
  std::span<const std::byte> buf_;
  size_t off_ = 0;
};
