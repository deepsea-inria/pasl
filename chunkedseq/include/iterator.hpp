/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief STL style iterators for our chunked sequences
 * \file iterator.hpp
 *
 */

#include <assert.h>
#include <iterator>

#include "itemsearch.hpp"

#ifndef _PASL_DATA_ITERATOR_H_
#define _PASL_DATA_ITERATOR_H_

namespace pasl {
namespace data {
namespace chunkedseq {
namespace iterator {
    
/***********************************************************************/
  
using position_type = enum { begin, end };

/*---------------------------------------------------------------------*/
//! [bidirectional]
/*!
 *  \class bidirectional
 *  \ingroup chunkedseq
 *  \brief Bi-directional iterator
 *
 * Implements the BidirectionalIterator category of the Standard
 * Template Library.
 *
 */
template <class Configuration>
class bidirectional {
private:
  
  using config_type = Configuration;
  using chunk_pointer = const typename config_type::chunk_type*;
  using segment_type = typename config_type::segment_type;
  
public:
  
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = typename config_type::value_type;
  using difference_type = typename config_type::difference_type;
  using pointer = value_type*;
  using reference = value_type&;
  using self_type = bidirectional<config_type>;
  
private:
  
  chunk_pointer cur;
  segment_type seg;
  
  self_type& increment() {
    assert(false); // todo
    return *this;
  }
  
  self_type& decrement() {
    assert(false); // todo
    return *this;
  }
  
public:
  
  bidirectional(chunk_pointer p)
  : cur(p) {
    assert(false); // todo
  }
  
  bidirectional() {
    assert(false); // todo
  }
  
  /*---------------------------------------------------------------------*/
  /** @name ForwardIterator
   */
  ///@{
  
  bool operator==(const self_type& other) const {
    return seg.middle == other.seg.middle
        && seg.end == other.seg.end;
  }
  
  bool operator!=(const self_type& other) const {
    return ! (*this == other);
  }
  
  reference operator*() const {
    return *seg.middle;
  }
  
  self_type& operator++() {
    return increment();
  }
  
  self_type operator++(int) {
    return increment();
    
  }
  
  self_type& operator--() {
    return decrement();
  }
  
  self_type operator--(int) {
    return decrement();
  }
  
  ///@}
  
  segment_type get_segment() const {
    return seg;
  }
  
};
  
/*---------------------------------------------------------------------*/
//! [random_access]
/*!
 *  \class random_access
 *  \ingroup chunkedseq
 *  \brief Random-access iterator
 *
 * Implements the RandomAccessIterator category of the Standard Template
 * Library.
 *
 */
template <class Chunkedseq, class Configuration>
class random_access {
private:
  
  using chunkedseq_type = Chunkedseq;
  using config_type = Configuration;
  using const_chunkedseq_pointer = const chunkedseq_type*;
  using const_chunk_pointer = const typename config_type::chunk_type*;
  
public:
  
  /*---------------------------------------------------------------------*/
  /** @name STL-specific configuration types
   */
  ///@{
  using iterator_category = std::random_access_iterator_tag;
  using size_type = typename config_type::size_type;
  using difference_type = typename config_type::difference_type;
  ///@}
  
  /*---------------------------------------------------------------------*/
  /** @name Container-specific types
   */
  ///@{
  using self_type = random_access<chunkedseq_type, config_type>;
  using value_type = typename config_type::value_type;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using reference = value_type&;
  using const_reference = const value_type&;
  using segment_type = typename config_type::segment_type;
  ///@}
  
  /*---------------------------------------------------------------------*/
  /** @name Cached-measurement types
   */
  ///@{
  using cache_type = typename config_type::middle_cache_type;
  using measured_type = typename cache_type::measured_type;
  using algebra_type = typename cache_type::algebra_type;
  using measure_type = typename cache_type::measure_type;
  ///@}
  
