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
  
void map_incr(const value_type* source, value_type* dest, long n) {
  // todo: fill in
}
 
// source: pointer to the first item of the source array
// n: number of items in the source array
// seed: value to return in the case where `n` == 0
value_type max(const value_type* source, long n, value_type seed) {
  return seed; // todo: fill in
}
  
value_type max(const value_type* source, long n) {
  return max(source, n, VALUE_MIN);
}
  
value_type plus_rec(const value_type* source, long lo, long hi, value_type seed) {
  value_type result = seed;
  if (hi - lo == 0)
    result = seed;
  else if (hi - lo == 1)
    result = source[lo];
  else {
    long mid = (hi + lo) / 2;
    value_type a = plus_rec(source, lo, mid, seed);
    value_type b = plus_rec(source, mid, hi, seed);
    result = a + b;
  }
  return result;
}
  
value_type plus(const value_type* source, long n, value_type seed) {
  return plus_rec(source, 0, n, seed);
}
  
value_type plus(const value_type* source, long n) {
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
  
// Merge the combined contents in the two given ranges of
// the two given source arrays, namely `xs` and `ys`, copying
// out the result to `tmp`.
// In each of the two given ranges in the two given source
// arrays, the items are guaranteed by precondition to appear
// in ascending order.
// By postcondition, the items that are written to tmp are to
// appear in ascending order as well.
// More specifically, we want to merge the combined results in the
// range [lo_xs, hi_xs) of xs and in the range [lo_ys, hi_ys) of
// ys, copying out the items to the range [lo_tmp, m) of tmp,
// where m = (hi_xs-lo_xs)+(hi_ys-lo_ys).
// For an example use of this function, see the function in
// examples.hpp that is named `merge_ex_test`.
void merge_par(const sparray& xs, const sparray& ys, sparray& tmp,
               long lo_xs, long hi_xs,
               long lo_ys, long hi_ys,
               long lo_tmp) {
  // todo: fill in
}
  
void merge_par(sparray& xs, sparray& tmp, long lo, long mid, long hi) {
  merge_par(xs, xs, tmp, lo, mid, mid, hi, lo);
  // copy back to source array
  prim::pcopy(&tmp[0], &xs[0], lo, hi, lo);
}
  
} // end namespace

/***********************************************************************/

#endif /*! _MINICOURSE_EXERCISES_H_ */
