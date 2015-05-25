/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file psort.hpp
 * \brief Sorting algorithms
 *
 */

#include "pchunkedseq.hpp"

#ifndef _PCTL_PSORT_H_
#define _PCTL_PSORT_H_

namespace pasl {
namespace pctl {
namespace sort {

/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/* Mergesort */
  
namespace contr {

template <class Item>
class merge_pchunkedseq {
public:
  static controller_type contr;
};

template <class Item>
controller_type merge_pchunkedseq<Item>::contr("merge"+sota<Item>());

template <class Item>
class merge_parray {
public:
  static controller_type contr;
};

template <class Item>
controller_type merge_parray<Item>::contr("merge"+sota<Item>());
  
template <class Item>
class mergesort_parray {
public:
  static controller_type contr;
};

template <class Item>
controller_type mergesort_parray<Item>::contr("mergesort"+sota<Item>());

} // end namespace

template <class Item, class Compare>
pchunkedseq<Item> merge_seq(pchunkedseq<Item>& xs, pchunkedseq<Item>& ys, const Compare& compare) {
  pchunkedseq<Item> result;
  long n = xs.seq.size();
  long m = ys.seq.size();
  while (true) {
    if (n == 0) {
      result.seq.concat(ys.seq);
      break;
    } else if (m == 0) {
      result.seq.concat(xs.seq);
      break;
    } else {
      Item x = xs.seq.front();
      Item y = ys.seq.front();
      if (compare(x, y)) {
        xs.seq.pop_front();
        result.seq.push_back(x);
        n--;
      } else {
        ys.seq.pop_front();
        result.seq.push_back(y);
        m--;
      }
    }
  }
  return result;
}

template <class Item, class Compare>
pchunkedseq<Item> merge(pchunkedseq<Item>& xs, pchunkedseq<Item>& ys, const Compare& compare) {
  using controller_type = contr::merge_pchunkedseq<Item>;
  long n = xs.seq.size();
  long m = ys.seq.size();
  pchunkedseq<Item> result;
  par::cstmt(controller_type::contr, [&] { return n + m; }, [&] {
    if (n < m) {
      result = merge(ys, xs, compare);
    } else if (n == 1) {
      if (m == 0) {
        result.seq.push_back(xs.seq.back());
      } else {
        result.seq.push_back(std::min(xs.seq.back(), ys.seq.back(), compare));
        result.seq.push_back(std::max(xs.seq.back(), ys.seq.back(), compare));
      }
    } else {
      pchunkedseq<Item> xs2;
      xs.seq.split((size_t)(n/2), xs2.seq);
      Item mid = xs.seq.back();
      auto pivot = std::lower_bound(ys.seq.begin(), ys.seq.end(), mid, compare);
      pchunkedseq<Item> ys2;
      ys.seq.split(pivot, ys2.seq);
      pchunkedseq<Item> result2;
      par::fork2([&] {
        result = merge(xs, ys, compare);
      }, [&] {
        result2 = merge(xs2, ys2, compare);
      });
      result.seq.concat(result2.seq);
    }
  }, [&] {
    result = merge_seq(xs, ys, compare);
  });
  return result;
}
  
template <class Item, class Compare>
void sort_seq(Item* xs, long lo, long hi, const Compare& compare) {
  long n = hi-lo;
  if (n <= 1)
    return;
  std::sort(&xs[lo], &xs[hi-1]+1, compare);
}
  
template <class Item, class Compare>
pchunkedseq<Item> sort_seq(pchunkedseq<Item>& xs, const Compare& compare) {
  pchunkedseq<Item> result;
  long n = xs.seq.size();
  if (n <= 1)
    return result;
  parray<Item> tmp(n);
  xs.seq.backn(&tmp[0], n);
  xs.seq.clear();
  sort_seq(&tmp[0], 0, n, compare);
  result.seq.pushn_back(&tmp[0], n);
  return result;
}
  
template <class Item, class Compare>
pchunkedseq<Item> mergesort(pchunkedseq<Item>& xs, const Compare& compare) {
  using pchunkedseq_type = pchunkedseq<Item>;
  using chunkedseq_type = typename pchunkedseq_type::seq_type;
  using input_type = level4::chunked_sequence_input<chunkedseq_type>;
  pchunkedseq_type id;
  input_type in(xs.seq);
  auto out = level3::create_mergeable_output(id, [&] (pchunkedseq_type& xs, pchunkedseq_type& ys) {
    return merge(xs, ys, compare);
  });
  pchunkedseq_type result;
  auto convert_reduce_comp = [&] (input_type& in) {
    return in.seq.size(); // later: use correct value
  };
  auto convert_reduce = [&] (input_type& in, pchunkedseq_type& dst) {
    /*
    in.seq.swap(dst.seq);
    std::sort(dst.seq.begin(), dst.seq.end());
     */
    pchunkedseq_type tmp;
    tmp.seq.swap(in.seq);
    dst = sort_seq(tmp, compare);
  };
  auto seq_convert_reduce = convert_reduce;
  level4::reduce(in, out, id, result, convert_reduce_comp, convert_reduce, seq_convert_reduce);
  return result;
}

template <class Item>
void copy(const Item* src, Item* dst,
          long lo_src, long hi_src, long lo_dst) {
  if (hi_src-lo_src > 0)
    std::copy(&src[lo_src], &src[hi_src-1]+1, &dst[lo_dst]);
}

template <class Item, class Compare>
void merge_seq(const Item* xs, const Item* ys, Item* tmp,
               long lo_xs, long hi_xs,
               long lo_ys, long hi_ys,
               long lo_tmp,
               const Compare& compare) {
  std::merge(&xs[lo_xs], &xs[hi_xs], &ys[lo_ys], &ys[hi_ys], &tmp[lo_tmp], compare);
}

template <class Item, class Compare>
void merge_seq(Item* xs, Item* tmp, long lo, long mid, long hi, const Compare& compare) {
  merge_seq(xs, xs, tmp, lo, mid, mid, hi, lo, compare);
  // copy back to source array
  copy(&tmp[0], &xs[0], lo, hi, lo);
}
  
template <class Item, class Compare>
long lower_bound(const Item* xs, long lo, long hi, const Item& val, const Compare& compare) {
  const Item* first_xs = &xs[0];
  return std::lower_bound(first_xs+lo, first_xs+hi, val, compare)-first_xs;
}

template <class Item, class Compare>
void merge_par(const Item* xs, const Item* ys, Item* tmp,
               long lo_xs, long hi_xs,
               long lo_ys, long hi_ys,
               long lo_tmp,
               const Compare& compare) {
  using controller_type = contr::merge_parray<Item>;
  long n1 = hi_xs-lo_xs;
  long n2 = hi_ys-lo_ys;
  auto seq = [&] {
    merge_seq(xs, ys, tmp, lo_xs, hi_xs, lo_ys, hi_ys, lo_tmp, compare);
  };
  par::cstmt(controller_type::contr, [&] { return n1+n2; }, [&] {
    if (n1 < n2) {
      // to ensure that the first subarray being sorted is the larger or the two
      merge_par(ys, xs, tmp, lo_ys, hi_ys, lo_xs, hi_xs, lo_tmp, compare);
    } else if (n1 == 1) {
      if (n2 == 0) {
        // xs singleton; ys empty
        tmp[lo_tmp] = xs[lo_xs];
      } else {
        // both singletons
        tmp[lo_tmp+0] = std::min(xs[lo_xs], ys[lo_ys], compare);
        tmp[lo_tmp+1] = std::max(xs[lo_xs], ys[lo_ys], compare);
      }
    } else {
      // select pivot positions
      long mid_xs = (lo_xs+hi_xs)/2;
      long mid_ys = lower_bound(ys, lo_ys, hi_ys, xs[mid_xs], compare);
      // number of items to be treated by the first parallel call
      long k = (mid_xs-lo_xs) + (mid_ys-lo_ys);
      par::fork2([&] {
        merge_par(xs, ys, tmp, lo_xs, mid_xs, lo_ys, mid_ys, lo_tmp, compare);
      }, [&] {
        merge_par(xs, ys, tmp, mid_xs, hi_xs, mid_ys, hi_ys, lo_tmp+k, compare);
      });
    }
  }, seq);
}
 
template <class Item, class Compare>
void merge_par(Item* xs, Item* tmp,
               long lo, long mid, long hi,
               const Compare& compare) {
  merge_par(xs, xs, tmp, lo, mid, mid, hi, lo, compare);
  // copy back to source array
  pmem::copy(&tmp[lo], &tmp[hi], &xs[lo]);
}
  
template <class Item, class Compare>
void merge(typename parray<Item>::const_iterator first1,
           typename parray<Item>::const_iterator last1,
           typename parray<Item>::const_iterator first2,
           typename parray<Item>::const_iterator last2,
           typename parray<Item>::iterator d_first,
           const Compare& compare) {
  long lo_xs = 0;
  long hi_xs = last1 - first1;
  long lo_ys = 0;
  long hi_ys = last2 - first2;
  long lo_tmp = 0;
  merge_par(first1, first2, d_first, lo_xs, hi_xs, lo_ys, hi_ys, lo_tmp, compare);
}
  
// later: encode this function as an application of the reduce function
template <class Item, class Compare>
void mergesort_rec(Item* xs, Item* tmp, long lo, long hi, const Compare& compare) {
  using controller_type = contr::mergesort_parray<Item>;
  long n = hi - lo;
  auto seq = [&] {
    sort_seq(xs, lo, hi, compare);
  };
  par::cstmt(controller_type::contr, [&] { return n; }, [&] {
    if (n <= 2) {
      seq();
      return;
    }
    long mid = (lo + hi) / 2;
    par::fork2([&] {
      mergesort_rec(xs, tmp, lo, mid, compare);
    }, [&] {
      mergesort_rec(xs, tmp, mid, hi, compare);
    });
    merge_par(xs, tmp, lo, mid, hi, compare);
  }, seq);
}
  
template <class Item, class Compare>
void mergesort(parray<Item>& xs, const Compare& compare) {
  long n = xs.size();
  if (n == 0)
    return;
  parray<Item> tmp(n);
  mergesort_rec(&xs[0], &tmp[0], 0, n, compare);
}

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace


#endif /*! _PCTL_PSORT_H_ */