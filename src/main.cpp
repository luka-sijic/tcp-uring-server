#include <exception>
#include <iostream>

#include "core/cpu_pin.hpp"
#include "core/router.hpp"
#include "handlers.h"
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

    // 1) Build router once
    auto router = std::make_unique<Router>();
    router->get("/", Handlers::handleGet);

    Router *r = router.get(); // shared, read-only after setup
    int fd = Server::make_listen_socket(port);
    UringDriver driver(r, fd);
    driver.start();

    return 0;

  } catch (const std::exception &e) {
    std::cerr << "fatal: " << e.what() << "\n";
    return 1;
  }
}
