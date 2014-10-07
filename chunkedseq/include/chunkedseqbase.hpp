 /*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Chunked-sequence functor
 * \file chunkedseqbase.hpp
 *
 */

#include <assert.h>
#include <iostream>
#include <utility>
#include <vector>
#include <initializer_list>

#include "iterator.hpp"
#include "chunkedseqextras.hpp"

#ifndef _PASL_DATA_CHUNKEDSEQBASE_H_
#define _PASL_DATA_CHUNKEDSEQBASE_H_

namespace pasl {
namespace data {
namespace chunkedseq {

/***********************************************************************/

/*!
 * \class chunkedseqbase
 * \tparam Configuration type of the class from which to access configuration
 * parameters for the container
 * \tparam Iterator type of the class to represent the iterator
 *

 later: explain the following preconditions
  Assume "Chunk" to implement fixed-capacity circular buffers.
  Assume "Topchunk_capacity >= 2" and "Recchunk_capacity >= 2".
  Assume TopItem to have a trivial destructor.

 later: study in detail the memory usage behavior of chunkedseq
  - in particular, study the alternative chunk-compaction policies
    we implemented previously in fftree; these policies still
    need to be implemented here
  - implement the bag mode
  - analyze memory usage via malloc_count

 */
template <
  class Configuration,
  template <
    class Chunkedseq,
    class Configuration1
  >
  class Iterator=iterator::random_access
>
class chunkedseqbase {
protected:

  using chunk_type = typename Configuration::chunk_type;
  using chunk_search_type = typename Configuration::chunk_search_type;
  using chunk_cache_type = typename Configuration::chunk_cache_type;
  using chunk_measured_type = typename chunk_cache_type::measured_type;
  using chunk_algebra_type = typename chunk_cache_type::algebra_type;
  using chunk_measure_type = typename chunk_cache_type::measure_type;
  using chunk_pointer = chunk_type*;

  using middle_type = typename Configuration::middle_type;
  using middle_cache_type = typename Configuration::middle_cache_type;
  using middle_measured_type = typename middle_cache_type::measured_type;
  using middle_algebra_type = typename middle_cache_type::algebra_type;
  using middle_measure_type = typename middle_cache_type::measure_type;
  using size_access = typename Configuration::size_access;

  using const_self_pointer_type = const chunkedseqbase<Configuration>*;

  static constexpr int chunk_capacity = Configuration::chunk_capacity;

public:

  /*---------------------------------------------------------------------*/
  /** @name Container-configuration types
   */
  ///@{
  using config_type = Configuration;
  using self_type = chunkedseqbase<config_type>;
  using size_type = typename config_type::size_type;
  using difference_type = typename config_type::difference_type;
  using allocator_type = typename config_type::item_allocator_type;
  ///@}

  /*---------------------------------------------------------------------*/
  /** @name Item-specific types
   */
  ///@{
  using value_type = typename config_type::value_type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using segment_type = typename config_type::segment_type;
  ///@}

  /*---------------------------------------------------------------------*/
  /** @name Cached-measure-specific types
   */
  ///@{
  using cache_type = chunk_cache_type;
  using measured_type = typename cache_type::measured_type;
  using algebra_type = typename cache_type::algebra_type;
  using measure_type = typename cache_type::measure_type;
  ///@}

  using iterator = Iterator<self_type, config_type>;
  friend iterator;

private:

  // representation of the structure: four chunks plus a middle sequence of chunks;
  // for efficient implementation of emptiness test, additional invariants w.r.t. the paper:
  //  - if front_outer is empty, then front_inner is empty
  //  - if back_outer is empty, then back_inner is empty
  //  - if front_outer and back_outer are empty, then middle is empty
  chunk_type front_inner, back_inner, front_outer, back_outer;
  std::unique_ptr<middle_type> middle;

  chunk_measure_type chunk_meas;
  middle_measure_type middle_meas;

  /*---------------------------------------------------------------------*/

  static inline chunk_pointer chunk_alloc() {
    return new chunk_type();
  }

  // only to free empty chunks
  static inline void chunk_free(chunk_pointer c) {
    assert(c->empty());
    delete c;
  }

  template <class Pred>
  middle_measured_type chunk_split(const Pred& p, middle_measured_type prefix,
                                   chunk_type& src, reference x, chunk_type& dst) {
    chunk_search_type chunk_search;
    return src.split(chunk_meas, p, chunk_search, middle_meas, prefix, x, dst);
  }

  /*---------------------------------------------------------------------*/

  bool is_buffer(const chunk_type* c) const {
    return c == &front_outer || c == &front_inner
        || c == &back_inner  || c == &back_outer;
  }

  // ensures that if front_outer is empty, then front_inner and middle (and back_inner)
  // are all empty
  void restore_front_outer_empty_other_empty() {
    if (front_outer.empty()) {
      if (! front_inner.empty()) {
        front_inner.swap(front_outer);
      } else if (! middle->empty()) {
        chunk_pointer c = middle->pop_front(middle_meas);
        front_outer.swap(*c);
        chunk_free(c);
      } else if (! back_inner.empty()) {
        back_inner.swap(front_outer); // optional operation
      }
    }
    assert(! front_outer.empty()
       || (front_inner.empty() && middle->empty() && back_inner.empty()) );
  }

  void restore_back_outer_empty_other_empty() {
    if (back_outer.empty()) {
      if (! back_inner.empty()) {
        back_inner.swap(back_outer);
      } else if (! middle->empty()) {
        chunk_pointer c = middle->pop_back(middle_meas);
        back_outer.swap(*c);
        chunk_free(c);
      } else if (! front_inner.empty()) {
        front_inner.swap(back_outer); // optional operation
      }
    }
    assert(! back_outer.empty()
       || (back_inner.empty() && middle->empty() && front_inner.empty()) );
  }

