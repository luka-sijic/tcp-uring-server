#pragma once

#include <charconv>
#include <cstring>
#include <string>
#include <string_view>

class Response {
public:
  static std::string http200(std::string_view body) {
    return build("HTTP/1.1 200 OK\r\n", body);
  }

  static std::string http404(std::string_view body) {
    return build("HTTP/1.1 404 Not Found\r\n", body);
  }

private:
  static std::string build(std::string_view status_line,
                           std::string_view body) {
    static constexpr std::string_view kHeaders1 = "Content-Type: text/plain\r\n"
                                                  "Content-Length: ";
    static constexpr std::string_view kHeaders2 = "\r\n"
                                                  "Connection: keep-alive\r\n"
                                                  "\r\n";

    char lenbuf[32];
    auto [ptr, ec] =
        std::to_chars(lenbuf, lenbuf + sizeof(lenbuf), body.size());
    (void)ec;
    const size_t len_digits = static_cast<size_t>(ptr - lenbuf);

    const size_t total = status_line.size() + kHeaders1.size() + len_digits +
                         kHeaders2.size() + body.size();

    std::string out(total, '\0');
    char *p = out.data();

    auto copy_sv = [&](std::string_view sv) {
      std::memcpy(p, sv.data(), sv.size());
      p += sv.size();
    };

    copy_sv(status_line);
    copy_sv(kHeaders1);
    std::memcpy(p, lenbuf, len_digits);
    p += len_digits;
    copy_sv(kHeaders2);
    std::memcpy(p, body.data(), body.size());
    p += body.size();

    return out;
  }
};
