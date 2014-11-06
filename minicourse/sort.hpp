/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file sort.hpp
 * \brief Sorting algorithms
 *
 */

#include "native.hpp"
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
  array tmp = slice(xs, lo, hi);
  in_place_sort(tmp);
  return tmp;
}

/*---------------------------------------------------------------------*/
/* Parallel quicksort */

controller_type quicksort_contr("quicksort");

array quicksort(const_array_ref xs) {
  long n = xs.size();
  array result = { };
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
      array left = { };
      array right = { };
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
/* Sequential merge */

void merge_seq(const_array_ref xs, const_array_ref ys, array_ref tmp,
               long lo_xs, long hi_xs,
               long lo_ys, long hi_ys,
               long lo_tmp) {
  std::merge(&xs[lo_xs], &xs[hi_xs-1]+1, &ys[lo_ys], &ys[hi_ys-1]+1,
             &tmp[lo_tmp]);
}

void merge_seq(array_ref xs, array_ref tmp,
               long lo, long mid, long hi) {
  merge_seq(xs, xs, tmp, lo, mid, mid, hi, lo);
  
  // copy back to source array
  prim::copy(&tmp[0], &xs[0], lo, hi, lo);
}

/*---------------------------------------------------------------------*/
/* Parallel merge */

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
        // xs singleton; ys empty
        tmp[lo_tmp] = xs[lo_xs];
      } else {
        // both singletons
        tmp[lo_tmp+0] = std::min(xs[lo_xs], ys[lo_ys]);
        tmp[lo_tmp+1] = std::max(xs[lo_xs], ys[lo_ys]);
      }
    } else {
      // select pivot positions
      long mid_xs = (lo_xs+hi_xs)/2;
      long mid_ys = lower_bound(ys, lo_ys, hi_ys, xs[mid_xs]);
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

void merge_par(array_ref xs, array_ref tmp,
               long lo, long mid, long hi) {
  merge_par(xs, xs, tmp, lo, mid, mid, hi, lo);

  // copy back to source array
  prim::pcopy(&tmp[0], &xs[0], lo, hi, lo);
}

array merge(const_array_ref xs, const_array_ref ys) {
  long n = xs.size();
  long m = ys.size();
  array tmp = array(n + m);
  merge_par(xs, ys, tmp, 0l, n, 0l, m, 0l);
  return tmp;
}

/*---------------------------------------------------------------------*/
/* Parallel mergesort */

controller_type mergesort_contr("mergesort");

void mergesort_rec(array_ref xs, array_ref tmp, long lo, long hi) {
  long n = hi - lo;
  auto seq = [&] {
    in_place_sort(xs, lo, hi);
  };
  par::cstmt(mergesort_contr, [&] { return nlogn(n); }, [&] {
    if (n <= 2) {
      seq();
      return;
    }

    long mid = (lo + hi) / 2;
    par::fork2([&] {
      mergesort_rec(xs, tmp, lo, mid);
    }, [&] {
      mergesort_rec(xs, tmp, mid, hi);
    });

    merge_par(xs, tmp, lo, mid, hi);
  }, seq);
}

array mergesort(const_array_ref xs) {
  long n = xs.size();
  array result = copy(xs);
  array tmp = array(n);
  mergesort_rec(result, tmp, 0l, n);
  return result;
}

/*---------------------------------------------------------------------*/
/* Cilksort */

#define KILO 1024
#define MERGESIZE (2*KILO)
#define QUICKSIZE (2*KILO)
#define INSERTIONSIZE 20

using ELM = value_type;

static inline ELM med3(ELM a, ELM b, ELM c)
{
  if (a < b) {
    if (b < c) {
      return b;
    } else {
      if (a < c)
        return c;
      else
        return a;
    }
  } else {
    if (b > c) {
      return b;
    } else {
      if (a > c)
        return c;
      else
        return a;
    }
  }
}

/*
 * simple approach for now; a better median-finding
 * may be preferable
 */
static inline ELM choose_pivot(ELM *low, ELM *high)
{
  return med3(*low, *high, low[(high - low) / 2]);
}

static ELM *seqpart(ELM *low, ELM *high)
{
  ELM pivot;
  ELM h, l;
  ELM *curr_low = low;
  ELM *curr_high = high;
  
  pivot = choose_pivot(low, high);
  
  while (1) {
    while ((h = *curr_high) > pivot)
      curr_high--;
    
    while ((l = *curr_low) < pivot)
      curr_low++;
    
    if (curr_low >= curr_high)
      break;
    
    *curr_high-- = l;
    *curr_low++ = h;
  }
  
  /*
   * I don't know if this is really necessary.
   * The problem is that the pivot is not always the
   * first element, and the partition may be trivial.
   * However, if the partition is trivial, then
   * *high is the largest element, whence the following
   * code.
   */
  if (curr_high < high)
    return curr_high;
  else
    return curr_high - 1;
}

static void insertion_sort(ELM *low, ELM *high)
{
  ELM *p, *q;
  ELM a, b;
  
  for (q = low + 1; q <= high; ++q) {
    a = q[0];
    for (p = q - 1; p >= low && (b = p[0]) > a; p--)
      p[1] = b;
    p[1] = a;
  }
}

/*
 * tail-recursive quicksort, almost unrecognizable :-)
 */
void seqquick(ELM *low, ELM *high)
{
  ELM *p;
  
  while (high - low >= INSERTIONSIZE) {
    p = seqpart(low, high);
    seqquick(low, p);
    low = p + 1;
  }
  
  insertion_sort(low, high);
}

void wrap_seqquick(ELM *low, long len)
{
  seqquick(low, low + len - 1);
}

void seqmerge(ELM *low1, ELM *high1, ELM *low2, ELM *high2,
              ELM *lowdest)
{
  ELM a1, a2;
  
  /*
   * The following 'if' statement is not necessary
   * for the correctness of the algorithm, and is
   * in fact subsumed by the rest of the function.
   * However, it is a few percent faster.  Here is why.
   *
   * The merging loop below has something like
   *   if (a1 < a2) {
   *        *dest++ = a1;
   *        ++low1;
   *        if (end of array) break;
   *        a1 = *low1;
   *   }
   *
   * Now, a1 is needed immediately in the next iteration
   * and there is no way to mask the latency of the load.
   * A better approach is to load a1 *before* the end-of-array
   * check; the problem is that we may be speculatively
   * loading an element out of range.  While this is
   * probably not a problem in practice, yet I don't feel
   * comfortable with an incorrect algorithm.  Therefore,
   * I use the 'fast' loop on the array (except for the last
   * element) and the 'slow' loop for the rest, saving both
   * performance and correctness.
   */
  
  if (low1 < high1 && low2 < high2) {
    a1 = *low1;
    a2 = *low2;
    for (;;) {
      if (a1 < a2) {
        *lowdest++ = a1;
        a1 = *++low1;
        if (low1 >= high1)
          break;
      } else {
        *lowdest++ = a2;
        a2 = *++low2;
        if (low2 >= high2)
          break;
      }
    }
  }
  if (low1 <= high1 && low2 <= high2) {
    a1 = *low1;
    a2 = *low2;
    for (;;) {
      if (a1 < a2) {
        *lowdest++ = a1;
        ++low1;
        if (low1 > high1)
          break;
        a1 = *low1;
      } else {
        *lowdest++ = a2;
        ++low2;
        if (low2 > high2)
          break;
        a2 = *low2;
      }
    }
  }
  if (low1 > high1) {
    memcpy(lowdest, low2, sizeof(ELM) * (high2 - low2 + 1));
  } else {
    memcpy(lowdest, low1, sizeof(ELM) * (high1 - low1 + 1));
  }
}

void wrap_seqmerge(ELM *low1, long len1, ELM* low2, long len2, ELM* dest)
{
  seqmerge(low1, low1 + len1 - 1,
           low2, low2 + len2 - 1, dest);
}

#define swap_indices(a, b) \
{ \
ELM *tmp;\
tmp = a;\
a = b;\
b = tmp;\
}

ELM *binsplit(ELM val, ELM *low, ELM *high)
{
  /*
   * returns index which contains greatest element <= val.  If val is
   * less than all elements, returns low-1
   */
  ELM *mid;
  
  while (low != high) {
    mid = low + ((high - low + 1) >> 1);
    if (val <= *mid)
      high = mid - 1;
    else
      low = mid;
  }
  
  if (*low > val)
    return low - 1;
  else
    return low;
}

void cilkmerge(ELM *low1, ELM *high1, ELM *low2,
               ELM *high2, ELM *lowdest)
{
  /*
   * Cilkmerge: Merges range [low1, high1] with range [low2, high2]
   * into the range [lowdest, ...]
   */
  
  ELM *split1, *split2;	/*
                         * where each of the ranges are broken for
                         * recursive merge
                         */
  long int lowsize;		/*
                       * total size of lower halves of two
                       * ranges - 2
                       */
  
  /*
   * We want to take the middle element (indexed by split1) from the
   * larger of the two arrays.  The following code assumes that split1
   * is taken from range [low1, high1].  So if [low1, high1] is
   * actually the smaller range, we should swap it with [low2, high2]
   */
  
  if (high2 - low2 > high1 - low1) {
    swap_indices(low1, low2);
    swap_indices(high1, high2);
  }
  if (high1 < low1) {
    /* smaller range is empty */
    memcpy(lowdest, low2, sizeof(ELM) * (high2 - low2));
    return;
  }
  if (high2 - low2 < MERGESIZE) {
    seqmerge(low1, high1, low2, high2, lowdest);
    return;
  }
  /*
   * Basic approach: Find the middle element of one range (indexed by
   * split1). Find where this element would fit in the other range
   * (indexed by split 2). Then merge the two lower halves and the two
   * upper halves.
   */
  
  split1 = ((high1 - low1 + 1) / 2) + low1;
  split2 = binsplit(*split1, low2, high2);
  lowsize = split1 - low1 + split2 - low2;
  
  /*
   * directly put the splitting element into
   * the appropriate location
   */
  *(lowdest + lowsize + 1) = *split1;
  pasl::sched::native::fork2([&] {
    cilkmerge(low1, split1 - 1, low2, split2, lowdest);
  }, [&] {
    cilkmerge(split1 + 1, high1, split2 + 1, high2,
              lowdest + lowsize + 2);
  });

  return;
}

void cilksort(ELM *low, ELM *tmp, long size)
{
  /*
   * divide the input in four parts of the same size (A, B, C, D)
   * Then:
   *   1) recursively sort A, B, C, and D (in parallel)
   *   2) merge A and B into tmp1, and C and D into tmp2 (in parallel)
   *   3) merbe tmp1 and tmp2 into the original array
   */
  long quarter = size / 4;
  ELM *A, *B, *C, *D, *tmpA, *tmpB, *tmpC, *tmpD;
  
  if (size < QUICKSIZE) {
    /* quicksort when less than 1024 elements */
    seqquick(low, low + size - 1);
    return;
  }
  A = low;
  tmpA = tmp;
  B = A + quarter;
  tmpB = tmpA + quarter;
  C = B + quarter;
  tmpC = tmpB + quarter;
  D = C + quarter;
  tmpD = tmpC + quarter;
  
  pasl::sched::native::fork2([&] {
    pasl::sched::native::fork2([&] {
      cilksort(A, tmpA, quarter);
    }, [&] {
      cilksort(B, tmpB, quarter);
    });
  }, [&] {
    pasl::sched::native::fork2([&] {
      cilksort(C, tmpC, quarter);
    }, [&] {
      cilksort(D, tmpD, size - 3 * quarter);
    });
  });
  
  pasl::sched::native::fork2([&] {
    cilkmerge(A, A + quarter - 1, B, B + quarter - 1, tmpA);
  }, [&] {
    cilkmerge(C, C + quarter - 1, D, low + size - 1, tmpC);
  });
  
  cilkmerge(tmpA, tmpC - 1, tmpC, tmpA + size - 1, A);
}

array cilksort(const_array_ref xs) {
  long n = xs.size();
  array ys = copy(xs);
  array tmp = array(n);
  if (n > 0)
    cilksort(&ys[0], &tmp[0], n);
  return ys;
}

/***********************************************************************/

#endif /*! _MINICOURSE_SORT_H_ */