  // ensures that if the container is nonempty, then so is the front-outer buffer
  void ensure_front_outer_nonempty() {
    restore_front_outer_empty_other_empty();
    if (front_outer.empty() && ! back_outer.empty())
      front_outer.swap(back_outer);
    assert(empty() || ! front_outer.empty());
  }

  void ensure_back_outer_nonempty() {
    restore_back_outer_empty_other_empty();
    if (back_outer.empty() && ! front_outer.empty())
      back_outer.swap(front_outer);
    assert(empty() || ! back_outer.empty());
  }

  // assumption: invariant "both outer empty implies middle empty" may be broken;
  // calling this function restores it.
  void restore_both_outer_empty_middle_empty() {
    if (front_outer.empty() && back_outer.empty() && ! middle->empty()) {
      // pop to the front (to the back would also work)
      chunk_pointer c = middle->pop_front(middle_meas);
      front_outer.swap(*c);
      chunk_free(c);
    }
  }

  // ensures that inner buffers are empty, by pushing them in the middle if full
  void ensure_empty_inner() {
    if (! front_inner.empty())
      push_buffer_front_force(front_inner);
    if (! back_inner.empty())
      push_buffer_back_force(back_inner);
  }

  using const_chunk_pointer = const chunk_type*;

  const_chunk_pointer get_chunk_containing_last_item() const {
    if (! back_outer.empty())
      return &back_outer;
    if (! back_inner.empty())
      return &back_inner;
    if (! middle->empty())
#ifdef DEBUG_MIDDLE_SEQUENCE
      return middle->back();
#else
      return middle->cback();
#endif
    if (! front_inner.empty())
      return &front_inner;
    return &front_outer;
  }

  using position_type = enum {
    found_front_outer,
    found_front_inner,
    found_middle,
    found_back_inner,
    found_back_outer,
    found_nowhere
  };

  template <class Pred>
  middle_measured_type search(const Pred& p, middle_measured_type prefix,
                              position_type& pos) const {
    middle_measured_type cur = prefix; // prefix including current chunk
    if (! front_outer.empty()) {
      prefix = cur;
      cur = middle_algebra_type::combine(cur, middle_meas(&front_outer));
      if (p(cur)) {
        pos = found_front_outer;
        return prefix;
      }
    }
    if (! front_inner.empty()) {
      prefix = cur;
      cur = middle_algebra_type::combine(cur, middle_meas(&front_inner));
      if (p(cur)) {
        pos = found_front_inner;
        return prefix;
      }
    }
    if (! middle->empty()) {
      prefix = cur;
      cur = middle_algebra_type::combine(cur, middle->get_cached());
      if (p(cur)) {
        pos = found_middle;
        return prefix;
      }
    }
    if (! back_inner.empty()) {
      prefix = cur;
      cur = middle_algebra_type::combine(cur, middle_meas(&back_inner));
      if (p(cur)) {
        pos = found_back_inner;
        return prefix;
      }
    }
    if (! back_outer.empty()) {
      prefix = cur;
      cur = middle_algebra_type::combine(cur, middle_meas(&back_outer));
      if (p(cur)) {
        pos = found_back_outer;
        return prefix;
      }
    }
    prefix = cur;
    pos = found_nowhere;
    return prefix;
  }

  // set "found" to true or false depending on success of the search;
  // and set "cur" to the chunk that contains the item searched,
  // or to the chunk containing the last item of the sequence if the item was not found.
  // if finger search is enabled, then the routine checks whether `cur` contains a
  // null pointer or a pointer to a chunk; in the latter case, search starts from
  // the chunk
  // precondition: `cur` contains either a `nullptr` or a pointer to a "top chunk";
  // i.e., a leaf node of the middle sequence
  template <class Pred>
  middle_measured_type search_for_chunk(const Pred& p, middle_measured_type prefix,
                                        bool& found, const_chunk_pointer& cur) const {
    position_type pos;
    prefix = search(p, prefix, pos);
    found = true; // default
    switch (pos) {
      case found_front_outer: {
        cur = &front_outer;
        break;
      }
      case found_front_inner: {
        cur = &front_inner;
        break;
      }
      case found_middle: {
#ifdef DEBUG_MIDDLE_SEQUENCE
        assert(false);
#else
        prefix = middle->search_for_chunk(p, prefix, cur);
#endif
        break;
      }
      case found_back_inner: {
        cur = &back_inner;
        break;
      }
      case found_back_outer: {
        cur = &back_outer;
        break;
      }
      case found_nowhere: {
        cur = &back_outer;
        found = false;
        break;
      }
    } // end switch
    return prefix;
  }

  // precondition: other is empty
  template <class Pred>
  middle_measured_type split_aux(const Pred& p, middle_measured_type prefix, reference x, self_type& other) {
    assert(other.empty());
    copy_measure_to(other);
    ensure_empty_inner();
    position_type pos;
    prefix = search(p, prefix, pos);
    switch (pos) {
      case found_front_outer: {
        prefix = chunk_split(p, prefix, front_outer, x, other.front_outer);
        middle.swap(other.middle);
        back_outer.swap(other.back_outer);
        break;
      }
      case found_front_inner: {
        assert(false); // thanks to the call to ensure_empty_inner()
        break;
      }
      case found_middle: {
        back_outer.swap(other.back_outer);
        chunk_pointer c;
        prefix = middle->split(middle_meas, p, prefix, c, *other.middle);
        back_outer.swap(*c);
        chunk_free(c);
        prefix = chunk_split(p, prefix, back_outer, x, other.front_outer);
        break;
      }
      case found_back_inner: {
        assert(false); // thanks to the call to ensure_empty_inner()
        break;
      }
      case found_back_outer: {
        prefix = chunk_split(p, prefix, back_outer, x, other.back_outer);
        break;
      }
      case found_nowhere: {
        // don't split (item not found)
        break;
      }
    } // end switch
    restore_both_outer_empty_middle_empty();
    other.restore_both_outer_empty_middle_empty();
    return prefix;
  }

