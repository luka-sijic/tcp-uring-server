#pragma once
#include <cstring>
#include <iostream>
#include <models/net.h>

/*
op - 4 bytes
id - 4 bytes
x - 4 bytes
y - 4 bytes
color - 4 bytes
size - 4 bytes

*/

struct Init {
  int x;
};

class Parser {
public:
  Parser() {}

  Players parse(std::span<const std::byte> bytes) {
    Players p{};
    if (bytes.size() == (int)sizeof(Players)) {
      std::memcpy(&p, bytes.data(), sizeof(Players));
      std::cerr << "PARSE TEST: ";
      std::cerr << p.x << '\n';
    } else if (bytes.size() == (int)sizeof(Init)) {
      std::cerr << "TEST" << '\n';
    }
    return p;
  }

private:
  int res_;
};
