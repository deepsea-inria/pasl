/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file psort.hpp
 * \brief Sorting algorithms
 *
 */

#include "datapar.hpp"

#ifndef _PCTL_PSORT_H_
#define _PCTL_PSORT_H_

namespace pasl {
namespace pctl {
namespace sort {

/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/* Mergesort */
  
namespace {
  
template <class Item>
class merge_chunkedseq_contr {
public:
  static controller_type contr;
};

template <class Item>
controller_type merge_chunkedseq_contr<Item>::contr("merge"+sota<Item>());

template <class Item>
class merge_parray_contr {
public:
  static controller_type contr;
};

template <class Item>
controller_type merge_parray_contr<Item>::contr("merge"+sota<Item>());
  
template <class Item>
using chunkedseq = typename pchunkedseq<Item>::seq_type;
  
template <class Item, class Compare>
chunkedseq<Item> csmerge_seq(chunkedseq<Item>& xs, chunkedseq<Item>& ys, const Compare& compare) {
  chunkedseq<Item> result;
  long n = xs.size();
  long m = ys.size();
  while (true) {
    if (n == 0) {
      result.concat(ys);
      break;
    } else if (m == 0) {
      result.concat(xs);
      break;
    } else {
      Item x = xs.front();
      Item y = ys.front();
      if (compare(x, y)) {
        xs.pop_front();
        result.push_back(x);
        n--;
      } else {
        ys.pop_front();
        result.push_back(y);
        m--;
      }
    }
  }
  return result;
}
  
template <class Item, class Compare>
chunkedseq<Item> csmerge(chunkedseq<Item>& xs, chunkedseq<Item>& ys, const Compare& compare) {
  using controller_type = merge_chunkedseq_contr<Item>;
  long n = xs.size();
  long m = ys.size();
  chunkedseq<Item> result;
  par::cstmt(controller_type::contr, [&] { return n + m; }, [&] {
    if (n < m) {
      result = csmerge<Item>(ys, xs, compare);
    } else if (n == 0) {
      result = { };
    } else if (n == 1) {
      if (m == 0) {
        result.push_back(xs.back());
      } else {
        result.push_back(std::min(xs.back(), ys.back(), compare));
        result.push_back(std::max(xs.back(), ys.back(), compare));
      }
    } else {
      chunkedseq<Item> xs2;
      xs.split((size_t)(n/2), xs2);
      Item mid = xs.back();
      auto pivot = std::lower_bound(ys.begin(), ys.end(), mid, compare);
      chunkedseq<Item> ys2;
      ys.split(pivot, ys2);
      chunkedseq<Item> result2;
      par::fork2([&] {
        result = csmerge<Item>(xs, ys, compare);
      }, [&] {
        result2 = csmerge<Item>(xs2, ys2, compare);
      });
      result.concat(result2);
    }
  }, [&] {
    result = csmerge_seq<Item>(xs, ys, compare);
  });
  return result;
}
  
} // end namespace

template <class Item, class Compare>
pchunkedseq<Item> pcmerge(pchunkedseq<Item>& xs, pchunkedseq<Item>& ys, const Compare& compare) {
  pchunkedseq<Item> result;
  result.seq = csmerge(xs.seq, ys.seq, compare);
  return result;
}
  
namespace {
  
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
  
} // end namespace
  
template <class Item, class Compare>
pchunkedseq<Item> pcmergesort(pchunkedseq<Item>& xs, const Compare& compare) {
  using pchunkedseq_type = pchunkedseq<Item>;
  using seq_type = typename pchunkedseq_type::seq_type;
  using input_type = level4::chunked_sequence_input<seq_type>;
  pchunkedseq_type id;
  input_type in(xs.seq);
  auto combine = [&] (seq_type& xs, seq_type ys) {
    return csmerge<Item>(xs, ys, compare);
  };
  using output_type = level3::mergeable_output<decltype(combine), seq_type>;
  output_type out(combine);
  pchunkedseq_type result;
  auto convert_reduce_comp = [&] (input_type& in) {
    return in.seq.size(); // later: use correct value
  };
  auto convert_reduce = [&] (input_type& in, seq_type& dst) {
    pchunkedseq_type tmp;
    tmp.seq.swap(in.seq);
    tmp = sort_seq(tmp, compare);
    dst.swap(tmp.seq);
  };
  auto seq_convert_reduce = convert_reduce;
  level4::reduce(in, out, id.seq, result.seq, convert_reduce_comp, convert_reduce, seq_convert_reduce);
  return result;
}
  
template <class Item, class Compare>
pchunkedseq<Item> pcsort(pchunkedseq<Item>& xs, const Compare& compare) {
  return mergesort(xs, compare);
}
  
namespace {

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
  using controller_type = merge_parray_contr<Item>;
  long n1 = hi_xs-lo_xs;
  long n2 = hi_ys-lo_ys;
  par::cstmt(controller_type::contr, [&] { return n1+n2; }, [&] {
    if (n1 < n2) {
      // to ensure that the first subarray being sorted is the larger or the two
      merge_par(ys, xs, tmp, lo_ys, hi_ys, lo_xs, hi_xs, lo_tmp, compare);
    } else if (n1 == 0) {
      // xs and ys empty
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
  }, [&] {
    merge_seq(xs, ys, tmp, lo_xs, hi_xs, lo_ys, hi_ys, lo_tmp, compare);
  });
}
  
} // end namespace
  
template <
  class Input_iter,
  class Output_iter,
  class Compare
>
void merge(Input_iter first1,
           Input_iter last1,
           Input_iter first2,
           Input_iter last2,
           Output_iter d_first,
           const Compare& compare) {
  long lo_xs = 0;
  long hi_xs = last1 - first1;
  long lo_ys = 0;
  long hi_ys = last2 - first2;
  long lo_tmp = 0;
  merge_par(first1, first2, d_first, lo_xs, hi_xs, lo_ys, hi_ys, lo_tmp, compare);
}
  
namespace {
  
 
template <class Merge_fct>
class merge_output {
public:
  
