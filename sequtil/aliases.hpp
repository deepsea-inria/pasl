/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * Exports a collection of type abbreviations
 *
 */

#ifndef _ALIASES_H_
#define _ALIASES_H_

#include <string>

#include "microtime.hpp"
#include "barrier.hpp"
#include "ticks.hpp"

/***********************************************************************/

namespace pasl {

typedef int64_t worker_id_t;

typedef util::ticks::ticks_t ticks_t;

typedef util::microtime::microtime_t microtime_t;

typedef util::barrier::barrier_t barrier_t;
typedef barrier_t* barrier_p;

}

/***********************************************************************/

#endif /* _ALIASES_H_ */
