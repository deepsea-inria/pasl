/*!
 * \file fib.cpp
 * \brief Exponential Fibonacci computation.
 * \example fib.cpp
 * \date 2014
 * \copyright COPYRIGHT (c) 2012 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 * Arguments:
 * ==================================================================
 *   - `-n <int>` (default=3)
 *       determines the value of fib(n) to compute
 *   - `-cutoff <int>` (default=20)
 *       sequentializes fib(m) as soon as m <= cutoff.
 *
 * Implementation: compute in parallel the recursive calls,
 * using fork-join.
 *
 */

#include <math.h>
#include "benchmark.hpp"

/***********************************************************************/

namespace par = pasl::sched::native;

long cutoff = 0;

/*---------------------------------------------------------------------*/

static long seq_fib (long n){
  if (n < 2)
    return n;
  else
    return seq_fib (n - 1) + seq_fib (n - 2);
}

/*---------------------------------------------------------------------*/

static long par_fib(long n) {
  if (n <= cutoff || n < 2)
    return seq_fib(n);
  long a, b;
  par::fork2([n, &a] { a = par_fib(n-1); },
             [n, &b] { b = par_fib(n-2); });
  return a + b;
}

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  long result = 0;
  long n = 0;

  /* The call to `launch` creates an instance of the PASL runtime and
   * then runs a few given functions in order. Specifically, the call
   * to launch calls our local functions in order:
   *
   *          init(); run(); output(); destroy();
   *
   * Each of these functions are allowed to call internal PASL
   * functions, such as `fork2`. Note, however, that it is not safe to
   * call such PASL library functions outside of the PASL environment.
   *
   * After the calls to the local functions all complete, the PASL
   * runtime reports among other things, the execution time of the
   * call `run();`.
   */

  auto init = [&] {
    cutoff = (long)pasl::util::cmdline::parse_or_default_int("cutoff", 25);
    n = (long)pasl::util::cmdline::parse_or_default_int("n", 24);
  };
  auto run = [&] (bool sequential) {
    result = par_fib(n);
  };
  auto output = [&] {
    std::cout << "result " << result << std::endl;
  };
  auto destroy = [&] {
    ;
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
  return 0;
}

/***********************************************************************/
