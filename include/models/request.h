#include <string_view>
#include <vector>

struct HttpHeader {
  std::string_view name;
  std::string_view value;
};

struct HttpRequest {
  std::string_view method;
  std::string_view path;
  std::string_view version;
  std::vector<HttpHeader> headers;
};