  // precondition: other is empty
  template <class Pred>
  middle_measured_type split_aux(const Pred& p, middle_measured_type prefix, self_type& other) {
    size_type sz_orig = size();
    value_type x;
    prefix = split_aux(p, prefix, x, other);
    if (size_access::csize(prefix) < sz_orig)
      other.push_front(x);
    return prefix;
  }

  /*---------------------------------------------------------------------*/

  // take a chunk "c" and push its content into the back of the middle sequence
  // as a new chunk; leaving "c" empty.
  void push_buffer_back_force(chunk_type& c) {
    chunk_pointer d = chunk_alloc();
    c.swap(*d);
    middle->push_back(middle_meas, d);
  }

  // symmetric to push_buffer_back
  void push_buffer_front_force(chunk_type& c) {
    chunk_pointer d = chunk_alloc();
    c.swap(*d);
    middle->push_front(middle_meas, d);
  }

  // take a chunk "c" and concatenate its content into the back of the middle sequence
  // leaving "c" empty.
  void push_buffer_back(chunk_type& c) {
    size_t csize = c.size();
    if (csize == 0) {
      // do nothing
    } else if (middle->empty()) {
      push_buffer_back_force(c);
    } else {
      chunk_pointer b = middle->back();
      size_t bsize = b->size();
      if (bsize + csize > chunk_capacity) {
        push_buffer_back_force(c);
      } else {
        middle->pop_back(middle_meas);
        c.transfer_from_front_to_back(chunk_meas, *b, csize);
        middle->push_back(middle_meas, b);
      }
    }
  }

  // symmetric to push_buffer_back
  void push_buffer_front(chunk_type& c) {
    size_t csize = c.size();
    if (csize == 0) {
      // do nothing
    } else if (middle->empty()) {
      push_buffer_front_force(c);
    } else {
      chunk_pointer b = middle->front();
      size_t bsize = b->size();
      if (bsize + csize > chunk_capacity) {
        push_buffer_front_force(c);
      } else {
        middle->pop_front(middle_meas);
        c.transfer_from_back_to_front(chunk_meas, *b, csize);
        middle->push_front(middle_meas, b);
      }
    }
  }

  void init() {
    middle.reset(new middle_type());
  }

public:

  /*---------------------------------------------------------------------*/
  /** @name Constructors
   */
  ///@{

  /*!
   * \brief Empty container constructor
   *
   * Constructs an empty container, with no items.
   *
   * #### Complexity ####
   * Constant
   *
   */
  chunkedseqbase() {
    init();
  }

  /*!
   * \brief Fill constructor
   *
   * Constructs an empty container, with `nb` items.
   * Each item is a copy of `val`.
   *
   * \param nb Number of items to put in the container
   * \param val Value to replicate
   *
   * #### Complexity ####
   * Linear in `nb`.
   *
   */
  chunkedseqbase(size_type n, const value_type& val) {
    init();
    std::vector<value_type> vals(chunk_capacity, val);
    const value_type* p = vals.data();
    auto prod = [&] (size_type, size_type nb) {
      return std::make_pair(p, p + nb);
    };
    stream_pushn_back(prod, n);
  }

  /*!
   * \brief Copy constructor
   *
   * Constructs a container with a copy of each of the
   * elements in `other`, in the same order.
   *
   * \param other Container from which to copy.
   *
   * #### Complexity ####
   * Linear in the resulting container size.
   *
   */
  chunkedseqbase(const self_type& other)
  : front_outer(other.front_outer),
    front_inner(other.front_inner),
    back_inner(other.back_inner),
    back_outer(other.back_outer),
    chunk_meas(other.chunk_meas),
    middle_meas(other.middle_meas) {
    middle.reset(new middle_type(*other.middle));
    check();
  }
  ///@}
  
  chunkedseqbase(std::initializer_list<value_type> l) {
    init();
    for (auto it = l.begin(); it != l.end(); it++)
      push_back(*it);
  }

  /*---------------------------------------------------------------------*/
  /** @name Capacity
   */
  ///@{

  /*!
   * \brief Test whether the container is empty
   *
   * Returns whether the container is empty (
   * i.e. whether its size is 0).
   *
   * #### Complexity ####
   * Constant time.
   *
   */
  bool empty() const {
    return front_outer.empty() && back_outer.empty();
  }

  /*!
   * \brief Returns size
   *
   * Returns the number of items in the container.
   *
   *
   * #### Complexity ####
   * Constant time.
   *
   */
  size_type size() const {
    size_type sz = 0;
    sz += front_outer.size();
    sz += front_inner.size();
    sz += size_access::csize(middle->get_cached());
    sz += back_inner.size();
    sz += back_outer.size();
    return sz;
  }
  ///@}

  /*---------------------------------------------------------------------*/
  /** @name Item access
   */
  ///@{

  ///@{
  /*!
   * \brief Accesses last item
   *
   * Returns a reference to the last item in the container.
   *
   * Calling this method on an `empty` container causes undefined
   * behavior.
   *
   * #### Complexity ####
   * Amortized constant time (worst case logarithmic time).
   *
   *
   * \return A reference to the last item in the
   * container.
   *
   */
  value_type front() const {
    assert(! front_outer.empty() || front_inner.empty());
    value_type v;
    if (! front_outer.empty()) {
      return front_outer.front();
    } else if (! back_inner.empty()) {
      return back_inner.front();
    } else {
      assert(! back_outer.empty());
      return back_outer.front();
    }
  }

  /*!
   * \brief Accesses first item
   *
   * Returns a reference to the first item in the container.
   *
   * Calling this method on an `empty` container causes undefined
   * behavior.
   *
   * #### Complexity ####
   * Amortized constant time (worst case logarithmic time).
   *
   * \return A reference to the first item in the
   * container.
   *
   */
  value_type back() const {
    assert(! back_outer.empty() || back_inner.empty());
    if (! back_outer.empty()) {
      return back_outer.back();
    } else if (! front_inner.empty()) {
      return front_inner.back();
    } else {
      assert(! front_outer.empty());
      return front_outer.back();
    }
  }