  using result_type = std::pair<long, long>;

  Merge_fct merge_fct;
  
  merge_output(const Merge_fct& merge_fct)
  : merge_fct(merge_fct) { }
  
  void init(result_type& rng) const {
    rng = std::make_pair(0L, 0L);
  }
  
  void copy(const result_type& src, result_type& dst) const {
    dst = src;
  }
  
  // dst, src represent left, right range to be merged, respectively
  void merge(const result_type& src, result_type& dst) const {
    assert(dst.second == src.first);
    merge_fct(dst.first, dst.second, src.second);
    dst.second = src.second;
  }
  
};
  
template <class Item, class Compare>
void mergesort_by_reduce(Item* xs, Item* tmp, long lo, long hi, const Compare& compare) {
  using input_type = level4::tabulate_input;
  auto merge_fct = [&] (long lo, long mid, long hi) {
    merge_par(xs, xs, tmp, lo, mid, mid, hi, lo, compare);
    // copy back to source array
    pmem::copy(&tmp[lo], &tmp[hi], &xs[lo]);
  };
  using output_type = merge_output<decltype(merge_fct)>;
  using result_type = typename output_type::result_type;
  input_type in(lo, hi);
  output_type out(merge_fct);
  result_type id = std::make_pair(0L, 0L);
  result_type dst = id;
  auto convert_reduce_comp = [&] (input_type& in) {
    return in.hi - in.lo;
  };
  auto convert_reduce = [&] (input_type& in, result_type& dst) {
    sort_seq(xs, in.lo, in.hi, compare);
    dst = std::make_pair(in.lo, in.hi);
  };
  auto seq_convert_reduce = convert_reduce;
  level4::reduce(in, out, id, dst, convert_reduce_comp, convert_reduce, seq_convert_reduce);
  assert(dst.first == lo);
  assert(dst.second == hi);
}
  
} // end namespace
  
template <class Iter, class Compare>
void mergesort(Iter lo, Iter hi, const Compare& compare) {
  using value_type = typename std::iterator_traits<Iter>::value_type;
  long n = hi - lo;
  parray<value_type> tmp(n);
  mergesort_by_reduce(lo, tmp.begin(), 0L, n, compare);
}
  
template <class Iter, class Compare>
void sort(Iter lo, Iter hi, const Compare& compare) {
  mergesort(lo, hi, compare);
}

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace


#endif /*! _PCTL_PSORT_H_ */