  /*---------------------------------------------------------------------*/
  
private:

  using chunk_search_type = typename config_type::chunk_search_type;
  
  using size_access = typename config_type::size_access;
  
  const_chunkedseq_pointer seq;
  const_chunk_pointer cur;
  segment_type seg;

  measure_type meas_fct;
  
  /*---------------------------------------------------------------------*/
  
  void check() {
#ifndef NDEBUG
    size_type sz = size();
    size_type ssz = seq->size();
    if (sz > ssz + 1)
      std::cout << "error" << std::endl;
    sz = size();  
    assert(sz <= ssz + 1);
    assert(sz >= 0);
#endif
  }

  template <class Pred>
  measured_type chunk_search_by(const Pred& p, measured_type prefix) {
    chunk_search_type chunk_search;
    assert(size_access::csize(prefix) != seq->size());
    cur->annotation.prefix.set_cached(prefix);
    auto res = chunk_search(*cur, meas_fct, prefix, p);
    size_type pos = res.position;
    prefix = res.prefix;
    seg = cur->segment_by_index(pos - 1);
    if (seg.middle - seg.begin > cur->size())
      cur->segment_by_index(pos - 1);
    assert(seg.middle - seg.begin <= cur->size());
    return prefix;
  }
  
  // Updates position of the iterator to the position of the target
  // updates cur and seg fields
  template <class Pred>
  measured_type chunkedseq_search_by(const Pred& p, measured_type prefix) {
    bool found;
    if (seq->is_buffer(cur))
      cur = nullptr;
    prefix = seq->search_for_chunk(p, prefix, found, cur);
    if (found) {
      prefix = chunk_search_by(p, prefix);
    } else {
      // make the iterator logically point one past the end of the sequence
      assert(size_access::csize(prefix) == seq->size());
      cur = seq->get_chunk_containing_last_item();
      size_type sz_cur = cur->size();
      measured_type m = prefix;
      size_access::size(m) = seq->size() - sz_cur;
      cur->annotation.prefix.set_cached(m);
      if (sz_cur == 0)
        seg.begin = seg.end = nullptr;
      else
        seg = cur->segment_by_index(sz_cur - 1);
      seg.middle = seg.end;
    }
    check();
    return prefix;
  }
  
  template <class Pred>
  measured_type chunkedseq_search_by(const Pred& p) {
    return chunkedseq_search_by(p, algebra_type::identity());
  }
  
  self_type new_iterator_at(size_type sz) const {
    self_type it(*this);
    it.search_by_one_based_index(sz);
    return it;
  }
  
  self_type& increment_by(size_type n) {
    check();
    size_type orig_sz = size();
    pointer m = seg.middle + n;
    if (m >= seg.end)
      search_by_one_based_index(orig_sz + n);
    else
      seg.middle = m;
    assert(size() == orig_sz + n);
    check();
    return *this;
  }
  
  self_type& decrement_by(size_type n) {
    check();
    size_type orig_sz = size();
    pointer m = seg.middle - n;
    if (m < seg.begin)
      search_by_one_based_index(orig_sz - n);
    else
      seg.middle = m;
    assert(size() == orig_sz - n);
    check();
    return *this;
  }
  
  static size_type nb_before_middle(const_chunk_pointer c, segment_type seg) {
    if (seg.middle == seg.end) {
      if (seg.middle == seg.begin)
        return 0;
      return c->index_of_pointer(seg.middle - 1) + 1;
    } else {
      return c->index_of_pointer(seg.middle);
    }
  }
  
  size_type size_of_prefix() const {
    measured_type prefix_of_chunk = cur->annotation.prefix.template get_cached<measured_type>();
    size_type nb_items_before_chunk = size_access::csize(prefix_of_chunk);
    size_type nb_items_before_seg_middle = nb_before_middle(cur, seg);
    return nb_items_before_chunk + nb_items_before_seg_middle;
  }
  