  /*!
   * \brief Access last items
   *
   * Copies the last `nb` items from the container to the
   * cells in the array `dst`.
   *
   * Calling this method when `size() < nb` causes undefined
   * behavior.
   *
   * #### Complexity ####
   * Linear in the number of items being copied (i.e., `nb`).
   *
   */
  void backn(value_type* dst, size_type nb) const {
    extras::backn(*this, dst, nb);
  }

  /*!
   * \brief Access first items
   *
   * Copies the first `nb` items from the container to the
   * cells in the array `dst`.
   *
   * Calling this method when `size() < nb` causes undefined
   * behavior.
   *
   * #### Complexity ####
   * Linear in the number of items being copied (i.e., `nb`).
   *
   */
  void frontn(value_type* dst, size_type nb) const {
    extras::frontn(*this, dst, nb);
  }

  /*!
   * \brief Consume last items
   *
   * Streams the first `nb` items from the container to the
   * client-supplied function `cons`.
   *
   * Let our container be `c = [c_0, c_1, ..., c_n]`. Then,
   * the call `stream_backn(cons, nb)` performs:
   *
   *       for (size_type i = nb-1; i < n+1; i++)
   *          cons(c_i);
   *
   * Calling this method when `size() < nb` causes undefined
   * behavior.
   *
   * #### Complexity ####
   * Linear in the number of items being copied (i.e., `nb`).
   *
   */
  template <class Consumer>
  void stream_backn(const Consumer& cons, size_type nb) const {
    extras::stream_backn(*this, cons, nb);
  }

  /*!
   * \brief Consume first items
   *
   * Streams the first `nb` items from the container to the
   * client-supplied function `cons`.
   *
   * Let our container be `c = [c_0, c_1, ..., c_n]`. Then,
   * the call `stream_backn(cons, nb)` performs:
   *
   *       for (size_type i = 0; i < nb; i++)
   *          cons(c_i);
   *
   * Calling this method when `size() < nb` causes undefined
   * behavior.
   *
   * #### Complexity ####
   * Linear in the number of items being copied (i.e., `nb`).
   *
   */
  template <class Consumer>
  void stream_frontn(const Consumer& cons, size_type nb) const {
    extras::stream_frontn(*this, cons, nb);
  }

  /*!
   * \brief Access item
   *
   * Returns a copy of the element at position `n` in
   * the vector container.
   *
   * \return The item at the specified position in the container.
   *
   * \param Position of an item in the container.
   * Notice that the first item has a position of 0 (not 1).
   * Member type size_type is an unsigned integral type.
   *
   * #### Complexity ####
   * Logarithmic time.
   *
   *
   */
  value_type operator[](size_type n) const {
    assert(n >= 0);
    assert(n < size());
    auto it = begin() + n;
    assert(it.size() == n + 1);
    return *it;
  }

  /*!
   * \brief Access item
   *
   * Returns a reference to the element at position `n` in
   * the vector container.
   *
   * \return The item at the specified position in the container.
   *
   * \param Position of an item in the container.
   * Notice that the first item has a position of 0 (not 1).
   * Member type size_type is an unsigned integral type.
   *
   * #### Complexity ####
   * Logarithmic time.
   *
   *
   */
  reference operator[](size_type n) {
    assert(n >= 0);
    assert(n < size());
    auto it = begin() + n;
    assert(it.size() == n + 1);
    return *it;
  }
  ///@}

  /*---------------------------------------------------------------------*/
  /** @name Modifiers
   */
  ///@{

  /*!
   * \brief Adds item at the beginning
   *
   * Adds a new item to the front of the container,
   * before its current first item. The content of `x` is
   * copied (or moved) to the new item.
   *
   * \param x Value to be copied (or moved) to the new item.
   *
   * #### Complexity ####
   * Amortized constant (worst case logarithmic).
   *
   */
  void push_front(const value_type& x) {
    if (front_outer.full()) {
      if (front_inner.full())
        push_buffer_front_force(front_inner);
      front_outer.swap(front_inner);
      assert(front_outer.empty());
    }
    front_outer.push_front(chunk_meas, x);
  }

  /*!
   * \brief Adds item at the end
   *
   * Adds a new item to the back of the container,
   * after its current last item. The content of `x` is
   * copied (or moved) to the new item.
   *
   * \param x Value to be copied (or moved) to the new item.
   *
   * #### Complexity ####
   * Amortized constant (worst case logarithmic).
   *
   */
  void push_back(const value_type& x) {
    if (back_outer.full()) {
      if (back_inner.full())
        push_buffer_back_force(back_inner);
      back_outer.swap(back_inner);
      assert(back_outer.empty());
    }
    back_outer.push_back(chunk_meas, x);
  }

  /*!
   * \brief Deletes first item
   *
   * Removes the first item in the container, effectively
   * reducing the container size by one.
   *
   * Calling this method destroys the removed item.
   *
   * Calling this method on an `empty` container causes undefined
   * behavior.
   *
   * #### Complexity ####
   * Amortized constant (worst case logarithmic).
   *
   * \return A copy of the first item in the
   * container.
   *
   */
  value_type pop_front() {
    if (front_outer.empty()) {
      assert(front_inner.empty());
      if (! middle->empty()) {
        chunk_pointer c = middle->pop_front(middle_meas);
        front_outer.swap(*c);
        chunk_free(c);
      } else if (! back_inner.empty()) {
        back_inner.swap(front_outer);
      } else if (! back_outer.empty()) {
        back_outer.swap(front_outer);
      }
    }
    assert(! front_outer.empty());
    value_type x = front_outer.pop_front(chunk_meas);
    restore_front_outer_empty_other_empty();
    return x;
  }

