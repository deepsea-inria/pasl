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

#ifndef _PCTL_PCHUNKEDSEQ_BASE_H_
#define _PCTL_PCHUNKEDSEQ_BASE_H_

namespace pasl {
namespace pctl {

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Forward declarations */
  
template <class Item /*, class Alloc = std::allocator<Item>*/ >
class pchunkedseq;

namespace segmented {
  
template <class Iter>
pchunkedseq<value_type_of<Iter>> copy(Iter lo, Iter hi);
  
template <class Iter, class Visit_segment_idx>
void for_each_segmenti(Iter lo, Iter hi, const Visit_segment_idx& visit_segment_idx);
  
template <class Item>
void clear(pchunkedseq<Item>& pc);
  
template <class Item, class Body_comp_rng, class Body_idx_dst>
void tabulate_rng_dst(long n,
                      const Body_comp_rng& body_comp_rng,
                      pchunkedseq<Item>& dst,
                      const Body_idx_dst& body_idx_dst);

template <class Item, class Body_comp, class Body_idx_dst>
void tabulate_dst(long n,
                  const Body_comp& body_comp,
                  pchunkedseq<Item>& dst,
                  const Body_idx_dst& body_idx_dst);

template <class Item, class Body_idx_dst>
void tabulate_dst(long n,
                  pchunkedseq<Item>& dst,
                  const Body_idx_dst& body_idx_dst);
  
template <class Item>
pchunkedseq<Item> fill(long n, const Item& x);
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Main class */
  
template <class Item /*, class Alloc = std::allocator<Item>*/ >
class pchunkedseq {
public:
  
  using seq_type = data::chunkedseq::bootstrapped::deque<Item>;
  
  using value_type = Item;
  using allocator_type = std::allocator<Item>; //Alloc;
  using size_type = std::size_t;
  using ptr_diff = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = typename seq_type::iterator;
  using const_iterator = typename seq_type::const_iterator;
  using segment_type = typename seq_type::segment_type;
  using const_segment_type = typename seq_type::const_segment_type;
  
  seq_type seq;
  
private:
  
  void fill(long n, const value_type& val) {
    if (n == 0) {
      return;
    }
    pchunkedseq<Item> tmp = segmented::fill(n, val);
    swap(tmp);
  }
  
public:
  
  pchunkedseq(long n = 0) {
    value_type val;
    fill(n, val);
  }
  
  pchunkedseq(long n, const value_type& val) {
    fill(n, val);
  }
  
  pchunkedseq(long sz, const std::function<value_type(long)>& body) {
    segmented::tabulate_dst(sz, *this, [&] (long i, reference dst) {
      dst = body(i);
    });
  }
  
  pchunkedseq(long sz,
              const std::function<long(long)>& body_comp,
              const std::function<value_type(long)>& body) {
    segmented::tabulate_dst(sz, *this, body_comp, [&] (long i, reference dst) {
      dst = body(i);
    });
  }
  
  pchunkedseq(std::initializer_list<value_type> xs)
  : seq(xs) { }
  
  ~pchunkedseq() {
    clear();
  }
  
  pchunkedseq(const pchunkedseq& other) {
    pchunkedseq<Item> tmp = segmented::copy(other.cbegin(), other.cend());
    swap(tmp);
  }
  
  pchunkedseq(pchunkedseq&& other)
  : seq(std::move(other.seq)) { }
  
  pchunkedseq& operator=(pchunkedseq&& other) {
    new (&seq) seq_type(std::move(other.seq));
    return *this;
  }
  
  void clear() {
    segmented::clear(*this);
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
  
  void swap(pchunkedseq<Item>& other) {
    seq.swap(other.seq);
  }
  
};

/***********************************************************************/

} // end namespace
} // end namespace

#endif /*! _PCTL_PCHUNKEDSEQ_BASE_H_ */
