/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Routines for finding an item in a container via binary search
 * \file itemsearch.hpp
 *
 */

#include "segment.hpp"

#ifndef _PASL_DATA_ITEMSEARCH_H_
#define _PASL_DATA_ITEMSEARCH_H_

namespace pasl {
namespace data {
namespace chunkedseq {
namespace itemsearch {

/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/* Compare-by-position (i.e., one-based index)
 *
 * The purpose of this operator is to enable optimizations in item
 * search that are specific to indexing (i.e., finding an item
 * quickly by using just pointer arithmetic to locate its position in
 * the sequence).
 */

template <class Comparable>
class less_than {
public:
  bool operator()(Comparable x, Comparable y) {
    return x < y;
  }
};
  
template <class Measured, class Position, class Measured_fields, class Comparator>
class compare_measured_by_position {
private:
  
  Position pos;
  
public:

  compare_measured_by_position(Position pos) : pos(pos) { }
  
  bool operator()(Measured m) const {
    return Comparator()(pos, Measured_fields::csize(m));
  }
  
  Position get_position() const {
    return pos;
  }
  
};
  
/* \brief Predicate constructor to define zero-based indexing scheme
 *
 * Example use:
 *
 * less_than_by_position p(5);   // p(i) = 5 < i
 *
 * p(1) p(2) p(3) p(4) p(5) p(6) p(7)
 * f    f    f    f    f    t    t
 */
template <class Measured, class Position, class Measured_fields>
using less_than_by_position =
  compare_measured_by_position<Measured, Position, Measured_fields, less_than<Position>>;
  
/*---------------------------------------------------------------------*/
/* Search result type */
  
/*!
 * \class search_result
 * \brief Result of a search for an item in a sequence
 * \tparam Position type of the position used by the search (e.g., pointer, index, etc.)
 * \tparam Measured type of the cached measurement of the sequence
 */
template <class Position, class Measured>
class search_result {
public:
  Position position;
  Measured prefix;
  search_result() { }
  search_result(Position position, Measured prefix)
  : position(position), prefix(prefix) { }
};

/*---------------------------------------------------------------------*/
/* Linear search for an item in a segment */

template <class Item, class Algebra>
class search_in_segment {
public:
  
  using value_type = Item;
  using const_pointer = const value_type*;
  using segment_type = segment<const_pointer>;
  
  using algebra_type = Algebra;
  using measured_type = typename Algebra::value_type;
  
  using result_type = search_result<const_pointer, measured_type>;
  
  template <class Pred, class Measure>
  static result_type search_by(segment_type seg, const Measure& meas, measured_type prefix,
                               const Pred& p) {
    measured_type new_prefix = prefix;
    const_pointer ptr;
    for (ptr = seg.middle; ptr != seg.end; ptr++) {
      measured_type m = algebra_type::combine(new_prefix, meas(*ptr));
      if (p(m))
        break; // found it
      new_prefix = m;
    }
    return result_type(ptr, new_prefix);
  }
  
};

/*---------------------------------------------------------------------*/
/* Linear search for an item in a fixed-capacity queue */
  
class no_size_access {
public:
  static constexpr bool enable_index_optimization = false;
  
  template <class Measured>
  static size_t size(Measured& m) {
    assert(false);
    return 0;
  }
  template <class Measured>
  static size_t csize(Measured m) {
    assert(false);
    return 0;
  }

};

template <class Fixedcapacity_queue, class Algebra, class Size_access=no_size_access>
class search_in_fixed_capacity_queue {
public:
  
  using queue_type = Fixedcapacity_queue;
  using size_type = size_t;
  using value_type = typename queue_type::value_type;
  using pointer = value_type*;
  
  using algebra_type = Algebra;
  using measured_type = typename Algebra::value_type;
  
  using result_type = search_result<size_type, measured_type>;
  
private:
  
  using search_in_segment_type = search_in_segment<value_type, algebra_type>;
  using search_in_segment_result_type = typename search_in_segment_type::result_type;
  using segment_type = typename search_in_segment_type::segment_type;
  