  /*!
   * \brief Deletes last item
   *
   * Removes the last item in the container, effectively
   * reducing the container size by one.
   *
   * Calling this method destroys the removed item.
   *
   * Calling this method on an `empty` container causes undefined
   * behavior.
   *
   * #### Complexity ####
   * Amortized constant (worst case logarithmic).
   *
   * \return A copy of the last item in the container.
   *
   */
  value_type pop_back() {
    if (back_outer.empty()) {
      assert(back_inner.empty());
      if (! middle->empty()) {
        chunk_pointer c = middle->pop_back(middle_meas);
        back_outer.swap(*c);
        chunk_free(c);
      } else if (! front_inner.empty()) {
        front_inner.swap(back_outer);
      } else if (! front_outer.empty()) {
        front_outer.swap(back_outer);
      }
    }
    assert(! back_outer.empty());
    value_type x = back_outer.pop_back(chunk_meas);
    restore_back_outer_empty_other_empty();
    return x;
  }

  /*!
   * \brief Adds items at the end
   *
   * Adds new items from a given array to the back of the container,
   * after its current last item. The content of `items` is
   * copied (or moved) to the new positions in the container.
   *
   * \param items Values to be copied (or moved) to the container.
   * \param nb Number of items to be inserted.
   *
   * #### Complexity ####
   * Linear in number of inserted items.
   *
   */
  void pushn_back(const_pointer src, size_type nb) {
    extras::pushn_back(*this, src, nb);
  }

  /*!
   * \brief Adds items at the beginning
   *
   * Adds new items to the front of the container,
   * before its current first item. The content of `items` is
   * copied (or moved) to the new positions in the container.
   *
   * \param items Values to be copied (or moved) to the container.
   * \param nb Number of items to be inserted.
   *
   * #### Complexity ####
   * Linear in number of inserted items.
   *
   */
  void pushn_front(const_pointer src, size_type nb) {
    extras::pushn_front(*this, src, nb);
  }

  /*!
   * \brief Deletes first items
   *
   * Removes the first items in the container, effectively
   * reducing the container size by `nb`.
   *
   * Calling this method destroys the removed items.
   *
   * The behavior is undefined if `nb > size()`.
   *
   * #### Complexity ####
   * Linear in number of items to be removed.
   *
   * \param nb Number of items to remove.
   *
   */
  void popn_front(size_type nb) {
    auto c = [] (const_pointer, const_pointer) { };
    stream_popn_front<typeof(c), false>(c, nb);
  }

  /*!
   * \brief Deletes last items
   *
   * Removes the last items in the container, effectively
   * reducing the container size by one.
   *
   * Calling this method destroys the removed items.
   *
   * The behavior is undefined if `nb > size()`.
   *
   * #### Complexity ####
   * Linear in number of items to be removed.
   *
   * \param destination Array to which the removed values
   * are to be copied (or moved).
   * \param nb Number of items to remove.
   *
   */
  void popn_back(size_type nb) {
    auto c = [] (const_pointer, const_pointer) { };
    stream_popn_back<typeof(c), false>(c, nb);
  }

  /*!
   * \brief Deletes first items
   *
   * Removes the first items in the container, effectively
   * reducing the container size by `nb`.
   *
   * Copies the removed items to the array `destination`.
   *
   * Calling this method destroys the removed items.
   *
   * The behavior is undefined if `nb > size()`.
   *
   * #### Complexity ####
   * Linear in number of items to be removed.
   *
   * \param destination Array to which the removed values
   * are to be copied (or moved).
   * \param nb Number of items to remove.
   *
   */
  void popn_back(value_type* dst, size_type nb) {
    extras::popn_back(*this, dst, nb);
  }

  /*!
   * \brief Deletes last items
   *
   * Removes the last items in the container, effectively
   * reducing the container size by one.
   *
   * Calling this method destroys the removed items.
   *
   * The behavior is undefined if `nb > size()`.
   *
   * #### Complexity ####
   * Linear in number of items to be removed.
   *
   * \param destination Array to which the removed values
   * are to be copied (or moved).
   * \param nb Number of items to remove.
   *
   */
  void popn_front(value_type* dst, size_type nb) {
    extras::popn_front(*this, dst, nb);
  }

  /*!
   * \brief Adds items at the end
   *
   * Adds new items from a given array to the back of the container,
   * after its current last item. The content is generated by applications
   * of the client-supplied function `body`. The applications initialize
   * the cells in the container in place.
   *
   * For a container `a`, new content is generated by applying
   *   body(0, a[0]); body(1, a[1]); ... body(n-1, a[n-1]);
   * where `n` is the size of the container.
   *
   * \tparam Type of the loop body class. This class must implement the
   * interface defined by \ref foreach_loop_body.
   * \param body function to generate the contents
   * \param nb Number of items to be inserted.
   *
   * #### Complexity ####
   * Linear in number of inserted items.
   *
   */
  template <class Producer>
  void stream_pushn_back(const Producer& prod, size_type nb) {
    if (nb == 0)
      return;
    size_type sz_orig = size();
    ensure_empty_inner();
    chunk_type c;
    c.swap(back_outer);
    size_type i = 0;
    while (i < nb) {
      size_type cap = (size_type)chunk_capacity;
      size_type m = std::min(cap, nb - i);
      m = std::min(m, cap - c.size());
      std::pair<const_pointer,const_pointer> rng = prod(i, m);
      const_pointer lo = rng.first;
      const_pointer hi = rng.second;
      size_type nb = hi - lo;
      c.pushn_back(chunk_meas, lo, nb);
      push_buffer_back(c);
      i += m;
    }
    restore_back_outer_empty_other_empty();
    assert(sz_orig + nb == size());
  }

