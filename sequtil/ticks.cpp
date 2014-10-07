/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file ticks.hpp
 * \brief Collection of operations for measuring elapsed time
 *
 */

#include "ticks.hpp"

namespace pasl {
namespace util {
namespace ticks {

/***********************************************************************/

uint64_t to_uint64(ticks_t t) {
  return (uint64_t) t;
}

ticks_t now () { 
  return getticks();
}

double diff (ticks_t t1, ticks_t t2) {
  return elapsed (t2, t1);
}

double since (ticks_t t) {
  return diff (t, now ());
}

double ticks_per_seconds;

void set_ticks_per_seconds(double nb) {
  ticks_per_seconds = nb;
}

double seconds(ticks_t t) {
  return ((double) t) / ticks_per_seconds;
}

double microseconds(ticks_t t) {
  return ((double) t) * 1000000. / ticks_per_seconds;
}

double nanoseconds(ticks_t t) {
  return ((double) t) * 1000000000. / ticks_per_seconds;
}

double seconds_since(ticks_t t) {
  return seconds(since(t));
}

double microseconds_since(ticks_t t) {
  return microseconds(since(t)) ;
}

double nanoseconds_since(ticks_t t) {
  return nanoseconds(since(t)) ;
}

void microseconds_sleep(double nb) {
   ticks_t start = now();
   while (microseconds_since(start) < nb) { }
}



/***********************************************************************/

} // namespace
} // namespace
} // namespace
