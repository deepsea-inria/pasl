/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file ticks.hpp
 * \brief Collection of operations for measuring elapsed time
 *
 */

#ifndef _TICKS_H_
#define _TICKS_H_

#include <stdint.h>

#include "cycles.hpp"

namespace pasl {
namespace util {
namespace ticks {
  
/***********************************************************************/

typedef ticks_repr ticks_t;

/* Get current timestamp counter */

ticks_t now();

/* Convert into uint64_t */

uint64_t to_uint64(ticks_t t);

/* Compute difference between two timestamps */

double diff(ticks_t t1, ticks_t t2);

/* Compute difference between a timestamp and now;
   since(t) is equivalent to diff(t, now()) */

double since(ticks_t t);

/* Set the number of ticks per seconds (machine dependent) */

void set_ticks_per_seconds(double nb);

/* Convert in seconds 
   -- requires set_ticks_per_seconds to be called */

double seconds(ticks_t t);

/* Convert in microseconds 
   -- requires set_ticks_per_seconds to be called */

double microseconds(ticks_t t);

/* Convert in nanoseconds 
   -- requires set_ticks_per_seconds to be called */

double nanoseconds(ticks_t t);

/* Compute difference directly in seconds
    -- requires set_ticks_per_seconds to be called  */

double seconds_since(ticks_t t);

/* Same for microseconds */

double microseconds_since(ticks_t t);

/* Same for nanoseconds */

double nanoseconds_since(ticks_t t);

/* Sleep by "while(true){} for a number of microseconds */

void microseconds_sleep(double nb);


/***********************************************************************/


} // namespace
} // namespace
} // namespace

#endif /*! _TICKS_H_ */




