

#include "microtime.hpp"
#include <sys/time.h>
#include <time.h>

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

void microsleep(double t) { 
  for (int k = 0; k < (int) (10*t); k++) {
    for (int j = k; j < k + 5; j++)
      (k/(j+1));
  }
}

/***********************************************************************/

} // namespace
} // namespace
} // namespace

