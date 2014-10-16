/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file sort.hpp
 * \brief Sorting algorithms
 *
 */

#include "array.hpp"

#ifndef _MINICOURSE_SORT_H_
#define _MINICOURSE_SORT_H_

/***********************************************************************/


long nlogn(long n) {
  return pasl::data::estimator::annotation::nlgn(n);
}

void in_place_sort(array_ref xs, long lo, long hi) {
  if (hi-lo > 0)
    std::sort(&xs[lo], &xs[hi-1]+1);
}

void in_place_sort(array_ref xs) {
  in_place_sort(xs, 0l, xs.size());
}

controller_type quicksort_contr("quicksort");

array quicksort(const_array_ref xs) {
  long n = xs.size();
  array result = empty();
  auto seq = [&] {
    result = copy(xs);
    in_place_sort(result);
  };
  par::cstmt(quicksort_contr, [&] { return nlogn(n); }, [&] {
    if (n < 2) {
      seq();
    } else {
      long m = n/2;
      value_type p = xs[m];
      array less = filter([&] (value_type x) { return x < p; }, xs);
      array equal = filter([&] (value_type x) { return x == p; }, xs);
      array greater = filter([&] (value_type x) { return x > p; }, xs);
      array left = array(0);
      array right = array(0);
      par::fork2([&] {
        left = quicksort(less);
      }, [&] {
        right = quicksort(greater);
      });
      result = concat(left, concat(equal, right));
    }
  });
  return result;
}

void merge_seq(const_array_ref xs, array_ref tmp,
               long lo1_xs, long hi1_xs,
               long lo2_xs, long hi2_xs,
               long lo_tmp) {
  long i = lo1_xs;
  long j = lo2_xs;
  long z = lo_tmp;
  
  // merge two halves until one is empty
  while (i < hi1_xs and j < hi2_xs)
    tmp[z++] = (xs[i] < xs[j]) ? xs[i++] : xs[j++];
  
  // copy remaining items
  prim::copy(&xs[0], &tmp[0], i, hi1_xs, z);
  prim::copy(&xs[0], &tmp[0], j, hi2_xs, z);
}

void merge_seq(array_ref xs, array_ref tmp,
               long lo, long mid, long hi) {
  merge_seq(xs, tmp, lo, mid, mid, hi, lo);
  // copy back to source array
  prim::copy(&tmp[0], &xs[0], lo, hi, lo);
}

long lower_bound(const_array_ref xs, long lo, long hi, value_type val) {
  const value_type* first_xs = &xs[0];
  return std::lower_bound(first_xs+lo, first_xs+hi, val)-first_xs;
}

controller_type merge_contr("merge");

void merge_par(const_array_ref xs, array_ref tmp,
               long lo1_xs, long hi1_xs,
               long lo2_xs, long hi2_xs,
               long lo_tmp) {
  long n1 = hi1_xs-lo1_xs;
  long n2 = hi2_xs-lo2_xs;
  auto seq = [&] {
    merge_seq(xs, tmp, lo1_xs, hi1_xs, lo2_xs, hi2_xs, lo_tmp);
  };
  par::cstmt(merge_contr, [&] { return n1+n2; }, [&] {
    if (n1 < n2) {
      // to ensure that the first subarray being sorted is the larger or the two
      merge_par(xs, tmp, lo2_xs, hi2_xs, lo1_xs, hi1_xs, lo_tmp);
    } else if (n1 == 1) {
      if (n2 == 0) {
        // a1 singleton; a2 empty
        tmp[lo_tmp] = xs[lo1_xs];
      } else {
        // both singletons
        tmp[lo_tmp+0] = std::min(xs[lo1_xs], xs[lo2_xs]);
        tmp[lo_tmp+1] = std::max(xs[lo1_xs], xs[lo2_xs]);
      }
    } else {
      // select pivot positions
      long mid1_xs = (lo1_xs+hi1_xs)/2;
      long mid2_xs = lower_bound(xs, lo2_xs, hi2_xs, xs[mid1_xs]);
      // number of items to be treated by the first parallel call
      long k = (mid1_xs-lo1_xs) + (mid2_xs-lo2_xs);
      par::fork2([&] {
        merge_par(xs, tmp, lo1_xs, mid1_xs, lo2_xs, mid2_xs, lo_tmp);
      }, [&] {
        merge_par(xs, tmp, mid1_xs, hi1_xs, mid2_xs, hi2_xs, lo_tmp+k);
      });
    }}, seq);
}

void merge(array_ref xs, array_ref tmp,
           long lo, long mid, long hi) {
  merge_par(xs, tmp, lo, mid, mid, hi, lo);
  // copy back to source array
  prim::pcopy(&tmp[0], &xs[0], lo, hi, lo);
}

controller_type mergesort_contr("mergesort");

void mergesort_par(array_ref xs, array_ref tmp,
                   long lo, long hi) {
  long n = hi-lo;
  auto seq = [&] {
    in_place_sort(xs, lo, hi);
  };
  par::cstmt(mergesort_contr, [&] { return nlogn(n); }, [&] {
    if (n == 0) {
      return;
    } else if (n == 1) {
      tmp[lo] = xs[lo];
    } else {
      long mid = (lo+hi)/2;
      par::fork2([&] {
        mergesort_par(xs, tmp, lo, mid);
      }, [&] {
        mergesort_par(xs, tmp, mid, hi);
      });
      merge(xs, tmp, lo, mid, hi);
    }
  }, seq);
}

array mergesort(const_array_ref xs) {
  array tmp = copy(xs);
  long n = xs.size();
  array scratch = array(n);
  mergesort_par(tmp, scratch, 0l, n);
  return tmp;
}

array seqsort(const_array_ref xs) {
  array tmp = copy(xs);
  in_place_sort(tmp);
  return tmp;
}

/***********************************************************************/

#endif /*! _MINICOURSE_SORT_H_ */