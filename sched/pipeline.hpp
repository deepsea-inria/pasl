/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file thread.hpp
 *
 */

#include <vector>

#include "native.hpp"

#ifndef _PASL_SCHED_PIPELINE_H_
#define _PASL_SCHED_PIPELINE_H_

/***********************************************************************/

namespace pasl {
namespace sched {
namespace pipeline {

template <class Done, class Body>
void pipe_while(long nb_stages, const Done& done, const Body& body) {
  using outstrategy_type = outstrategy::one_to_one_future;
  using stages_type = std::vector<outstrategy_type*>;
  native::finish([&] (thread_p join) {
    stages_type* prev = nullptr;
    while (! done()) {
      long i = 0;
      stages_type* next = new stages_type(nb_stages, nullptr);
      for (auto it = next->begin(); it != next->end(); it++) {
        *it = new outstrategy::one_to_one_future();
      }
      auto cont = [=] (long s) {
        long t = s + 1;
        if (t >= nb_stages) {
          return;
        }
        outstrategy_type* fwd = (*next)[t];
        fwd->finished();
      };
      auto wait = [=] (long s) {
        if (prev != nullptr) {
          outstrategy_type* bk = (*prev)[s];
          native::multishot* t = native::my_thread();
          t->wait(bk);
        }
        cont();
      };
      native::async([&] {
        body(wait, cont);
        if (prev != nullptr) {
          delete prev;
        }
      }, join);
      i++;
    }
    if (prev != nullptr) {
      delete prev;
    }
  });
}
  
} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_SCHED_PIPELINE_H_ */