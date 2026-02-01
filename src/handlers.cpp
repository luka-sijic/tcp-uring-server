#include "handlers.h"

namespace Handlers {
std::string handleGet() {
  static const std::string kResponse = "test";
  return kResponse;
}
} // namespace Handlers
