/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Representation of a chunk
 * \file chunk.hpp
 *
 */

#include "itemsearch.hpp"
#include "annotation.hpp"

#ifndef _PASL_DATA_CHUNK_H_
#define _PASL_DATA_CHUNK_H_

namespace pasl {
namespace data {
namespace chunkedseq {
  
/***********************************************************************/
  
/*---------------------------------------------------------------------*/
// Carrier for a deallocation function that calls "delete" on its argument

class Pointer_deleter {
public:
  static constexpr bool should_use = true;
  template <class Item>
  static void dealloc(Item* x) {
    delete x;
  }
};
  
class Dummy_pointer_deleter { // does nothing
public:
  static constexpr bool should_use = false;
  template <class Item>
  static void dealloc(Item x) {
    assert(false);
  }
};

/*---------------------------------------------------------------------*/
// Carrier for a deep-copy function that calls the copy constructor on its argument

class Pointer_deep_copier {
public:
  static constexpr bool should_use = true;
  template <class Item>
  static Item* copy(Item* x) {
    return new Item(*x);
  };
};

class Dummy_pointer_deep_copier {  // does nothing
public:
  static constexpr bool should_use = false;
  template <class Item>
  static Item copy(Item x) {
    assert(false);
    return x;
  };
};
  
/*---------------------------------------------------------------------*/
/*!
 * \class chunk
 * \brief Fixed-capacity array along with cached measurement of array contents
 * \tparam Fixed_capacity_queue type of the fixed-capacity array
 * \tparam Cached_measure type of the policy used to maintain the cached measurement
 * \tprarm Annotation type of an annotation object that a client might want to attach,
 * e.g., parent/sibling pointer
 * \tparam Pointer_deleter1 type of the policy used to manage deallocation
 * of (pointer-value) items
 * \tparam Pointer_deep_copier1 type of the policy used to manage copying
 * of (pointer-value) items
 *
 * We assume that each chunk consists of one or two segments. Two
 * segments indicates wraparound in a ringbuffer. In the future we
 * might consider supporting unbounded segments.
 */
template <
  class Fixed_capacity_queue,
  class Cached_measure,
  class Annotation=annotation::annotation_builder<>,
  class Pointer_deleter1=Dummy_pointer_deleter,
  class Pointer_deep_copier1=Dummy_pointer_deep_copier,
  class Size_access=itemsearch::no_size_access
  >
class chunk {
public:
  
  /*---------------------------------------------------------------------*/
  /** @name Item-specific types
   */
  ///@{
  using queue_type = Fixed_capacity_queue;
  using value_type = typename queue_type::value_type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using segment_type = typename queue_type::segment_type;
  ///@}

  /*---------------------------------------------------------------------*/
  /** @name Cached-measurement-specific  types
   */
  ///@{
  using cache_type = Cached_measure;
  using measured_type = typename cache_type::measured_type;
  using algebra_type = typename cache_type::algebra_type;
  using measure_type = typename cache_type::measure_type;
  ///@}

  /*---------------------------------------------------------------------*/
  /** @name Container-configuration types
   */
  ///@{
  using size_type = typename cache_type::size_type;
  using annotation_type = Annotation;
  using self_type = chunk<queue_type, cache_type, annotation_type, Pointer_deleter1, Pointer_deep_copier1, Size_access>;
  ///@}
  
  //! capacity in number of items
  static constexpr int capacity = queue_type::capacity;
  
  //! queue structure to contain the items of the chunk
  queue_type items;
  //! cached measurement of the items contained in the chunk
  measured_type cached;
  //! annotation value to be attached to the chunk
  annotation_type annotation;
  
  using search_type = itemsearch::search_in_chunk<self_type, algebra_type>;
  
private:
  
  /* Note:
   * Our combine operator is not necessarily commutative.
   * as such, we need to be careful to get the order of the
   * operands right when we increment on the front and back.
   */
  
  inline void incr_front(const measured_type& m) {
    cached = algebra_type::combine(m, cached);
  }
  
  inline void incr_back(const measured_type& m) {
    cached = algebra_type::combine(cached, m);
  }

  inline void decr_front(const measured_type& m) {
    incr_front(algebra_type::inverse(m));
  }
  
