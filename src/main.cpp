#include <algorithm>
#include <exception>
#include <iostream>
#include <thread>
#include <vector>

#include "core/cpu_pin.hpp"
#include "core/router.hpp"
#include "handlers.h"
#include "net/server.hpp"
#include "net/uring_driver.hpp"

struct Worker {
  int listen_fd{-1};
  UringDriver driver;
  int cpu{-1};

  Worker(int fd, Router *r, int cpu_)
      : listen_fd(fd), driver(r, fd), cpu(cpu_) {}

  void run() {
#ifdef __linux__
    if (cpu >= 0)
      pin_this_thread_to_cpu(cpu);
#endif
    driver.run();
  }
};

int main() {
  try {
    const std::uint16_t port = 9000;

    // 1) Build router once
    auto router = std::make_unique<Router>();
    router->get("/", Handlers::handleGet);

    // 2) Spawn workers
    unsigned N = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;
    threads.reserve(N);

    Router *r = router.get(); // shared, read-only after setup

    for (unsigned i = 0; i < N; ++i) {
      threads.emplace_back([=]() {
#ifdef __linux__
        const int cpu = int(i % N);
#else
        const int cpu = -1;
#endif

        int fd = Server::make_listen_socket(port);
        Worker w(fd, r, cpu);
        w.run();
        ::close(fd);
      });
    }

    for (auto &t : threads)
      t.join();
    return 0;

  } catch (const std::exception &e) {
    std::cerr << "fatal: " << e.what() << "\n";
    return 1;
  }
}
