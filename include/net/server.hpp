#pragma once

#include <cstdint>
#include <memory>

#include "core/router.hpp"
#include "net/uring_driver.hpp"

class Server {
public:
  explicit Server(Router *r, uint16_t port);
  //~Server();

  Server(Server&&) = delete;
  Server& operator=(Server&&) = delete;

  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  void run();

private:
  //struct Impl;
  //std::unique_ptr<Impl> impl_;
  Router *router_;
  uint16_t port_;
};
