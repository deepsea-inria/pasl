/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file pchunkedseq.hpp
 * \brief Parallel chunked sequence data structure
 *
 */

#include <cmath>

#include "parray.hpp"
#include "chunkedseq.hpp"
#include "datapar.hpp"

#ifndef _PCTL_PCHUNKEDSEQ_H_
#define _PCTL_PCHUNKEDSEQ_H_

namespace pasl {
namespace pctl {
namespace pchunkedseq {

/***********************************************************************/
 
template <class Item, class Alloc = std::allocator<Item>>
class pchunkedseq {
public:
  
  using value_type = Item;
  using allocator_type = Alloc;
  using size_type = std::size_t;
  using ptr_diff = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using seq_type = data::chunkedseq::bootstrapped::deque<Item>;
  
  seq_type seq;
  
private:
  
  template <class Body_comp_rng, class Body_idx_dst>
  static void tabulate_rng_dst(long n,
                        const Body_comp_rng& body_comp_rng,
                        seq_type& dst,
                        const Body_idx_dst& body_idx_dst) {
    using input_type = level4::tabulate_input<Body_idx_dst>;
    using output_type = level3::chunkedseq_output<seq_type>;
    input_type in(0, n, body_idx_dst);
    output_type out;
    seq_type id;
    auto convert_comp = [&] (input_type& in) {
      return body_comp_rng(in.lo, in.hi);
    };
    long chunk_capacity = dst.chunk_capacity;
    parray::parray<value_type> tmp(chunk_capacity);
    auto convert = [&] (input_type& in, seq_type dst) {
      dst.stream_pushn_back([&] (long i, long n) {
        for (long k = 0; k < n; k++) {
          body_idx_dst(k + in.lo, tmp[k]);
        }
        const value_type* lo = &tmp[0];
        const value_type* hi = &tmp[n];
        return std::make_pair(lo, hi);
      }, in.hi - in.lo);
    };
    level4::reduce(in, out, id, dst, convert_comp, convert, convert);
  }
  
  template <class Body_comp, class Body_idx_dst>
  static void tabulate_dst(long n,
                    const Body_comp& body_comp,
                    seq_type& dst,
                    const Body_idx_dst& body_idx_dst) {
    parray::parray<long> w = weights(n, [&] (long i) {
      return body_comp(i);
    });
    auto body_comp_rng = [&] (long lo, long hi) {
      return w[hi] - w[lo];
    };
    tabulate_rng_dst(n, body_comp_rng, dst, body_idx_dst);
  }
  
  template <class Body_idx_dst>
  static void tabulate_dst(long n,
                    seq_type& dst,
                    const Body_idx_dst& body_idx_dst) {
    auto body_comp_rng = [&] (long lo, long hi) {
      return hi - lo;
    };
    tabulate_rng_dst(n, body_comp_rng, dst, body_idx_dst);
  }
  
  static void copy(iterator lo, iterator hi, seq_type& dst) {
    using output_type = level3::chunkedseq_output<seq_type>;
    output_type out;
    assert(false);
  }
  
public:
  
  pchunkedseq(long sz = 0) {
    
  }
  
  pchunkedseq(long sz, const value_type& val) {
    
  }
  
  pchunkedseq(long sz, const std::function<value_type(long)>& body) {
    tabulate_dst(sz, seq, [&] (long i, reference dst) {
      dst = body(i);
    });
  }
  
  pchunkedseq(long sz,
              const std::function<long(long)>& body_comp,
              const std::function<value_type(long)>& body) {
    tabulate_dst(sz, seq, body_comp, [&] (long i, reference dst) {
      dst = body(i);
    });
  }
  
  pchunkedseq(std::initializer_list<value_type> xs)
  : seq(xs) { }
  
  ~pchunkedseq() {
    clear();
  }
  
  pchunkedseq(const pchunkedseq& other) {

  }
  
  pchunkedseq& operator=(pchunkedseq&& other) {
    seq = std::move(other.seq);
    return *this;
  }
  
  void clear() {
    using input_type = level4::chunked_sequence_input<seq_type>;
    using output_type = level3::trivial_output<int>;
    input_type in(seq);
    output_type out;
    auto convert_comp = [&] (const input_type& in) {
      return in.seq.size();
    };
    auto convert =  [&] (input_type& in, int&) {
      in.seq.clear();
    };
    int dummy;
    level4::reduce(in, out, dummy, dummy, convert_comp, convert, convert);
  }
  
  iterator begin() const {
    return seq.begin();
  }
  
  const_iterator cbegin() const {
    return seq.cbegin();
  }
  
  iterator end() const {
    return seq.end();
  }
  
  const_iterator cend() const {
    return seq.cend();
  }
  
};

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif /*! _PCTL_PCHUNKEDSEQ_H_ */
