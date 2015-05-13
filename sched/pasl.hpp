/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file pasl.hpp
 *
 */

#include "cmdline.hpp"
#include "threaddag.hpp"
#include "native.hpp"

#ifndef _PASL_PASL_H_
#define _PASL_PASL_H_

namespace pasl {
namespace sched {

/***********************************************************************/

#ifdef USE_CILK_RUNTIME
static long seq_fib (long n){
  if (n < 2)
    return n;
  else
    return seq_fib (n - 1) + seq_fib (n - 2);
}
#endif

template <class Body>
void launch(int argc, char** argv, const Body& body) {
  util::cmdline::set(argc, argv);
  threaddag::init();
  bool sequential = (util::cmdline::parse_or_default_int("proc", 1, false) == 0);
#if defined(NOPASL)
  body();
#elif defined(USE_CILK_RUNTIME)
  // hack that seems to be required to initialize cilk runtime cleanly
  cilk_spawn seq_fib(2);
  cilk_sync;
  body();
#else
  threaddag::launch(native::new_multishot_by_lambda([&] { body(sequential); }));
#endif
  threaddag::destroy();
}

/***********************************************************************/

} // end namespace
} // end namespace


#endif /*! _PASL_PASL_H_ */
