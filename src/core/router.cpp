#include "core/router.hpp"
#include <iostream>

/*
/ root
change to use radix tree in the future
*/
void Router::get(const std::string &path, Handler func) {
  std::string key;
  key.reserve(4 + path.size());
  key.append("GET ");
  key.append(path);
  routes_[key] = func;
}

void Router::post(const std::string &path, Handler func) {
  std::string key;
  key.reserve(5 + path.size());
  key.append("POST ");
  key.append(path);
  routes_[key] = func;
}

Handler Router::match(std::string_view method, std::string_view path) {
  std::string key;
  key.reserve(method.size() + 1 + path.size());
  key.append(method);
  key.push_back(' ');
  key.append(path);
  auto it = routes_.find(key);
  if (it != routes_.end()) {
    return it->second;
  }
  return []() {
    return Response::http404("Not Found");
  };
}
