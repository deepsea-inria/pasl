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
  
template <class Item>
using pchunkedseq = pchunkedseq::pchunkedseq<Item>;
  
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

template <class Item>
pchunkedseq<Item> merge_seq(pchunkedseq<Item>& xs, pchunkedseq<Item>& ys) {
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
      if (x <= y) {
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

template <class Item>
pchunkedseq<Item> merge(pchunkedseq<Item>& xs, pchunkedseq<Item>& ys) {
  using controller_type = contr::merge_pchunkedseq<Item>;
  long n = xs.seq.size();
  long m = ys.seq.size();
  pchunkedseq<Item> result;
  par::cstmt(controller_type::contr, [&] { return n + m; }, [&] {
    if (n < m) {
      result = merge(ys, xs);
    } else if (n == 1) {
      if (m == 0) {
        result.seq.push_back(xs.seq.back());
      } else {
        result.seq.push_back(std::min(xs.seq.back(), ys.seq.back()));
        result.seq.push_back(std::max(xs.seq.back(), ys.seq.back()));
      }
    } else {
      pchunkedseq<Item> xs2;
      xs.seq.split((size_t)(n/2), xs2.seq);
      Item mid = xs.seq.back();
      auto pivot = std::lower_bound(ys.seq.begin(), ys.seq.end(), mid);
      pchunkedseq<Item> ys2;
      ys.seq.split(pivot, ys2.seq);
      pchunkedseq<Item> result2;
      par::fork2([&] {
        result = merge(xs, ys);
      }, [&] {
        result2 = merge(xs2, ys2);
      });
      result.seq.concat(result2.seq);
    }
  }, [&] {
    result = merge_seq(xs, ys);
  });
  return result;
}
  
template <class Item>
void sort_seq(Item* xs, long lo, long hi) {
  long n = hi-lo;
  if (n <= 1)
    return;
  std::sort(&xs[lo], &xs[hi-1]+1);
}
  
template <class Item>
pchunkedseq<Item> sort_seq(pchunkedseq<Item>& xs) {
  pchunkedseq<Item> result;
  long n = xs.seq.size();
  if (n <= 1)
    return result;
  parray::parray<Item> tmp(n);
  xs.seq.backn(&tmp[0], n);
  xs.seq.clear();
  sort_seq(&tmp[0], 0, n);
  result.seq.pushn_back(&tmp[0], n);
  return result;
}
  
template <class Item>
pchunkedseq<Item> mergesort(pchunkedseq<Item>& xs) {
  using pchunkedseq_type = pchunkedseq<Item>;
  using chunkedseq_type = typename pchunkedseq_type::seq_type;
  using input_type = level4::chunked_sequence_input<chunkedseq_type>;
  pchunkedseq_type id;
  input_type in(xs.seq);
  auto out = level3::create_mergeable_output(id, [&] (pchunkedseq_type& xs, pchunkedseq_type& ys) {
    return merge(xs, ys);
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
    dst = sort_seq(tmp);
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

template <class Item>
void merge_seq(const Item* xs, const Item* ys, Item* tmp,
               long lo_xs, long hi_xs,
               long lo_ys, long hi_ys,
               long lo_tmp) {
  std::merge(&xs[lo_xs], &xs[hi_xs], &ys[lo_ys], &ys[hi_ys], &tmp[lo_tmp]);
}

template <class Item>
void merge_seq(Item* xs, Item* tmp, long lo, long mid, long hi) {
  merge_seq(xs, xs, tmp, lo, mid, mid, hi, lo);
  // copy back to source array
  copy(&tmp[0], &xs[0], lo, hi, lo);
}
  
template <class Item>
long lower_bound(const Item* xs, long lo, long hi, const Item& val) {
  const Item* first_xs = &xs[0];
  return std::lower_bound(first_xs+lo, first_xs+hi, val)-first_xs;
}

template <class Item>
void merge_par(const Item* xs, const Item* ys, Item* tmp,
               long lo_xs, long hi_xs,
               long lo_ys, long hi_ys,
               long lo_tmp) {
  using controller_type = contr::merge_parray<Item>;
  long n1 = hi_xs-lo_xs;
  long n2 = hi_ys-lo_ys;
  auto seq = [&] {
    merge_seq(xs, ys, tmp, lo_xs, hi_xs, lo_ys, hi_ys, lo_tmp);
  };
  par::cstmt(controller_type::contr, [&] { return n1+n2; }, [&] {
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
 
template <class Item>
void merge_par(Item* xs, Item* tmp,
               long lo, long mid, long hi) {
  merge_par(xs, xs, tmp, lo, mid, mid, hi, lo);
  // copy back to source array
  pmem::copy(&tmp[lo], &tmp[hi], &xs[lo]);
}
  
long nlogn(long n) {
  return pasl::data::estimator::annotation::nlgn(n);
}
  
// later: encode this function as an application of the reduce function
template <class Item>
void mergesort_rec(Item* xs, Item* tmp, long lo, long hi) {
  using controller_type = contr::mergesort_parray<Item>;
  long n = hi - lo;
  auto seq = [&] {
    sort_seq(xs, lo, hi);
  };
  par::cstmt(controller_type::contr, [&] { return nlogn(n); }, [&] {
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
  
template <class Item>
void mergesort(parray::parray<Item>& xs) {
  long n = xs.size();
  if (n == 0)
    return;
  parray::parray<Item> tmp(n);
  mergesort_rec(&xs[0], &tmp[0], 0, n);
}

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace


#endif /*! _PCTL_PSORT_H_ */