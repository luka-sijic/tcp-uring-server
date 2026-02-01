#pragma once

#include <cstdint>
#include <memory>

#include "net/uring_driver.hpp"

class Server {
public:
  explicit Server(uint16_t port);
  static int make_listen_socket(uint16_t port);
  //~Server();

  Server(Server &&) = delete;
  Server &operator=(Server &&) = delete;

  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  void run();

private:
  uint16_t port_;
};
