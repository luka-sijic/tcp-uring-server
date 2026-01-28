#pragma once

#include <functional>
#include <models/response.h>
#include <string>
#include <string_view>
/*
hello Post
hello Get
*/

using Handler = std::function<std::string()>;

class Router {
public:
  Router() = default;
  void get(const std::string &path, Handler func);
  void post(const std::string &path, Handler func);
  Handler match(std::string_view method, std::string_view path);

private:
  std::unordered_map<std::string, Handler> routes_;
};