  /*!
   * \brief Adds items at the beginning
   *
   * Adds new items to the front of the container,
   * before its current first item. The content is generated by applications
   * of the client-supplied function `body`. The applications initialize
   * the cells in the container in place.
   *
   * For a container `a`, new content is generated by applying
   *   body(0, a[0]); body(1, a[1]); ... body(n-1, a[n-1]);
   * where `n` is the size of the container.
   *
   * \tparam Type of the loop body class. This class must implement the
   * interface defined by \ref foreach_loop_body.
   * \param body function to generate the contents
   * \param nb Number of items to be inserted.
   *
   * #### Complexity ####
   * Linear in number of inserted items.
   *
   */
  template <class Producer>
  void stream_pushn_front(const Producer& prod, size_type nb) {
    if (nb == 0)
      return;
    size_type sz_orig = size();
    ensure_empty_inner();
    chunk_type c;
    c.swap(front_outer);
    size_type n = nb;
    while (n > 0) {
      size_type cap = (size_type)chunk_capacity;
      size_type m = std::min(cap, n);
      m = std::min(m, cap - c.size());
      n -= m;
      std::pair<const_pointer,const_pointer> rng = prod(n, m);
      const_pointer lo = rng.first;
      const_pointer hi = rng.second;
      size_type nb = hi - lo;
      c.pushn_front(chunk_meas, lo, nb);
      push_buffer_front(c);
    }
    restore_front_outer_empty_other_empty();
    assert(sz_orig + nb == size());
  }

  /*!
   * \brief Deletes last items
   *
   * Removes the last items in the container, effectively
   * reducing the container size by `nb`.
   *
   * Let our container be `c = [c_0, c_1, ..., c_n]`. Then,
   * the call `stream_backn(cons, nb)` performs:
   *
   *       for (size_type i = nb-1; i < n+1; i++)
   *          cons(c_i);
   *
   * Calling this method destroys the removed items.
   *
   * The behavior is undefined if `nb > size()`.
   *
   * #### Complexity ####
   * Linear in number of items to be removed.
   *
   * \param cons Function to receive items being removed from the
   * container.
   * \param nb Number of items to remove.
   *
   */
  template <class Consumer, bool should_consume=true>
  void stream_popn_back(const Consumer& cons, size_type nb) {
    size_type sz_orig = size();
    assert(sz_orig >= nb);
    size_type i = 0;
    while (i < nb) {
      ensure_back_outer_nonempty();
      size_type m = std::min(back_outer.size(), nb - i);
      back_outer.template popn_back<Consumer,should_consume>(chunk_meas, cons, m);
      i += m;
    }
    ensure_back_outer_nonempty();  // to restore invariants
    assert(sz_orig == size() + nb);
  }

  /*!
   * \brief Deletes first items
   *
   * Removes the first items in the container, effectively
   * reducing the container size by `nb`.
   *
   * Let our container be `c = [c_0, c_1, ..., c_n]`. Then,
   * the call `stream_backn(cons, nb)` performs:
   *
   *       for (size_type i = 0; i < nb; i++)
   *          cons(c_i);
   *
   * Calling this method destroys the removed items.
   *
   * The behavior is undefined if `nb > size()`.
   *
   * #### Complexity ####
   * Linear in number of items to be removed.
   *
   * \param cons Function to receive items being removed from the
   * container.
   * \param nb Number of items to remove.
   *
   */
  template <class Consumer, bool should_consume=true>
  void stream_popn_front(const Consumer& cons, size_type nb) {
    size_type sz_orig = size();
    assert(sz_orig >= nb);
    size_type i = 0;
    while (i < nb) {
      ensure_front_outer_nonempty();
      size_type m = std::min(front_outer.size(), nb - i);
      front_outer.template popn_front<Consumer,should_consume>(chunk_meas, cons, m);
      i += m;
    }
    ensure_front_outer_nonempty(); // to restore invariants
    assert(sz_orig == size() + nb);
  }

  /*!
   * \brief Merges with content of another container
   *
   * Removes all items from `other`, effectively reducing its
   * size to zero.
   *
   * Adds items removed from `other` to the back of this container,
   * after its current last item.
   *
   * \param other Container from which to take items.
   *
   * #### Complexity ####
   * Logarithmic in the size of the smallest of the two containers.
   *
   */
  void concat(self_type& other) {
    if (other.size() == 0)
      return;
    if (size() == 0)
      swap(other);
    // push buffers into the middle sequences
    push_buffer_back(back_inner);
    push_buffer_back(back_outer);
    other.push_buffer_front(other.front_inner);
    other.push_buffer_front(other.front_outer);
    // fuse front and back, if needed
    if (! middle->empty() && ! other.middle->empty()) {
      chunk_pointer c1 = middle->back();
      chunk_pointer c2 = other.middle->front();
      size_type nb1 = c1->size();
      size_type nb2 = c2->size();
      if (nb1 + nb2 <= chunk_capacity) {
        middle->pop_back(middle_meas);
        other.middle->pop_front(middle_meas);
        c2->transfer_from_front_to_back(chunk_meas, *c1, nb2);
        chunk_free(c2);
        middle->push_back(middle_meas, c1);
        // note: push might be factorized with earlier operations
      }
    }
    // migrate back chunks of the other and update the weight
    back_inner.swap(other.back_inner);
    back_outer.swap(other.back_outer);
    // concatenate the middle sequences
    middle->concat(middle_meas, *other.middle);
    // restore invariants
    restore_both_outer_empty_middle_empty();
    assert(other.empty());
  }

  //! \todo document
  template <class Pred>
  bool split(const Pred& p, reference middle_item, self_type& other) {
    size_type sz_orig = size();
    auto q = [&] (middle_measured_type m) {
      return p(size_access::cclient(m));
    };
    middle_measured_type prefix = split_aux(q, middle_algebra_type::identity(), middle_item, other);
    bool found = (size_access::csize(prefix) < sz_orig);
    return found;
  }

