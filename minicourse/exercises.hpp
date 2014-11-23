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
  
void map_incr(const value_type* source, value_type* dest) {
  // todo: fill in
}
  
value_type max(value_type* source, long n, value_type seed) {
  return seed; // todo: fill in
}
  
value_type max(value_type* source, long n) {
  return max(source, VALUE_MIN);
}
  
value_type plus(value_type* source, long n, value_type seed) {
  return seed; // todo: fill in
}
  
value_type plus(value_type* source, long n) {
  return plus(source, n, (value_type)0);
}
  
template <class Assoc_comb_op>
value_type reduce(Assoc_comb_op op, value_type seed, const value_type* source, long n) {
  return seed; // todo fill in
}

sparray duplicate(const sparray& xs) {
  return empty(); // todo: fill in
}

sparray ktimes(const sparray& xs, long k) {
  return empty(); // todo: fill in
}
  
sparray pack_ex(const sparray& flags, const sparray& xs) {
  return empty(); // todo: fill in
}
  
// This function is the one you will debug and benchmark.
// As such, use "-check filter_ex" and "-bench filter_ex"
// where appropriate.
template <class Predicate>
sparray filter(Predicate p, const sparray& xs) {
  return pack_ex(map(p, xs), xs);
}
  
void merge_par(sparray& xs, sparray& tmp, long lo, long mid, long hi) {
  // todo: fill in
}
  
} // end namespace

/***********************************************************************/

#endif /*! _MINICOURSE_EXERCISES_H_ */
