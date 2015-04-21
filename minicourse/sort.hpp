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
#include "exercises.hpp"

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
/* Sequential merge */

void merge_seq(const sparray& xs, const sparray& ys, sparray& tmp,
               long lo_xs, long hi_xs,
               long lo_ys, long hi_ys,
               long lo_tmp) {
  std::merge(&xs[lo_xs], &xs[hi_xs-1]+1, &ys[lo_ys], &ys[hi_ys-1]+1,
             &tmp[lo_tmp]);
}

void merge_seq(sparray& xs, sparray& tmp,
               long lo, long mid, long hi) {
  merge_seq(xs, xs, tmp, lo, mid, mid, hi, lo);
  
  // copy back to source array
  prim::copy(&tmp[0], &xs[0], lo, hi, lo);
}

/*---------------------------------------------------------------------*/
/* Parallel merge */

long lower_bound(const sparray& xs, long lo, long hi, value_type val) {
  const value_type* first_xs = &xs[0];
  return std::lower_bound(first_xs+lo, first_xs+hi, val)-first_xs;
}

controller_type merge_contr("merge");

void merge_par(const sparray& xs, const sparray& ys, sparray& tmp,
               long lo_xs, long hi_xs,
               long lo_ys, long hi_ys,
               long lo_tmp) {
  // later: fill in
}

void merge_par(sparray& xs, sparray& tmp,
               long lo, long mid, long hi) {
  merge_par(xs, xs, tmp, lo, mid, mid, hi, lo);
  // copy back to source array
  prim::pcopy(&tmp[0], &xs[0], lo, hi, lo);
}

sparray merge(const sparray& xs, const sparray& ys) {
  long n = xs.size();
  long m = ys.size();
  sparray tmp = sparray(n + m);
  merge_par(xs, ys, tmp, 0l, n, 0l, m, 0l);
  return tmp;
}

/*---------------------------------------------------------------------*/
/* Parallel mergesort */

controller_type mergesort_contr("mergesort");

template <bool use_parallel_merge>
void mergesort_rec(sparray& xs, sparray& tmp, long lo, long hi) {
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
      mergesort_rec<use_parallel_merge>(xs, tmp, lo, mid);
    }, [&] {
      mergesort_rec<use_parallel_merge>(xs, tmp, mid, hi);
    });
    if (use_parallel_merge)
      merge_par(xs, tmp, lo, mid, hi);
    else
      merge_seq(xs, tmp, lo, mid, hi);
  }, seq);
}

template <bool use_parallel_merge=true>
sparray mergesort(const sparray& xs) {
  long n = xs.size();
  sparray result = copy(xs);
  sparray tmp = sparray(n);
  mergesort_rec<use_parallel_merge>(result, tmp, 0l, n);
  return result;
}


/*---------------------------------------------------------------------*/
/* Parallel mergesort */

controller_type mergesort_ex_contr("mergesort_ex");

void mergesort_ex_rec(sparray& xs, sparray& tmp, long lo, long hi) {
  long n = hi - lo;
  auto seq = [&] {
    in_place_sort(xs, lo, hi);
  };
  par::cstmt(mergesort_ex_contr, [&] { return nlogn(n); }, [&] {
    if (n <= 2) {
      seq();
      return;
    }
    long mid = (lo + hi) / 2;
    par::fork2([&] {
      mergesort_ex_rec(xs, tmp, lo, mid);
    }, [&] {
      mergesort_ex_rec(xs, tmp, mid, hi);
    });
    exercises::merge_par(xs, tmp, lo, mid, hi);
  }, seq);
}

sparray mergesort_ex(const sparray& xs) {
  long n = xs.size();
  sparray result = copy(xs);
  sparray tmp = sparray(n);
  mergesort_ex_rec(result, tmp, 0l, n);
  return result;
}

/*---------------------------------------------------------------------*/
/* Cilksort */

#define INSERTIONSIZE 20

static inline value_type med3(value_type a, value_type b, value_type c)
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
static inline value_type choose_pivot(value_type *low, value_type *high)
{
  return med3(*low, *high, low[(high - low) / 2]);
}

