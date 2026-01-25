#pragma once
#include <cstdarg>

#ifdef ENABLE_TRACE
  #define TRACE_ENABLED 1
  #define TRACE(...) ::trace::emit(__VA_ARGS__)
#else
  #define TRACE_ENABLED 0
  #define TRACE(...) do {} while (0)
#endif

namespace trace {
  void emit(const char* fmt, ...);
}