  //! \todo document
  template <class Pred>
  void split(const Pred& p, self_type& other) {
    value_type middle_item;
    bool found = split(p, middle_item, other);
    if (found)
      other.push_front(middle_item);
  }

  /*!
   * \brief Split by index
   *
   * The container is erased after and including the item at (zero-based) index `i`.
   *
   * The erased items are moved to the `other` container.
   *
   * \pre The `other` container is empty.
   * \pre `i >= 0`
   * \pre `i < size()`
   */
  void split(size_type i, self_type& other) {
    extras::split_by_index(*this, i, other);
  }

  /*!
   * \brief Split by iterator
   *
   * The container is erased after and including the item at the
   * specified `position`.
   *
   * The erased items are moved to the `other` container.
   *
   * \pre The `other` container is empty.
   */
  void split(iterator position, self_type& other) {
    extras::split_by_iterator(*this, position, other);
  }

  void split_approximate(self_type& other) {
    extras::split_approximate(*this, other);
  }

  /*!
   * \brief Inserts items
   *
   * The container is extended by inserting new items before
   * the item at the specified position.
   *
   * This effectively increates the container size by the amount
   * of items inserted.
   *
   * Insertions at the front or back are slightly more efficient
   * than on other positions.
   *
   * The parameters determine how many elements are inserted and
   * to which values they are initialized.
   *
   * \param position Position in the container where the new
   * items are inserted
   *
   * \param val Value to be copied (or moved) to the inserted
   * items
   *
   * \return An iterator that points to the first of the newly
   * inserted items.
   *
   * #### Complexity ####
   * Logarithmic time.
   *
   */
  iterator insert(iterator position, const value_type& val) {
    return extras::insert(*this, position, val);
  }

  /*!
   * \brief Erases items
   *
   * Removes from the container either a single item
   * (`position`) or a  range of items (`[first,last)`).
   *
   * This effectively reduces the container size by the
   * number of elements removed, which are destroyed.
   *
   * #### Complexity ####
   * Linear in the number of items erased (destructions)
   * and logarithmic in the number of items in the size
   * of the sequence.
   *
   * \param first Pointer to the first item to erase.
   * \param last Pointer to the position one past the last
   * item to erase.
   *
   */
  iterator erase(iterator first, iterator last) {
    return extras::erase(*this, first, last);
  }

  /*!
   * \brief Clears items
   *
   * Removes all items from the container (which are
   * destroyed), leaving the container with a size of 0.
   *
   * #### Complexity ####
   * Linear time (destructions).
   *
   */
  void clear() {
    popn_back(size());
  }

  /*!
   * \brief Swaps content
   *
   * Exchanges the content of the container by the
   * content of `x`, which is another container
   * of the same type. Sizes may differ.
   *
   * After the call to this member function, the items
   * in this container are those which were in `x` before
   * the call, and the elements of `x` are those which were
   * in this.
   *
   * #### Complexity ####
   * Constant time.
   *
   * \param x Another container container of the same type
   * (i.e., instantiated with the same template parameters)
   * whose content is swapped with that of this container.
   *
   */
  void swap(self_type& other) {
    std::swap(chunk_meas, other.chunk_meas);
    std::swap(middle_meas, other.middle_meas);
    front_outer.swap(other.front_outer);
    front_inner.swap(other.front_inner);
    middle.swap(other.middle);
    back_inner.swap(other.back_inner);
    back_outer.swap(other.back_outer);
  }

  ///@}

  friend void extras::split_by_index<self_type,size_type>(self_type& c, size_type i, self_type& other);

  /*---------------------------------------------------------------------*/
  /** @name Iterators
   */
  ///@{

  /*!
   * \brief Returns iterator to beginning
   *
   * Returns an iterator pointing to the first element in the
   * container.
   *
   * Notice that, unlike member front(), which returns
   * a reference to the first element, this function returns a
   * random access iterator pointing to it.
   *
   * If the container is empty, the returned iterator value
   * shall not be dereferenced.
   *
   * \return An iterator to the beginning of the container.
   *
   * #### Complexity ####
   * Logarithmic time.
   *
   */
  iterator begin() const {
    return iterator(this, middle_meas, chunkedseq::iterator::begin);
  }

  /*!
   * \brief Returns iterator to end
   *
   * Returns an iterator referring to the past-the-end element
   * in the container.
   *
   * The past-the-end element is the theoretical element that
   * would follow the last element in the vector. It does not
   * point to any element, and thus shall not be dereferenced.
   *
   * Because the ranges used by functions of the standard
   * library do not include the element pointed by their closing
   * iterator, this function is often used in combination with
   * begin() to specify a range including all the elements
   * in the container.
   *
   * If the container is empty, this function returns the same as
   * begin().
   *
   * \return An iterator to the element past the end of the container.
   *
   * #### Complexity ####
   * Logarithmic time.
   *
   */
  iterator end() const {
    return iterator(this, middle_meas, chunkedseq::iterator::end);
  }

  /*!
   * \brief Visits every item in the container
   *
   * Applies a given function `f` to each item in the sequence
   * in the left-to-right order of the sequence.
   *
   * \param f Function to apply to each item. The type of this
   * function must be `std::function<void(value_type)>`.
   *
   * #### Complexity ####
   * Linear in the size of the container.
   *
   */
  template <class Body>
  void for_each(const Body& f) const {
    front_outer.for_each(f);
    front_inner.for_each(f);
    middle->for_each([&] (chunk_pointer p) {
      p->for_each(f);
    });
    back_inner.for_each(f);
    back_outer.for_each(f);
  }

  /*!
   * \brief Visits every item in a specified range
   *
   * Applies a given function `f` to each item in the range
   * specified by `[beg,end)`.
   *
   * The function is applied in left-to-right order.
   *
   * \param f Function to apply to each item. The type of this
   * function must be `std::function<void(value_type)>`.
   * \param beg Pointer to the first item to visit
   * \param end Pointer to the item one past the last item to visit
   *
   * #### Complexity ####
   * Linear in `end-beg`.
   *
   */
  template <class Body>
  void for_each(iterator beg, iterator end, const Body& f) const {
    extras::for_each(beg, end, f);
  }

