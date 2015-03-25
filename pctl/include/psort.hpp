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
  
template <class Item>
using pchunkedseq = pchunkedseq::pchunkedseq<Item>;

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
class merge_contr {
public:
  static controller_type contr;
};

template <class Item>
controller_type merge_contr<Item>::contr("merge"+sota<Item>());

template <class Item>
pchunkedseq<Item> merge(pchunkedseq<Item>& xs, pchunkedseq<Item>& ys) {
  using controller_type = merge_contr<Item>;
  long n = xs.seq.size();
  long m = ys.seq.size();
  pchunkedseq<Item> result;
  par::cstmt(merge_contr<Item>::contr, [&] { return n + m; }, [&] {
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
  });
  return result;
}

template <class Combine, class PChunkedseq>
class merge_container_output {
public:
  
  using result_type = PChunkedseq;
  Combine combine;
  
  merge_container_output(const Combine& combine)
  : combine(combine) { }
  
  void init(PChunkedseq&) const {
    
  }
  
  void merge(PChunkedseq& src, PChunkedseq& dst) const {
    dst = combine(src, dst);
  }
  
};

template <class Item>
pchunkedseq<Item> mergesort(pchunkedseq<Item>& xs) {
  using pchunkedseq_type = pchunkedseq<Item>;
  using chunkedseq_type = typename pchunkedseq_type::seq_type;
  using input_type = level4::chunked_sequence_input<chunkedseq_type>;
  auto combine = [&] (pchunkedseq_type& xs, pchunkedseq_type& ys) {
    return merge(xs, ys);
  };
  using output_type = merge_container_output<typeof(combine), pchunkedseq_type>;
  input_type in(xs.seq);
  output_type out(combine);
  pchunkedseq_type id;
  pchunkedseq_type result;
  auto convert_reduce_comp = [&] (input_type& in) {
    return in.seq.size(); // later: use correct value
  };
  auto convert_reduce = [&] (input_type& in, pchunkedseq_type& dst) {
    in.seq.swap(dst.seq); // later: use more efficient sequential sort?
    std::sort(dst.seq.begin(), dst.seq.end());
  };
  auto seq_convert_reduce = convert_reduce;
  level4::reduce(in, out, id, result, convert_reduce_comp, convert_reduce, seq_convert_reduce);
  return result;
}
  
/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace


#endif /*! _PCTL_PSORT_H_ */