static value_type *seqpart(value_type *low, value_type *high)
{
  value_type pivot;
  value_type h, l;
  value_type *curr_low = low;
  value_type *curr_high = high;
  
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

static void insertion_sort(value_type *low, value_type *high)
{
  value_type *p, *q;
  value_type a, b;
  
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
void seqquick(value_type *low, value_type *high)
{
  value_type *p;
  
  while (high - low >= INSERTIONSIZE) {
    p = seqpart(low, high);
    seqquick(low, p);
    low = p + 1;
  }
  
  insertion_sort(low, high);
}

void wrap_seqquick(value_type *low, long len)
{
  seqquick(low, low + len - 1);
}

void seqmerge(const value_type *low1, const value_type *high1, const value_type *low2, const value_type *high2,
              value_type *lowdest)
{
  value_type a1, a2;
  
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
    memcpy(lowdest, low2, sizeof(value_type) * (high2 - low2 + 1));
  } else {
    memcpy(lowdest, low1, sizeof(value_type) * (high1 - low1 + 1));
  }
}

void wrap_seqmerge(value_type *low1, long len1, value_type* low2, long len2, value_type* dest)
{
  seqmerge(low1, low1 + len1 - 1,
           low2, low2 + len2 - 1, dest);
}

#define swap_indices(a, b) \
{ \
value_type *tmp;\
tmp = a;\
a = b;\
b = tmp;\
}

value_type *binsplit(value_type val, value_type *low, value_type *high)
{
  /*
   * returns index which contains greatest element <= val.  If val is
   * less than all elements, returns low-1
   */
  value_type *mid;
  
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

controller_type cilkmerge_contr("cilkmerge");

void cilkmerge(value_type *low1, value_type *high1, value_type *low2,
               value_type *high2, value_type *lowdest)
{
  /*
   * Cilkmerge: Merges range [low1, high1] with range [low2, high2]
   * into the range [lowdest, ...]
   */
  
  value_type *split1, *split2;	/*
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

  auto seq = [&] {
    seqmerge(low1, high1, low2, high2, lowdest);
  };

  par::cstmt(cilkmerge_contr, [=] { return std::max(0l,(high1-low1)+(high2-low2)); }, [&] {
      
  if (high2 - low2 > high1 - low1) {
    swap_indices(low1, low2);
    swap_indices(high1, high2);
  }
  if (high1 < low1) {
    /* smaller range is empty */
    memcpy(lowdest, low2, sizeof(value_type) * (high2 - low2));
    return;
  }
  if (high2 - low2 < INSERTIONSIZE) {
    seq();
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

  par::fork2([&] {
      cilkmerge(low1, split1 - 1, low2, split2, lowdest);
    }, [&] {
      cilkmerge(split1 + 1, high1, split2 + 1, high2,
                lowdest + lowsize + 2);
    });
  }, seq);

  return;
}

sparray cilkmerge(sparray& xs, sparray& ys) {
  long n = xs.size();
  long m = ys.size();
  sparray tmp = sparray(n + m);
  cilkmerge(&xs[0], &xs[n-1], &ys[0], &ys[m-1], &tmp[0]);
  return tmp;
}

controller_type cilksort_contr("cilksort");

void cilksort(value_type *low, value_type *tmp, long size)
{
  /*
   * divide the input in four parts of the same size (A, B, C, D)
   * Then:
   *   1) recursively sort A, B, C, and D (in parallel)
   *   2) merge A and B into tmp1, and C and D into tmp2 (in parallel)
   *   3) merbe tmp1 and tmp2 into the original array
   */
  long quarter = size / 4;
  value_type *A, *B, *C, *D, *tmpA, *tmpB, *tmpC, *tmpD;

  auto seq = [&] {
    seqquick(low, low + size - 1);
  };

  par::cstmt(cilksort_contr, [=] { return nlogn(size); }, [&] {
  
  if (size < INSERTIONSIZE) {
    seq();
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
  
  par::fork2([&] {
    par::fork2([&] {
      cilksort(A, tmpA, quarter);
    }, [&] {
      cilksort(B, tmpB, quarter);
    });
  }, [&] {
    par::fork2([&] {
      cilksort(C, tmpC, quarter);
    }, [&] {
      cilksort(D, tmpD, size - 3 * quarter);
    });
  });
  
  par::fork2([&] {
    cilkmerge(A, A + quarter - 1, B, B + quarter - 1, tmpA);
  }, [&] {
    cilkmerge(C, C + quarter - 1, D, low + size - 1, tmpC);
  });
  
  cilkmerge(tmpA, tmpC - 1, tmpC, tmpA + size - 1, A);

  }, seq);
}

sparray cilksort(const sparray& xs) {
  long n = xs.size();
  sparray ys = copy(xs);
  sparray tmp = sparray(n);
  if (n > 0)
    cilksort(&ys[0], &tmp[0], n);
  return ys;
}

/***********************************************************************/

#endif /*! _MINICOURSE_SORT_H_ */
