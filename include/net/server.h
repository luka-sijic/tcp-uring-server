#pragma once
#include <cstdint>
#include <memory>

class Server {
public:
  explicit Server(uint16_t port);
  ~Server();

  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  void run();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
