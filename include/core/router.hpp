#pragma once

#include <functional>
#include <models/response.h>
#include <string>
/*
hello Post
hello Get
*/

using Handler = std::function<std::string()>;

struct RouteKey {
  std::string method;
  std::string path;
  bool operator==(const RouteKey &o) const noexcept {
    return method == o.method && path == o.path;
  }
};

struct RouteKeyHash {
  size_t operator()(const RouteKey &k) const noexcept {
    size_t h1 = std::hash<std::string>{}(k.method);
    size_t h2 = std::hash<std::string>{}(k.path);
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
  }
};

class Router {
public:
  Router() = default;
  void get(const std::string &path, Handler func);
  void post(const std::string &path, Handler func);
  Handler match(std::string method, std::string path);

private:
  std::unordered_map<RouteKey, Handler, RouteKeyHash> routes_;
};
