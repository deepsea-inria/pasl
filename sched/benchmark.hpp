/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file benchmark.hpp
 *
 */

#ifdef DUMP_JEMALLOC_STATS
#include <jemalloc/jemalloc.h>
#endif

#ifdef USE_LIBNUMA
#include <numa.h>
#endif

#include "cmdline.hpp"
#include "threaddag.hpp"
#include "native.hpp"

#ifndef _PASL_BENCHMARK_H_
#define _PASL_BENCHMARK_H_

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
void launch(const Body& body) {
#if defined(NOPASL)
  body();
#elif defined(USE_CILK_RUNTIME)
  // hack that seems to be required to initialize cilk runtime cleanly
  cilk_spawn seq_fib(2);
  cilk_sync;
  body();
#else
  threaddag::launch(native::new_multishot_by_lambda([&] { body(); }));
#endif
}

template <class Init, class Run, class Output, class Destroy>
void launch(const Init& init, const Run& run, const Output& output,
            const Destroy& destroy) {
  bool sequential = (util::cmdline::parse_or_default_int("proc", 1, false) == 0);
  bool report_time = util::cmdline::parse_or_default_bool("report_time", true, false);
#ifdef USE_LIBNUMA
  numa_set_interleave_mask(numa_all_nodes_ptr);
#endif
  threaddag::init();
  launch(init);
  LOG_BASIC(ENTER_ALGO);
  uint64_t start_time = util::microtime::now();
  launch([&] { run(sequential); });
  double exec_time = util::microtime::seconds_since(start_time);
  LOG_BASIC(EXIT_ALGO);
  if (report_time)
    printf ("exectime %.3lf\n", exec_time);
  STAT_IDLE(sum());
  STAT(dump(stdout));
  STAT_IDLE(print_idle(stdout));
#ifdef DUMP_JEMALLOC_STATS
  // Dump allocator statistics to stderr.
  malloc_stats_print(NULL, NULL, NULL);
#endif
  launch(output);
  launch(destroy);
  threaddag::destroy();
}

template <class Init, class Run, class Output, class Destroy>
void launch(int argc, char** argv,
            const Init& init, const Run& run, const Output& output,
            const Destroy& destroy) {
  util::cmdline::set(argc, argv);
  launch(init, run, output, destroy);
}

/***********************************************************************/

} // end namespace
} // end namespace


#endif /*! _PASL_BENCHMARK_H_ */
