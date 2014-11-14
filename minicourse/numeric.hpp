/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file numeric.hpp
 * \brief Numeric algorithms
 *
 */

#include "sparray.hpp"

#ifndef _MINICOURSE_NUMERIC_H_
#define _MINICOURSE_NUMERIC_H_

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Dense matrix by dense vector multiplication */

value_type ddotprod(const sparray& m, long r, const sparray& v) {
  long n = v.size();
  return sum(tabulate([&] (long i) { return m[r*n+i] * v[i];}, n));
}

loop_controller_type dmdvmult_contr("dmdvmult");

sparray dmdvmult(const sparray& m, const sparray& v) {
  long n = v.size();
  sparray result = sparray(n);
  auto compl_fct = [n] (long lo, long hi) {
    return (hi-lo)*n;
  };
  par::parallel_for(dmdvmult_contr, compl_fct, 0l, n, [&] (long i) {
    result[i] = ddotprod(m, i, v);
  });
  return result;
}

/***********************************************************************/

#endif /*! _MINICOURSE_NUMERIC_H_ */