  inline void decr_back(const measured_type& m) {
    incr_back(algebra_type::inverse(m));
  }
  
  inline measured_type measure_range(const measure_type& meas, size_type lo, size_type hi) const {
    size_type nb = hi - lo;
    size_type sz = size();
    assert(nb <= sz);
    measured_type res = algebra_type::identity();
    items.for_each_segment(int(lo), int(hi), [&] (const_pointer lo, const_pointer hi) {
      res = algebra_type::combine(res, meas(lo, hi));
    });
    return res;
  }
  
  inline measured_type measure(const measure_type& meas) const {
    return measure_range(meas, 0, size());
  }
  
  //! \todo change interface of incr/decr frontn/backn to take nb items instead of offset
  
  /* increments the cached measurement by the combined measurements
   * of the items at positions [0, hi]
   */
  inline void incr_frontn(const measure_type& meas, size_type hi) {
    incr_front(measure_range(meas, 0, hi));
  }
  
  /* increments the cached measurement by the combined measurements
   * of the items at positions [lo, size()]
   */
  inline void incr_backn(const measure_type& meas, size_type lo) {
    incr_back(measure_range(meas, lo, size()));
  }
  
  inline void decr_frontn(const measure_type& meas, size_type hi) {
    decr_front(measure_range(meas, 0, hi));
  }
  
  inline void decr_backn(const measure_type& meas, size_type lo) {
    decr_back(measure_range(meas, lo, size()));
  }
  
public:
  
  /*---------------------------------------------------------------------*/
  /** @name Constructors and destructors
   */
  ///@{
  chunk() {
    cached = algebra_type::identity();
  }
  
  chunk(const self_type& other)
  : cached(other.cached), items(other.items), annotation(other.annotation) {
    if (Pointer_deep_copier1::should_use)
      for_each([&] (value_type& v) {
        v = Pointer_deep_copier1::copy(v);
      });
  }
  
  ~chunk() {
    if (Pointer_deleter1::should_use)
      for_each([&] (value_type& v) {
        Pointer_deleter1::dealloc(v);
      });
  }
  ///@}
  
  /*---------------------------------------------------------------------*/
  /** @name Capacity
   */
  ///@{
  bool full() const {
    return items.full();
  }
  
  bool empty() const {
    return items.empty();
  }

  bool partial() const {
    return items.partial();
  }

  size_type size() const {
    return items.size();
  }
  ///@}
  
  /*---------------------------------------------------------------------*/
  /** @name Cached measurement
   */
  ///@{
  measured_type get_cached() const {
    return cached;
  }
  ///@}
  
  /*---------------------------------------------------------------------*/
  /** @name Item access
   */
  ///@{
  value_type& front() const {
    return items.front();
  }
  
  value_type& back() const {
    return items.back();
  }
  
  void frontn(value_type* xs, size_type nb) {
    items.frontn(xs, (int)nb);
  }
  
  void backn(value_type* xs, size_type nb) {
    items.backn(xs, (int)nb);
  }
  
  value_type& operator[](size_type ix) const {
    return items[ix];
  }
  
  template <class Body>
  void for_each(const Body& body) const {
    items.for_each(body);
  }
  
  template <class Body>
  void for_each_segment(const Body& body) const {
    for_each_segment(size_type(0), size(), body);
  }
  
  // lo inclusive; hi exclusive
  template <class Body>
  void for_each_segment(size_type lo, size_type hi, const Body& body) const {
    items.for_each_segment(int(lo), int(hi), body);
  }
  
  segment_type segment_by_index(size_type i) const {
    return items.segment_by_index(int(i));
  }
  
  size_type index_of_pointer(const value_type* p) const {
    return items.index_of_pointer(p);
  }
  ///@}
  
  /*---------------------------------------------------------------------*/
  /** @name Modifiers
   */
  ///@{
  void push_front(const measure_type& meas, const value_type& x) {
    items.push_front(x);
    incr_front(meas(x));
  }
  
  void push_back(const measure_type& meas, const value_type& x) {
    items.push_back(x);
    incr_back(meas(x));
  }
  
  value_type pop_front(const measure_type& meas) {
    value_type v = front();
    if (algebra_type::has_inverse)
      decr_front(meas(v));
    items.pop_front();
    if (! algebra_type::has_inverse)
      reset_cache(meas);
    return v;
  }
  