  /*!
   * \brief Visits every segment of items in the container
   *
   * Applies a given function `f` to each segment in the container.
   *
   * See \ref segments for a description of the segment type.
   *
   * \param f Function to apply to each segment. The type of this
   * function must be `std::function<void(segment_type)>`.
   *
   * #### Complexity ####
   * Linear in the number of segments in the container (assuming that
   * `f` visits each segment in unit time).
   *
   */
  template <class Body>
  void for_each_segment(const Body& f) const {
    front_outer.for_each_segment(f);
    front_inner.for_each_segment(f);
    middle->for_each([&] (chunk_pointer p) {
      p->for_each_segment(f);
    });
    back_inner.for_each_segment(f);
    back_outer.for_each_segment(f);
  }

  /*!
   * \brief Visits every segment of items in a specified range
   *
   * Applies a given function `f` to each segment in the range
   * defined by `[beg,end)`.
   *
   * See \ref segments for a description of the segment type.
   *
   * \param f Function to apply to the segments. The type of this
   * function must be `std::function<void(segment_type)>`.
   * \param beg Pointer to the first item to visit
   * \param end Pointer to the item one past the last item to visit
   *
   * #### Complexity ####
   * Linear in the number of segments in the given range (assuming that
   * `f` visits each segment in unit time).
   *
   */
  template <class Body>
  static void for_each_segment(iterator begin, iterator end, const Body& f) {
    extras::for_each_segment(begin, end, f);
  }

  ///@}

  /*---------------------------------------------------------------------*/
  /** @name Cached measurement
   */
  ///@{

  /*!
   * \brief Returns cached measurement
   *
   * #### Complexity ####
   * Constant time.
   *
   */
  measured_type get_cached() const {
    measured_type m = algebra_type::identity();
    m = algebra_type::combine(m, front_outer.get_cached());
    m = algebra_type::combine(m, front_inner.get_cached());
    measured_type n = size_access::cclient(middle->get_cached());
    m = algebra_type::combine(m, n);
    m = algebra_type::combine(m, back_inner.get_cached());
    m = algebra_type::combine(m, back_outer.get_cached());
    return m;
  }

  /*!
   * \brief Returns measurement operator
   *
   * #### Complexity ####
   * Constant time.
   *
   */
  measure_type get_measure() const {
    return chunk_meas;
  }

  /*!
   * \brief Sets measurement operator
   *
   * #### Complexity ####
   * Constant time.
   *
   */
  void set_measure(measure_type meas) {
    chunk_meas = meas;
    middle_meas.set_client_measure(meas);
  }

  /*!
   * \brief Copies the measurement operator
   *
   * #### Complexity ####
   * Constant time.
   *
   */
  void copy_measure_to(self_type& other) {
    other.set_measure(get_measure());
  }

  ///@}
  /*---------------------------------------------------------------------*/
  /** @name Debugging routines
   */
  ///@{
  // TODO: instead of taking this ItemPrinter argument, we could assume the ItemPrinter to be known from Item type
  template <class ItemPrinter>
  void print_chunk(const chunk_type& c) const  {
    printf("(");
    c.for_each([&] (value_type& x) {
      ItemPrinter::print(x);
      printf(" ");
    });
    printf(")");
  }

  // TODO: see whether we want to print cache
  class DummyCachePrinter { };

  template <class ItemPrinter, class CachePrinter = DummyCachePrinter>
  void print() const {
    auto show = [&] (const chunk_type& c) {
      print_chunk<ItemPrinter>(c); };
    show(front_outer);
    printf(" ");
    show(front_inner);
    printf(" [");
    middle->for_each([&] (chunk_pointer& c) {
      show(*c);
      printf(" ");
    });
    printf("] ");
    show(back_inner);
    printf(" ");
    show(back_outer);
  }

  void check_size() const {
#ifndef NDEBUG
    size_type sz = 0;
    middle->for_each([&] (chunk_pointer& c) {
      sz += c->size();
    });
    size_type msz = size_access::csize(middle->get_cached());
    assert(msz == sz);
    size_type sz2 = 0;
    for_each([&] (value_type&) {
      sz2++;
    });
    size_type sz3 = size();
    assert(sz2 == sz3);
#endif
  }

  void check() const {
#ifndef NDEBUG
    if (front_outer.empty())
      assert(front_inner.empty());
    if (back_outer.empty())
      assert(back_inner.empty());
    if (front_outer.empty() && back_outer.empty())
      assert(middle->empty());
    size_t sprev = -1;
    middle->for_each([&sprev] (chunk_pointer& c) {
      size_t scur = c->size();
      assert(scur > 0);
      if (sprev != -1)
        assert(sprev + scur > chunk_capacity);
      sprev = scur;
    });
    check_size();
#endif
  }

  template <class Add_edge, class Process_chunk>
  void reveal_internal_structure(const Add_edge& add_edge,
                                 const Process_chunk& process_chunk) const {
    long rootptr = (long)this;
    rootptr -= 8;
    add_edge((void*)rootptr, (void*)&front_outer);
    add_edge((void*)rootptr, (void*)&front_inner);
    add_edge((void*)rootptr, (void*)middle.get());
    add_edge((void*)rootptr, (void*)&back_inner);
    add_edge((void*)rootptr, (void*)&back_outer);
    process_chunk(&front_outer);
    process_chunk(&front_inner);
    middle->reveal_internal_structure(add_edge, process_chunk);
    process_chunk(&back_inner);
    process_chunk(&back_outer);
  }

  ///@}

};

/***********************************************************************/

} // end namespace chunkedseq
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_CHUNKEDSEQBASE_H_ */
