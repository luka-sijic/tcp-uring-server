#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#include <cstring>
#include <stdexcept>
#include <string>

static inline void pin_this_thread_to_cpu(int cpu) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);

  int rc = pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
  if (rc != 0) {
    throw std::runtime_error("pthread_setaffinity_np: " +
                             std::string(std::strerror(rc)));
  }
}
#endif