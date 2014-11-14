/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file dup.hpp
 * \brief Item-duplication algorithms
 *
 */

#include "sparray.hpp"

#ifndef _MINICOURSE_DUP_H_
#define _MINICOURSE_DUP_H_

/***********************************************************************/

sparray duplicate(const sparray& xs) {
  long m = xs.size() * 2;
  return tabulate([&] (long i) { return xs[i/2]; }, m);
}

sparray ktimes(const sparray& xs, long k) {
  long m = xs.size() * k;
  return tabulate([&] (long i) { return xs[i/k]; }, m);
}

/***********************************************************************/

#endif /*! _MINICOURSE_DUP_H_ */
