/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file fib.hpp
 * \brief Fibonacci
 *
 */

#include <math.h>
#include <climits>

#ifndef _MINICOURSE_FIB_H_
#define _MINICOURSE_FIB_H_

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Parallel fibonacci */

static long fib_seq(long n) {
  long result;
  if (n < 2) {
    result = n;
  } else {
    long a,b;
    a = fib_seq(n-1);
    b = fib_seq(n-2);
    result = a+b;
  }
  return result;
}

controller_type fib_contr("fib");

static long fib_par(long n) {
  long result;
  par::cstmt(fib_contr, [&] { return 1l<<n; }, [&] {
    if (n < 2) {
      result = n;
    } else {
      long a,b;
      par::fork2([&] {
        a = fib_par(n-1);
      }, [&] {
        b = fib_par(n-2);
      });
      result = a+b;
    }
  }, [&] {
    result = fib_seq(n);
  });
  return result;
}

long fib(long n) {
#ifdef SEQUENTIAL_BASELINE
  return fib_seq(n);
#else
  return fib_par(n);
#endif
}

const long threshold = 4;

long mfib(long n) {
  long result;
  if (n <= threshold) {
    result = fib_seq(n);
  } else {
    long a,b;
    pasl::sched::native::fork2([&] {
      a = fib_par(n-1);
    }, [&] {
      b = fib_par(n-2);
    });
    result = a+b;
  }
return result;
}

/***********************************************************************/

#endif /*! _MINICOURSE_FIB_H_ */
