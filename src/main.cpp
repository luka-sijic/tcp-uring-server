#include <exception>
#include <iostream>

#include "core/cpu_pin.hpp"
#include "net/server.hpp"
#include "net/uring_driver.hpp"

static const auto fast_io = []() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
  return 0;
}();

int main() {
  try {
    const std::uint16_t port = 9000;

    int fd = Server::make_listen_socket(port);
    UringDriver driver(fd);
    driver.start();

    return 0;

  } catch (const std::exception &e) {
    std::cerr << "fatal: " << e.what() << "\n";
    return 1;
  }
}
