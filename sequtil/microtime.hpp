/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file microtime.hpp
 * \brief Interface for system time; unit is microseconds
 *
 */

#ifndef _PASL_UTIL_MICROTIME_H_
#define _PASL_UTIL_MICROTIME_H_

#include <stdint.h>

namespace pasl {
namespace util {
namespace microtime {

/***********************************************************************/

typedef uint64_t microtime_t;

/* Convert in seconds */

double seconds(microtime_t t);

/* Get current time in microseconds */

microtime_t now();

/* Compute difference between two times, assert t2>=t1 */

microtime_t diff(microtime_t t1, microtime_t t2);

/* Compute difference between a given time and now;
   since(t) is equivalent to diff(t, now()) */

microtime_t since(microtime_t t);

/* Compute difference directly in seconds */

double seconds_since(microtime_t t);

/* Same for microseconds */

double microseconds_since(microtime_t t);


  // todo: microsleep(
void microsleep(double t);

// todo: present as class!


/***********************************************************************/

} // namespace
} // namespace
} // namespace

/***********************************************************************/

/* For debugging purposes, you can measure the time taken by a piece 
   of code by writing
      MICROTIME_START;
      my_code
      MICROTIME_END("my_code_name");
*/

#define MICROTIME_START \
  microtime::microtime_t __microtime = microtime::now()
#define MICROTIME_END(name) \
  printf("time_%s %lf\n", name, microtime::seconds_since(__microtime))



#endif /*! _PASL_UTIL_MICROTIME_H_ */

