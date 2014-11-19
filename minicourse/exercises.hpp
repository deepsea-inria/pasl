/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file exercises.hpp
 * \brief File that students are to use to enter solutions to the
 *  exercises that are assigned in the book.
 *
 */

#include <limits.h>

#include "sparray.hpp"

#ifndef _MINICOURSE_EXERCISES_H_
#define _MINICOURSE_EXERCISES_H_

/***********************************************************************/

namespace exercises {
  
value_type max(value_type* source, value_type seed) {
  return seed; // todo: fill in
}
  
value_type max(value_type* source) {
  return max(source, LONG_MIN);
}
  
value_type plus(value_type* source, value_type seed) {
  return seed; // todo: fill in
}
  
value_type plus(value_type* source) {
  return plus(source, 0l);
}

sparray duplicate(const sparray& xs) {
  return empty(); // todo: fill in
}

sparray ktimes(const sparray& xs, long k) {
  return empty(); // todo: fill in
}
  
void merge_par(sparray& xs, sparray& tmp, long lo, long mid, long hi) {
  // todo: fill in
}
  
} // end namespace

/***********************************************************************/

#endif /*! _MINICOURSE_EXERCISES_H_ */
