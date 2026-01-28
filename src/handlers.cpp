#include "handlers.h"
#include "models/response.h"

namespace Handlers {
std::string handleGet() { 
    static const std::string kResponse = Response::http200("test5");
    return kResponse;
}
} // namespace Handlers