  segment_type segment_by_index(const queue_type& items, size_type i) const {
    return make_const_segment(items.segment_by_index((int)i));
  }
  
public:
  
  template <class Pred, class Measure>
  result_type operator()(const queue_type& items, const Measure& meas, measured_type prefix,
                         const Pred& p) const {

    /* reference version
    measured_type new_prefix = prefix;
    size_t nb = items.size();
    size_t i;
    for (i = 0; i < nb; i++) {
      measured_type m = algebra_type::combine(new_prefix, meas(items[i]));
      if (p(m))
        break; // found it
      new_prefix = m;
    }
    return result_type(i+1, new_prefix);
    */

    size_type sz = size_type(items.size());
    segment_type seg1 = segment_by_index(items, 0);
    segment_type seg2 = segment_by_index(items, sz - 1);
    if (seg1.begin == seg2.begin) { // no wraparound
      assert(seg1.end - seg1.begin == sz);
      search_in_segment_result_type res = search_in_segment_type::search_by(seg1, meas, prefix, p);
      return result_type(res.position - seg1.begin + 1, res.prefix);
    }
    // wraparound
    search_in_segment_result_type res = search_in_segment_type::search_by(seg1, meas, prefix, p);
    size_type i = res.position - seg1.begin;
    prefix = res.prefix;
    if (res.position != seg1.end) // found in first of two segments
      return result_type(i + 1, prefix);
    seg2 = segment_type(seg2.begin, seg2.begin, seg2.end);
    res = search_in_segment_type::search_by(seg2, meas, prefix, p);
    size_type seg1_nb = seg1.end - seg1.begin;
    size_type seg2_nb = res.position - seg2.begin;
    i = seg1_nb + seg2_nb;
    prefix = res.prefix;
    return result_type(i + 1, prefix);

  }
  
  // optimization that is specific to a search for a position (i.e., one-based index)
  // the client field of the prefix value returned by this function is the identity value
  template <class Measure>
  result_type operator()(const queue_type& items, const Measure& meas, measured_type prefix,
                         const less_than_by_position<measured_type, size_type, Size_access>& p) const {
    if (! Size_access::enable_index_optimization) {
      // this optimization does not apply
      auto q = [&] (measured_type m) { return p(m); };
      return operator()(items, meas, prefix, q);
    }
    size_type target = p.get_position() + 1;
    size_type sz_prefix = Size_access::csize(prefix);
    size_type nb_items = size_type(items.size());
    size_type sz_with_items = nb_items + sz_prefix;
    assert(target <= sz_with_items + 1);
    result_type res;
    if (target == sz_with_items + 1) { // target pointing one past last item
      res.position = sz_with_items + 1;
      res.prefix = algebra_type::identity();
      Size_access::size(res.prefix) = res.position;
    } else { // target pointing to an item in the chunk
      res.position = target - sz_prefix;
      res.prefix = algebra_type::identity();                
      Size_access::size(res.prefix) = target - 1;
    }
    return res;
  } 

};
  
/*---------------------------------------------------------------------*/
/* Search over the items of a chunk */

template <class Chunk,
          class Algebra,
          class Size_access=no_size_access,
          template <class Fixedcapacity_queue, class Cache2, class Size_access2>
          class Queue_search=search_in_fixed_capacity_queue>
class search_in_chunk {
public:
  
  using chunk_type = Chunk;
  using queue_type = typename Chunk::queue_type;
  using size_type = size_t;
  using value_type = typename chunk_type::value_type;
  using pointer = value_type*;
  
  using algebra_type = Algebra;
  using measured_type = typename Algebra::value_type;
  
  using queue_search_type = Queue_search<queue_type, algebra_type, Size_access>;
  
  using result_type = typename queue_search_type::result_type;
  
public:
  
  template <class Pred, class Measure>
  result_type operator()(const chunk_type& chunk, const Measure& meas, measured_type prefix,
                         const Pred& p) const {
    queue_search_type search;
    return search(chunk.items, meas, prefix, p);
  }
  
};

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_ITEMSEARCH_H_ */
