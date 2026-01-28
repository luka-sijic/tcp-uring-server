#include "net/server.h"
#include <exception>
#include <iostream>

int main(int argc, char **argv) {
  std::uint16_t port = 9000;

  if (argc >= 2) {
    std::string_view s{argv[1]};
    unsigned value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);

    if (ec != std::errc{} || ptr != s.data() + s.size() || value > 65535u ||
        value == 0u) {
      std::cerr << "usage: " << argv[0] << " [port 1-65535]\n";
      return 1;
    }
    port = static_cast<std::uint16_t>(value);
  }
  try {
    Server srv(port);
    srv.run();
  } catch (const std::exception &e) {
    std::cerr << "fatal: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