  value_type pop_back(const measure_type& meas) {
    value_type v = back();
    if (algebra_type::has_inverse)
      decr_back(meas(v));
    items.pop_back();
    if (! algebra_type::has_inverse)
      reset_cache(meas);
    return v;
  }
  
  /* TODO: would this code be more efficient when has_inverse is true?
  value_type pop_front(const measure_type& meas) {
    value_type v = items.pop_front();
    if (algebra_type::has_inverse)
      decr_front(meas(v));
    if (! algebra_type::has_inverse)
      reset_cache(meas);
    return v;
  }
  
  value_type pop_back(const measure_type& meas) {
    value_type v = items.pop_back();
    if (algebra_type::has_inverse)
      decr_back(meas(v));
    if (! algebra_type::has_inverse)
      reset_cache(meas);
    return v;
  }
  */

  void pushn_front(const measure_type& meas, const value_type* xs, size_type nb) {
    items.pushn_front(xs, (int)nb);
    incr_frontn(meas, nb);
  }
  
  void pushn_back(const measure_type& meas, const value_type* xs, size_type nb) {
    size_type nb_before = size();
    items.pushn_back(xs, (int)nb);
    incr_backn(meas, int(nb_before));
  }
  
  template <class Body>
  void pushn_back(const measure_type& meas, const Body& body, size_type nb) {
    size_type nb_before = size();
    items.pushn_back(body, (int)nb);
    incr_backn(meas, int(nb_before));
  }
  
  void popn_front(const measure_type& meas, size_type nb) {
    if (algebra_type::has_inverse)
      decr_frontn(meas, nb);
    items.popn_front(int(nb));
    if (! algebra_type::has_inverse)
      reset_cache(meas);
  }
  
  void popn_back(const measure_type& meas, size_type nb) {
    if (algebra_type::has_inverse) {
      size_type nb_before = size() - nb;
      decr_backn(meas, nb_before);
    }
    items.popn_back((int)nb);
    if (! algebra_type::has_inverse)
      reset_cache(meas);
  }
  
  void popn_front(const measure_type& meas, value_type* xs, size_type nb) {
    check_cached(meas);
    if (algebra_type::has_inverse)
      decr_frontn(meas, nb);
    items.popn_front(xs, (int)nb);
    if (! algebra_type::has_inverse)
      reset_cache(meas);
    check_cached(meas);
  }
  
  void popn_back(const measure_type& meas, value_type* xs, size_type nb) {
    if (algebra_type::has_inverse) {
      size_type nb_before = size() - nb;
      decr_backn(meas, nb_before);
    }
    items.popn_back(xs, (int)nb);
    if (! algebra_type::has_inverse)
      reset_cache(meas);
  }
  
  template <class Consumer, bool should_consume>
  void popn_back(const measure_type& meas, const Consumer& cons, size_type nb) {
    size_type sz = size();
    assert(nb <= sz);
    if (should_consume && sz > 0 && nb > 0) {
      size_type i = sz - nb;
      segment_type seg = segment_by_index(i);
      size_type sz_seg = seg.end - seg.middle;
      if (sz_seg == nb) {
        cons(seg.middle, seg.middle + nb);
      } else { // handle wraparound
        segment_type seg2 = segment_by_index(i + sz_seg);
        cons(seg2.middle, seg2.middle + (nb - sz_seg));
        cons(seg.middle, seg.middle + sz_seg);
      }
    }
    popn_back(meas, nb);
  }
  
  template <class Consumer, bool should_consume>
  void popn_front(const measure_type& meas, const Consumer& cons, size_type nb) {
    size_type sz = size();
    assert(nb <= sz);
    if (should_consume && sz > 0 && nb > 0) {
      size_type i = nb - 1;
      segment_type seg = segment_by_index(i);
      size_type sz_seg = seg.middle - seg.begin + 1;
      if (sz_seg == nb) {
        cons(seg.begin, seg.middle + 1);
      } else { // handle wraparound
        segment_type seg2 = segment_by_index(0);
        cons(seg2.begin, seg2.end);
        cons(seg.begin, seg.middle + 1);
      }
    }
    popn_front(meas, nb);
  }
  
