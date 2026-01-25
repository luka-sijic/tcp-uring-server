#include "trace.h"
#include <cstdio>
#include <cstdarg>

namespace trace {

void emit(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(stderr, fmt, ap);
  std::fputc('\n', stderr);
  va_end(ap);
}

} // namespace trace
