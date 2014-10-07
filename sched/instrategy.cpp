/* COPYRIGHT (c) 2012 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file instrategy.cpp
 *
 */

#include <set>
#include <iostream>

#include "instrategy.hpp"

namespace pasl {
namespace sched {
namespace instrategy {

/***********************************************************************/
  
/* deprecated
  
distributed::distributed(thread_p t) {
  this->t = t;
  diff.init(0l);
}

void distributed::init(thread_p t) {
  this->t = t;
  common::init(t);
  scheduler::get_mine()->add_periodic(this);
}

void distributed::start(thread_p t) {
  assert(t == this->t);
  common::start(t);
}

void distributed::check(thread_p t) {
  assert(t == this->t);
  if (get_diff() == 0) {  
    scheduler::get_mine()->rem_periodic(this);
    start(t);
  }
}

void distributed::check() {
  check(t);
}

void distributed::delta(thread_p t, int64_t d) {
  assert(t == this->t);
  worker_id_t id = util::worker::get_my_id();
  diff[id] += d;
}

int64_t distributed::get_diff() {
  int nb_workers = util::worker::get_nb();
  int64_t sum = 0l;
  for (worker_id_t id = 0; id < nb_workers; id++)
    sum += diff[id];
  return sum;
}

 */
  
/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace
