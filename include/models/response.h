#pragma once

#include <charconv>
#include <sstream>
#include <unistd.h>
#include <string_view>

class Response {
public:
  Response() {}

  static std::string http200(std::string_view body) {
    std::string response;
    response.reserve(128 + body.size());

    response += "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: text/plain\r\n";
    response += "Content-Length: ";

    char lenbuf[32];
    auto [ptr, ec] = std::to_chars(lenbuf, lenbuf + sizeof(lenbuf), body.size());
    response.append(lenbuf, ptr);

    response += "\r\n";
    response += "Connection: keep-alive\r\n";
    response += "\r\n";
    response.append(body);

    return response;
  }

  static std::string http404(std::string_view body) {
    std::string response;
    response.reserve(128 + body.size());

    response += "HTTP/1.1 404 Not Found\r\n";
    response += "Content-Type: text/plain\r\n";
    response += "Content-Length: ";

    char lenbuf[32];
    auto [ptr, ec] = std::to_chars(lenbuf, lenbuf + sizeof(lenbuf), body.size());
    response.append(lenbuf, ptr);

    response += "\r\n";
    response += "Connection: keep-alive\r\n";
    response += "\r\n";
    response.append(body);

    return response;
  }
};