  void transfer_from_back_to_front(const measure_type& meas, chunk& target, size_type nb) {
    size_type sz = size();
    assert(sz >= nb);
    measured_type delta = measure_range(meas, sz - nb, sz);
    if (algebra_type::has_inverse)
      decr_back(delta);
    items.transfer_from_back_to_front(target.items, (int)nb);
    if (! algebra_type::has_inverse)
      reset_cache(meas);
    target.incr_front(delta);
  }
  
  void transfer_from_front_to_back(const measure_type& meas, chunk& target, size_type nb) {
    measured_type delta = measure_range(meas, size_type(0), nb);
    if (algebra_type::has_inverse)
      decr_front(delta);
    items.transfer_from_front_to_back(target.items, (int)nb);
    if (! algebra_type::has_inverse)
      reset_cache(meas);
    target.incr_back(delta);
  }
  
  /* 3-way split: place in `x` the first item that reaches the target measure,
   * if there is such an item.
   *
   * write in `found` whether or not there is such an item.
   */
  template <class Pred, class Search, class Search_measure>
  typename Search::measured_type split(const measure_type& meas,
                                       const Pred& p,
                                       const Search& search,
                                       const Search_measure& search_meas,
                                       typename Search::measured_type prefix,
                                       bool& found,
                                       value_type& x,
                                       self_type& other) {
    auto res = search(*this, search_meas, prefix, p);
    size_type pos = res.position; // one-based index pointing at the target item
    prefix = res.prefix;
    size_type sz = size();
    assert(sz > 0);
    assert(pos > 0);
    found = pos < sz+1;
    if (pos == 1) {
      x = pop_front(meas);
      swap(other);
      return prefix;
    }
    if (pos == sz) {
      x = pop_back(meas);
      return prefix;
    }
    transfer_from_back_to_front(meas, other, sz - pos);
    x = pop_back(meas);
    return prefix;
  }
  
  /* same as above but requires that there is an item in the chunk that reaches
   * the target measure
   */
  template <class Pred, class Search, class Search_measure>
  typename Search::measured_type split(const measure_type& meas,
                                       const Pred& p,
                                       const Search& search,
                                       const Search_measure& search_meas,
                                       typename Search::measured_type prefix,
                                       value_type& x,
                                       self_type& other) {
    bool found;
    prefix = split(meas, p, search, search_meas, prefix, found, x, other);
    assert(found);
    return prefix;
  }
  
  // 2-way split: based on a 3-way split ==> requires a predicate valid for a 3-way split
  template <class Pred, class Search, class Search_measure>
  typename Search::measured_type split(const measure_type& meas,
                                       const Pred& p,
                                       const Search& search,
                                       const Search_measure& search_meas,
                                       typename Search::measured_type prefix,
                                       self_type& other) {
    value_type x;
    prefix = split(meas, p, search, search_meas, prefix, x, other);
    other.push_front(meas, x);
    return prefix;
  }
  
  template <class Pred>
  measured_type split(const measure_type& meas,
                      const Pred& p,
                      measured_type prefix,
                      value_type& x,
                      self_type& other) {
    search_type search;
    return split(meas, p, search, meas, prefix, x, other);
  }
  
  template <class Pred>
  measured_type split(const measure_type& meas,
                      const Pred& p,
                      measured_type prefix,
                      self_type& other) {
    search_type search;
    return split(meas, p, search, meas, prefix, other);
  }
  
  void concat(const measure_type& meas, self_type& other) {
    other.transfer_from_front_to_back(meas, *this, other.size());
  }
  
  void clear() {
    items.popn_back(int(size()));
    cached = algebra_type::identity();
  }
  
  void swap(self_type& other) {
    items.swap(other.items);
    cache_type::swap(cached, other.cached);
    annotation.swap(other.annotation);
  }
  
  void reset_cache(const measure_type& meas) {
    cached = measure(meas);
  }
  ///@}
  
  void check_cached(const measure_type& meas) const {
#ifdef DEBUG_CHUNK
    if (Size_access::enable_index_optimization)
      assert(Size_access::csize(measure(meas)) == Size_access::csize(cached));
#endif
  }
  
};

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_CHUNK_H_ */