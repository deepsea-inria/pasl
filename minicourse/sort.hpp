/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file sort.hpp
 * \brief Sorting algorithms
 *
 */

#include <cstring>
#include <cmath>

#include "native.hpp"
#include "sparray.hpp"

#ifndef _MINICOURSE_SORT_H_
#define _MINICOURSE_SORT_H_

/***********************************************************************/

long nlogn(long n) {
  return pasl::data::estimator::annotation::nlgn(n);
}

/*---------------------------------------------------------------------*/
/* Sequential sort */

void in_place_sort(sparray& xs, long lo, long hi) {
  long n = hi-lo;
  if (n < 2)
    return;
  std::sort(&xs[lo], &xs[hi-1]+1);
}

void in_place_sort(sparray& xs) {
  in_place_sort(xs, 0l, xs.size());
}

sparray seqsort(const sparray& xs) {
  sparray tmp = copy(xs);
  in_place_sort(tmp);
  return tmp;
}

sparray seqsort(const sparray& xs, long lo, long hi) {
  sparray tmp = slice(xs, lo, hi);
  in_place_sort(tmp);
  return tmp;
}

/*---------------------------------------------------------------------*/
/* Parallel quicksort */

value_type median(value_type a, value_type b, value_type c) {
  return  a < b ? (b < c ? b : (a < c ? c : a))
  : (a < c ? a : (b < c ? c : b));
}

controller_type quicksort_contr("quicksort");

sparray quicksort(const sparray& xs) {
  long n = xs.size();
  sparray result = { };
  auto seq = [&] {
    result = seqsort(xs);
  };
  par::cstmt(quicksort_contr, [&] { return nlogn(n); }, [&] {
    if (n <= 4) {
      seq();
    } else {
      value_type p = median(xs[n/4], xs[n/2], xs[3*n/4]);
      sparray less = filter([&] (value_type x) { return x < p; }, xs);
      sparray equal = filter([&] (value_type x) { return x == p; }, xs);
      sparray greater = filter([&] (value_type x) { return x > p; }, xs);
      sparray left = { };
      sparray right = { };
      par::fork2([&] {
        left = quicksort(less);
      }, [&] {
        right = quicksort(greater);
      });
      result = concat(left, equal, right);
    }
  }, seq);
  return result;
}

/*---------------------------------------------------------------------*/
/* Parallel mergesort */

template <bool use_parallel_merge=true>
sparray mergesort(const sparray& xs) {
  // placeholder for reference solution
  return copy(xs);
}

template <bool use_parallel_merge=true>
sparray cilksort(const sparray& xs) {
  // placeholder for reference solution
  return copy(xs);
}

/***********************************************************************/

#endif /*! _MINICOURSE_SORT_H_ */
