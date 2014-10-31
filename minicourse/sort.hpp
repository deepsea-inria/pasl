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

/*---------------------------------------------------------------------*/
/* Sequential sort */

void in_place_sort(array_ref xs, long lo, long hi) {
  if (hi-lo > 0)
    std::sort(&xs[lo], &xs[hi-1]+1);
}

void in_place_sort(array_ref xs) {
  in_place_sort(xs, 0l, xs.size());
}

array seqsort(const_array_ref xs) {
  array tmp = copy(xs);
  in_place_sort(tmp);
  return tmp;
}

array seqsort(const_array_ref xs, long lo, long hi) {
  long n = hi-lo;
  array tmp = tabulate([&] (long i) { return xs[lo+i]; }, n);
  in_place_sort(tmp);
  return tmp;
}

/*---------------------------------------------------------------------*/
/* Parallel quicksort */

controller_type quicksort_contr("quicksort");

array quicksort(const_array_ref xs) {
  long n = xs.size();
  array result = empty();
  auto seq = [&] {
    result = seqsort(xs);
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
      array left = empty();
      array right = empty();
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

void merge_seq(const_array_ref xs, const_array_ref ys, array_ref tmp,
               long lo_xs, long hi_xs,
               long lo_ys, long hi_ys,
               long lo_tmp) {
  long i = lo_xs;
  long j = lo_ys;
  long z = lo_tmp;
  
  // merge two halves until one is empty
  while (i < hi_xs and j < hi_ys)
    tmp[z++] = (xs[i] < ys[j]) ? xs[i++] : ys[j++];
  
  // copy remaining items
  prim::copy(&xs[0], &tmp[0], i, hi_xs, z);
  prim::copy(&ys[0], &tmp[0], j, hi_ys, z);
}

void merge_seq(array_ref xs, array_ref tmp,
               long lo, long mid, long hi) {
  merge_seq(xs, xs, tmp, lo, mid, mid, hi, lo);
  // copy back to source array
  prim::copy(&tmp[0], &xs[0], lo, hi, lo);
}

long lower_bound(const_array_ref xs, long lo, long hi, value_type val) {
  const value_type* first_xs = &xs[0];
  return std::lower_bound(first_xs+lo, first_xs+hi, val)-first_xs;
}

controller_type merge_contr("merge");

void merge_par(const_array_ref xs, const_array_ref ys, array_ref tmp,
               long lo_xs, long hi_xs,
               long lo_ys, long hi_ys,
               long lo_tmp) {
  long n1 = hi_xs-lo_xs;
  long n2 = hi_ys-lo_ys;
  auto seq = [&] {
    merge_seq(xs, ys, tmp, lo_xs, hi_xs, lo_ys, hi_ys, lo_tmp);
  };
  par::cstmt(merge_contr, [&] { return n1+n2; }, [&] {
    if (n1 < n2) {
      // to ensure that the first subarray being sorted is the larger or the two
      merge_par(ys, xs, tmp, lo_ys, hi_ys, lo_xs, hi_xs, lo_tmp);
    } else if (n1 == 1) {
      if (n2 == 0) {
        // a1 singleton; a2 empty
        tmp[lo_tmp] = xs[lo_xs];
      } else {
        // both singletons
        tmp[lo_tmp+0] = std::min(xs[lo_xs], xs[lo_ys]);
        tmp[lo_tmp+1] = std::max(xs[lo_xs], xs[lo_ys]);
      }
    } else {
      // select pivot positions
      long mid_xs = (lo_xs+hi_xs)/2;
      long mid_ys = lower_bound(xs, lo_ys, hi_ys, xs[mid_xs]);
      // number of items to be treated by the first parallel call
      long k = (mid_xs-lo_xs) + (mid_ys-lo_ys);
      par::fork2([&] {
        merge_par(xs, ys, tmp, lo_xs, mid_xs, lo_ys, mid_ys, lo_tmp);
      }, [&] {
        merge_par(xs, ys, tmp, mid_xs, hi_xs, mid_ys, hi_ys, lo_tmp+k);
      });
    }
  }, seq);
}

array merge(const_array_ref xs, const_array_ref ys) {
  long n = xs.size();
  long m = ys.size();
  array tmp = array(n + m);
  merge_par(xs, ys, tmp, 0l, n, 0l, m, 0l);
  return tmp;
}

void merge(array_ref xs, array_ref tmp,
           long lo, long mid, long hi) {
  merge_par(xs, xs, tmp, lo, mid, mid, hi, lo);
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
  par::cstmt(mergesort_contr, [n] { return nlogn(n); }, [&] {
    if (n == 0) {
      
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

#if defined(USE_OLD_MERGESORT)
array mergesort(const_array_ref xs) {
  array tmp = copy(xs);
  long n = xs.size();
  array scratch = array(n);
  mergesort_par(tmp, scratch, 0l, n);
  return tmp;
}

#else

array mergesort_rec(const_array_ref xs, long lo, long hi) {
  long n = hi-lo;
  array result;
  auto seq = [&] {
    result = seqsort(xs, lo, hi);
  };
  par::cstmt(mergesort_contr, [n] { return nlogn(n); }, [&] {
    if (n < 2) {
      seq();
    } else {
      long mid = (lo+hi)/2;
      array a,b;
      par::fork2([&] {
        a = mergesort_rec(xs, lo, mid);
      }, [&] {
        b = mergesort_rec(xs, mid, hi);
      });
      result = merge(a, b);
    }
  }, seq);
  return result;
}

array mergesort(const_array_ref xs) {
  return mergesort_rec(xs, 0l, xs.size());
}

#endif

/***********************************************************************/

#endif /*! _MINICOURSE_SORT_H_ */