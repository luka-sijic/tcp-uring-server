#include "handlers.h"
#include "models/response.h"

namespace Handlers {
std::string handleGet() { 
    return Response::http200("test5");
}
} // namespace Handlers
