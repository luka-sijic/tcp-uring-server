#include "core/router.hpp"
#include <iostream>

/*
/ root
change to use radix tree in the future
*/
void Router::get(const std::string &path, Handler func) {
  RouteKey rk{"GET", path};
  routes_[rk] = func;
}

void Router::post(const std::string &path, Handler func) {
  RouteKey rk{"POST", path};
  routes_[rk] = func;
}

Handler Router::match(std::string method, std::string path) {
  RouteKey rk{method, path};
  auto it = routes_.find(rk);
  if (it != routes_.end()) {
    return it->second;
  }
  return []() {
    return Response::http404("Not Found");
  };
}