  void search_by_one_based_index(size_type i) {
    using predicate_type = itemsearch::less_than_by_position<measured_type, size_type, size_access>;
    predicate_type p(i - 1);
    chunkedseq_search_by(p);
    size_type sz = size();
    size_type ssz = seq->size();
    assert(sz <= ssz + 1);
    assert(sz == i);
  }
  
  void search_by_zero_based_index(size_type i) {
    search_by_one_based_index(i + 1);
  }
  
  /*---------------------------------------------------------------------*/
  
public:
  
  random_access(const_chunkedseq_pointer seq, const measure_type& meas, position_type pos)
  : seq(seq), cur(nullptr), meas_fct(meas) {
    search_by_one_based_index(1);
    switch (pos) {
      case begin: {
        search_by_one_based_index(1);
        break;
      }
      case end: {
        search_by_one_based_index(seq->size() + 1);
        break;
      }
    }

  }
  
  random_access() : seq(nullptr), cur(nullptr) { }
  
  /*---------------------------------------------------------------------*/
  /** @name ForwardIterator
   */
  ///@{
  
  bool operator==(const self_type& other) const {
    assert(seq == other.seq);
    bool eq =  seg.middle == other.seg.middle
            && seg.end == other.seg.end;
#ifndef NDEBUG
    size_type sz = size();
    size_type sz_other = other.size();
    assert(eq ? sz == sz_other : sz != sz_other);
#endif
    return eq;
  }
  
  bool operator!=(const self_type& other) const {
    return ! (*this == other);
  }
  
  reference operator*() const {
    return *seg.middle;
  }
  
  // prefix ++
  self_type& operator++() {
    return increment_by(1);
  }
  
  // postfix ++
  self_type operator++(int) {
    self_type result(*this);
    ++(*this);
    return result;
  }
  
  // prefix --
  self_type& operator--() {
    return decrement_by(1);
  }
  
  // postfix --
  self_type operator--(int) {
    self_type result(*this);
    --(*this);
    return result;
  }
  
  ///@}
  
  /*---------------------------------------------------------------------*/
  /** @name RandomAccessIterator
   */
  ///@{
  
  self_type& operator+=(const size_type n) {
    return increment_by(n);
  }
  
  self_type operator+(const self_type& other) const {
    return new_iterator_at(size() + other.size());
  }
  
  self_type operator+(const size_type n) const {
    return new_iterator_at(size() + n);
  }
  
  self_type& operator-=(const size_type n) {
    return decrement_by(n);
  }
  
  difference_type operator-(const self_type& other) const {
    return size() - other.size();
  }
  
  self_type operator-(const size_type n) const {
    size_type sz = size();
    assert(sz > n);
    return new_iterator_at(sz - n);
  }
  
  reference operator[](const size_type n) const {
    return *(*this + n);
  }
  
  bool operator<(const self_type& other) const {
    return size() < other.size();
  }
  
  bool operator>(const self_type& other) const {
    return size() > other.size();
  }
  
  bool operator<=(const self_type& other) const {
    return size() <= other.size();
  }
  
  bool operator>=(const self_type& other) const {
    return size() >= other.size();
  }
  
  ///@}
  
  /*---------------------------------------------------------------------*/
  /** @name Item search
   */
  ///@{
  
  /*!
   * \brief Returns the number of items preceding and including the item
   * pointed to by the iterator
   *
   * #### Complexity ####
   * Constant time.
   *
   */
  size_type size() const {
    return size_of_prefix() + 1;
  }
  
  template <class Pred>
  void search_by(const Pred& p) {
    auto q = [&] (measured_type m) {
      return p(size_access::cclient(m));
    };
    chunkedseq_search_by(q, algebra_type::identity());
  }
  
  segment_type get_segment() const {
    return seg;
  }
  
  ///@}
  
};

/***********************************************************************/
 
} // end namespace
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_ITERATOR_H_ */