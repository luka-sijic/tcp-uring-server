#include <exception>
#include <iostream>

#include "core/router.hpp"
#include "handlers.h"
#include "net/server.hpp"

int main() {
  std::uint16_t port = 9000;

  try {
    auto router = std::make_unique<Router>();

    router->get("/", Handlers::handleGet);

    Server srv(router.get(), port);
    //srv.run();
  } catch (const std::exception &e) {
    std::cerr << "fatal: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
