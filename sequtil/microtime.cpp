

#include "microtime.hpp"
#include <sys/time.h>
#include <time.h>
#include <stdio.h>

namespace pasl {
namespace util {
namespace microtime {

/***********************************************************************/

microtime_t now() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return 1000000l * ((microtime_t) tv.tv_sec) + ((microtime_t) tv.tv_usec);
}

microtime_t diff(microtime_t t1, microtime_t t2) {
  return t2 - t1;
}

microtime_t since(microtime_t t) {
  return diff(t, now());
}

double seconds(microtime_t t) {
  return ((double) t) / 1000000l;
}

  
double seconds_since(microtime_t t) {
  return seconds(since(t));
}

double microseconds_since(microtime_t t) {
  return (double) since(t);
}

// wait for a very little time
// t is number of cycles
void microsleep(double t) {
  long r = 0;
  for (long k = 0; k < (long) t; k++) {
    r += k;
  }
  if (r == 1234) {
    printf("ERROR\n");
  }
}

inline uint64_t Rdtsc() {
  unsigned int hi, lo;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return  ((uint64_t) lo) | (((uint64_t) hi) << 32);
}

void RdtscWait(uint64_t n) {
  const uint64_t start = Rdtsc();
  while (Rdtsc() < (start + n)) {
    __asm__("PAUSE");
  }
}

void wait_for(long n) {
  RdtscWait((uint64_t)n);
}
  
/***********************************************************************/

} // namespace
} // namespace
} // namespace

