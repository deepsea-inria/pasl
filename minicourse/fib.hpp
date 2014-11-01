/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file fib.hpp
 * \brief Fibonacci
 *
 */

#include <math.h>

#include "array.hpp"

#ifndef _MINICOURSE_FIB_H_
#define _MINICOURSE_FIB_H_

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Parallel fibonacci */

long de_moivre_fib(long n) {
  const double phi = 1.61803399;
  const double omega = -0.6180339887;
  double res = (pow(phi, (double)n)-pow(omega, (double)n))/sqrt(5);
  return (long)res;
}

long fib_seq(long n) {
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

long fib_par(long n) {
  long result;
  auto seq = [&] {
    result = fib_seq(n);
  };
  par::cstmt(fib_contr, [n] { return de_moivre_fib(n); }, [&] {
    if (n < 2) {
      seq();
    } else {
      long a,b;
      par::fork2([&] {
        a = fib_par(n-1);
      }, [&] {
        b = fib_par(n-2);
      });
      result = a+b;
    }
  }, seq);
  return result;
}

long fib(long n) {
#ifdef SEQUENTIAL_BASELINE
  return fib_seq(n);
#else
  return fib_par(n);
#endif
}

/***********************************************************************/

#endif /*! _MINICOURSE_FIB_H_ */