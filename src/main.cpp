#include "net/server.h"
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
  uint16_t port = 9000;
  if (argc >= 2) port = static_cast<uint16_t>(std::stoi(argv[1]));

  try {
    Server srv(port);
    srv.run();
  } catch (const std::exception &e) {
    std::cerr << "fatal: " << e.what() << "\n";
    return 1;
  }
  return 0;
}