/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Chunked-bag functor
 * \file chunkedbag.hpp
 *
 */

#include <cstddef>
#include <initializer_list>

#include "fixedcapacity.hpp"
#include "chunk.hpp"
#include "cachedmeasure.hpp"
#include "bootchunkedseq.hpp"
#include "ftree.hpp"
#include "iterator.hpp"
#include "chunkedseqextras.hpp"

#ifndef _PASL_DATA_CHUNKEDBAG_H_
#define _PASL_DATA_CHUNKEDBAG_H_

namespace pasl {
namespace data {
namespace chunkedseq {
    
/***********************************************************************/
  
/*!
 * \class chunkedbag
 * \tparam Configuration type of the class from which to access configuration 
 * parameters for the container
 * \tparam Iterator type of the class to represent the iterator
 * 
 */
template <
  class Configuration,
  template <
    class Chunkedseq,
    class Configuration1
  >
  class Iterator=iterator::random_access
>
class chunkedbagbase {
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
  
  using const_self_pointer_type = const chunkedbagbase<Configuration>*;
  
  static constexpr int chunk_capacity = Configuration::chunk_capacity;
  
public:

  /*---------------------------------------------------------------------*/
  /** @name Container-configuration types
   */
  ///@{
  using config_type = Configuration;
  using self_type = chunkedbagbase<config_type>;
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

protected:

  // representation of the structure: two chunks plus a middle sequence of chunks
  chunk_type back_outer; // if outer is empty, then inner and middle must be empty
  chunk_type back_inner; // inner is either empty or full
  std::unique_ptr<middle_type> middle;    // middle contains only full chunks
  
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
                                   chunk_type& src, value_type& x, chunk_type& dst) {
    chunk_search_type chunk_search;
    return src.split(chunk_meas, p, chunk_search, middle_meas, prefix, x, dst);
  }
  
  /*---------------------------------------------------------------------*/
  
  bool is_buffer(const chunk_type* c) const {
    return c == &back_outer || c == &back_inner;
  }
  
  // take a chunk "c" and push its content into the back of the middle sequence
  // as a new chunk; leaving "c" empty.
  void push_buffer_back(chunk_type& c) {
    chunk_pointer d = chunk_alloc();
    c.swap(*d);
    middle->push_back(middle_meas, d);
  }

  // assumes that back_outer may be empty, while items may be stored elsewhere;
  // ensures that some items are stored in the back_outer buffer
  void restore_back_outer_empty_iff_all_empty() {
    if (back_outer.empty()) {
      if (! back_inner.empty()) {
        back_inner.swap(back_outer);
      } else if (! middle->empty()) {
        chunk_pointer c = middle->pop_back(middle_meas);
        back_outer.swap(*c);
        chunk_free(c);
      }
    }
    assert(! back_outer.empty() || (back_inner.empty() && middle->empty()) );
  }

  // assumes that both back_inner and back_outer buffer may be partially filled,
  // and ensure that the back_inner is either empty or full
  void restore_back_inner_full_or_empty() {
    size_t bisize = back_inner.size();
    size_t bosize = back_outer.size();
    if (bisize + bosize <= chunk_capacity) // if fits, put everything in back
      back_inner.transfer_from_front_to_back(chunk_meas, back_outer, bisize);
    else // if does not fit, fill up the inner buffer
      back_outer.transfer_from_front_to_back(chunk_meas, back_inner, chunk_capacity - bisize);
  }

  // ensures that back_inner is empty, by pushing it in the middle if it's full
  void ensure_empty_back_inner() {
    if (! back_inner.empty()) {
      chunk_pointer d = chunk_alloc();
      back_inner.swap(*d);
      middle->push_back(middle_meas, d);
    }
  }
  
  using const_chunk_pointer = const chunk_type*;
  
  const_chunk_pointer get_chunk_containing_last_item() const {
    if (! back_outer.empty())
      return &back_outer;
    if (! back_inner.empty())
      return &back_inner;
    if (! middle->empty())
      return middle->cback();
    return &back_outer;
  }

  using position_type = enum {
    found_back_outer,
    found_back_inner,
    found_middle,
    found_nowhere
  };
  
  template <class Pred>
  middle_measured_type search(const Pred& p, middle_measured_type prefix, position_type& pos) const {
    middle_measured_type cur = prefix; // prefix including current chunk
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
  template <class Pred>
  middle_measured_type search_for_chunk(const Pred& p, middle_measured_type prefix,
                                        bool& found, const_chunk_pointer& cur) const {
    position_type pos;
    prefix = search(p, prefix, pos);
    found = true; // default
    switch (pos) {
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
    ensure_empty_back_inner();
    copy_measure_to(other);
    position_type pos;
    prefix = search(p, prefix, pos);
    switch (pos) {
      case found_middle: {
        back_outer.swap(other.back_outer);
        chunk_pointer c;
        prefix = middle->split(middle_meas, p, prefix, c, *other.middle);
        back_outer.swap(*c);
        chunk_free(c);
        prefix = chunk_split(p, prefix, back_outer, x, other.back_inner);
        other.restore_back_inner_full_or_empty();
        break;
      }
      case found_back_inner: {
        assert(false); // thanks to the call to ensure_empty_back_inner()
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
    restore_back_outer_empty_iff_all_empty();
    other.restore_back_outer_empty_iff_all_empty();
    return prefix;
  }
  
  // precondition: other is empty
  template <class Pred>
  middle_measured_type split_aux(const Pred& p, middle_measured_type prefix, self_type& other) {
    size_type sz_orig = size();
    value_type x;
    prefix = split_aux(p, prefix, x, other);
    if (size_access::csize(prefix) < sz_orig)
      other.push(x);
    return prefix;
  }
  
  void init() {
    middle.reset(new middle_type());
  }

public:
  
  /*---------------------------------------------------------------------*/
  /** @name Constructors
   */
  ///@{
  
  chunkedbagbase() {
    init();
  }
 
  chunkedbagbase(const measure_type& meas)
  : chunk_meas(meas), middle_meas(meas) {
    init();
  }
  
  chunkedbagbase(const self_type& other)
  : back_inner(other.back_inner),
    back_outer(other.back_outer),
    chunk_meas(other.chunk_meas),
    middle_meas(other.middle_meas) {
    middle.reset(new middle_type(*other.middle));
    check();
  }
  
  chunkedbagbase(std::initializer_list<value_type> l) {
    init();
    for (auto it = l.begin(); it != l.end(); it++)
      push_back(*it);
  }
  
  ///@}
  
  /*---------------------------------------------------------------------*/
  /** @name Capacity
   */
  ///@{
  
  bool empty() const {
    return back_outer.empty();
  }
  
  size_type size() const {
    size_type sz = 0;
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
  
  value_type back() {
    return back_outer.back();
  }
  
  value_type front() {
    return back();
  }
  
  value_type top() {
    return back();
  }
  
  void backn(value_type* dst, size_type nb) const {
    extras::backn(*this, dst, nb);
  }
  
  void frontn(value_type* dst, size_type nb) const {
    extras::frontn(*this, dst, nb);
  }
  
  template <class Consumer>
  void stream_backn(const Consumer& cons, size_type nb) const {
    extras::stream_backn(*this, cons, nb);
  }
  
  template <class Consumer>
  void stream_frontn(const Consumer& cons, size_type nb) const {
    extras::stream_frontn(*this, cons, nb);
  }
  
  value_type operator[](size_type n) const {
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
  
  void push_back(const value_type& x) {
    if (back_outer.full()) {
      if (back_inner.full())
        push_buffer_back(back_inner);
      back_outer.swap(back_inner);
      assert(back_outer.empty());
    }
    back_outer.push_back(chunk_meas, x);
  }
  
  void push_front(const value_type& x) {
    push_back(x);
  }
  
  void push(const value_type& x) {
    push_back(x);
  }
  
  value_type pop_back() {
    value_type x = back_outer.back();
    back_outer.pop_back(chunk_meas);
    restore_back_outer_empty_iff_all_empty();
    return x;
  }
  
  value_type pop_front() {
    return pop_back();
  }
  
  value_type pop() {
    return pop_back();
  }
  
  void pushn_back(const_pointer src, size_type nb) {
    extras::pushn_back(*this, src, nb);
  }
  
  void pushn_front(const_pointer src, size_type nb) {
    extras::pushn_front(*this, src, nb);
  }
  
  void pushn(const_pointer src, size_type nb) {
    pushn_back(src, nb);
  }
  
  void popn_front(size_type nb) {
    auto c = [] (const_pointer, const_pointer) { };
    stream_popn_front<typeof(c), false>(c, nb);
  }
  
  void popn(size_type nb) {
    popn_back(nb);
  }
  
  void popn_back(size_type nb) {
    auto c = [] (const_pointer, const_pointer) { };
    stream_popn_back<typeof(c), false>(c, nb);
  }
  
  void popn_back(value_type* dst, size_type nb) {
    extras::popn_back(*this, dst, nb);
  }
  
  void popn_front(value_type* dst, size_type nb) {
    extras::popn_front(*this, dst, nb);
  }
  
  void popn(value_type* dst, size_type nb) {
    extras::popn_front(*this, dst, nb);
  }
  
  template <class Producer>
  void stream_pushn_back(const Producer& prod, size_type nb) {
    if (nb == 0)
      return;
    size_type sz_orig = size();
    ensure_empty_back_inner();
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
    restore_back_outer_empty_iff_all_empty();
    assert(sz_orig + nb == size());
  }
  
  template <class Producer>
  void stream_pushn_front(const Producer& prod, size_type nb) {
    stream_pushn_back(prod, nb);
  }
  
  template <class Producer>
  void stream_pushn(const Producer& prod, size_type nb) {
    stream_pushn_back(prod, nb);
  }
  
  template <class Consumer, bool should_consume=true>
  void stream_popn_back(const Consumer& cons, size_type nb) {
    size_type sz_orig = size();
    assert(sz_orig >= nb);
    size_type i = 0;
    while (i < nb) {
      restore_back_outer_empty_iff_all_empty();
      size_type m = std::min(back_outer.size(), nb - i);
      back_outer.template popn_back<Consumer,should_consume>(chunk_meas, cons, m);
      i += m;
    }
    restore_back_outer_empty_iff_all_empty();  // to restore invariants
    assert(sz_orig == size() + nb);
  }
  
  template <class Consumer, bool should_consume=true>
  void stream_popn_front(const Consumer& cons, size_type nb) {
    stream_popn_back(cons, nb);
  }
  
  template <class Consumer, bool should_consume=true>
  void stream_popn(const Consumer& cons, size_type nb) {
    stream_popn_back(cons, nb);
  }
  
  // concatenate data from other; leaving other empty
  void concat(self_type& other) {
    if (other.size() == 0)
      return;
    if (size() == 0)
      swap(other);
    // push inner buffers into the middle sequences
    ensure_empty_back_inner();
    other.ensure_empty_back_inner();
    // merge outer buffer of other
    back_inner.swap(other.back_outer);
    restore_back_inner_full_or_empty();
    // merge the middle sequences
    middle->concat(middle_meas, *other.middle);
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
      other.push(middle_item);
  }
  
  /* \brief Split by index
   *
   * The container is erased after and including the item at (zero-based) index `i`.
   *
   * The erased items are moved to the `other` container.
   *
   * \pre The `other` container is empty.
   */
  void split(size_type i, self_type& other) {
    return extras::split_by_index(*this, i, other);
  }
  
  /* The container is erased after and including the item at the
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
  
  iterator insert(iterator position, const value_type& val) {
    return extras::insert(*this, position, val);
  }
  
  void clear() {
    popn_back(size());
  }
  
  void swap(self_type& other) {
    std::swap(chunk_meas, other.chunk_meas);
    std::swap(middle_meas, other.middle_meas);
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
  
  iterator begin() const {
    return iterator(this, middle_meas, chunkedseq::iterator::begin);
  }
  
  iterator end() const {
    return iterator(this, middle_meas, chunkedseq::iterator::end);
  }

  template <class Body>
  void for_each(const Body& f) const {
    middle->for_each([&] (chunk_pointer p) {
      p->for_each(f);
    });
    back_inner.for_each(f);
    back_outer.for_each(f);
  }
  
  template <class Body>
  void for_each(iterator beg, iterator end, const Body& f) const {
    extras::for_each(beg, end, f);
  }
  
  template <class Body>
  void for_each_segment(const Body& f) const {
    middle->for_each([&] (chunk_pointer p) {
      p->for_each_segment(f);
    });
    back_inner.for_each_segment(f);
    back_outer.for_each_segment(f);
  }
  
  template <class Body>
  void for_each_segment(iterator begin, iterator end, const Body& f) const {
    extras::for_each_segment(begin, end, f);
  }
  
  ///@}
  
  /*---------------------------------------------------------------------*/
  /** @name Cached measurement
   */
  ///@{

  measured_type get_cached() const {
    measured_type m = algebra_type::identity();
    measured_type n = size_access::cclient(middle->get_cached());
    m = algebra_type::combine(m, n);
    m = algebra_type::combine(m, back_inner.get_cached());
    m = algebra_type::combine(m, back_outer.get_cached());
    return m;
  }
  
  measure_type get_measure() const {
    return chunk_meas;
  }
  
  void set_measure(measure_type meas) {
    chunk_meas = meas;
    middle_meas.set_client_measure(meas);
  }
  
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
    assert(size_access::csize(middle->get_cached()) == sz);
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
    if (! back_inner.empty())
      assert(back_inner.full());
    if (back_outer.empty()) {
      assert(back_inner.empty());
      assert(middle->empty());
    }
    size_t sprev = -1;
    middle->for_each([&sprev] (chunk_pointer& c) {
      size_t scur = c->size();
      if (sprev != -1)
        assert(sprev + scur >= chunk_capacity);
      sprev = scur;
    });
    check_size();
#endif
  }
  
  ///@}
  
};
  
/*---------------------------------------------------------------------*/
/* Place to put configuration classes for defining various
 * instantiations of the chunked-sequence functor
 */

template <
  class Item,
  int Chunk_capacity=512,
  class Client_cache=cachedmeasure::trivial<Item, size_t>, 
  template <
    class Chunk_item,
    int Cap,
    class Item_alloc
  >
  class Chunk_struct = fixedcapacity::heap_allocated::ringbuffer_ptr,
  template <
    class Top_item_base,
    int ___Chunk_capacity,
    class ___Cached_measure,
    class Top_item_deleter,
    class Top_item_copier,
    template <
      class ___Item,
      int ___Capacity,
      class ___Item_alloc
    >
    class ___Chunk_struct,
    class Size_access
  >
  class Middle_sequence = bootchunkedseq::cdeque,
  class Item_alloc=std::allocator<Item>
>
class basic_bag_configuration {
public:
  
  using size_type = typename Client_cache::size_type;
  using difference_type = ptrdiff_t;  // later: check if this is ok
  
  using value_type = Item;
  using segment_type = segment<value_type*>;
  
  static constexpr size_type chunk_capacity = size_type(Chunk_capacity);
  
  using item_allocator_type = Item_alloc;
  using item_queue_type = Chunk_struct<value_type, Chunk_capacity, item_allocator_type>;
  
  using client_algebra_type = typename Client_cache::algebra_type;
  using size_algebra_type = algebra::int_group_under_addition_and_negation<size_type>;
  using middle_algebra_type = algebra::combiner<size_algebra_type, client_algebra_type>;
  using middle_measured_type = typename middle_algebra_type::value_type;
  
#ifdef DISABLE_RANDOM_ACCESS_ITERATOR
  using cached_prefix_type = annotation::without_measured;
#else
  using cached_prefix_type = annotation::with_measured<middle_measured_type>;
#endif
#ifndef ENABLE_FINGER_SEARCH
  using parent_pointer_type = annotation::without_parent_pointer;
#else
  using parent_pointer_type = annotation::with_parent_pointer<middle_measured_type>;
#endif
  
  using chunk_cache_type = Client_cache;
  using chunk_measured_type = typename chunk_cache_type::measured_type;
  using annotation_type = annotation::annotation_builder<cached_prefix_type, parent_pointer_type>;

//  using annotation_type = annotation::annotation_builder<annotation::with_measured<middle_measured_type>>;
  using chunk_type = chunk<item_queue_type, chunk_cache_type, annotation_type>;
  
  class middle_cache_type {
  public:
    
    using size_type = typename Client_cache::size_type;
    using value_type = const chunk_type*;
    using algebra_type = middle_algebra_type;
    using measured_type = typename algebra_type::value_type;
    
    class measure_type {
    public:
      
      using client_measure_type = typename Client_cache::measure_type;
      using client_value_type = typename chunk_type::value_type;
      
    private:
      
      client_measure_type client_meas;
      
    public:
      
      measure_type() { }
      
      measure_type(const client_measure_type& client_meas)
      : client_meas(client_meas) { }
      
      measured_type operator()(const client_value_type& v) const {
        measured_type m = algebra_type::identity();
        m.value1 = size_type(1);
        m.value2 = client_meas(v);
        return m;
      }
      
      measured_type operator()(const client_value_type* lo, const client_value_type* hi) const {
        measured_type m = algebra_type::identity();
        for (auto p = lo; p < hi; p++)
          m = algebra_type::combine(m, operator()(*p));
        return m;
      }
      
      measured_type operator()(value_type p) const {
        measured_type m = algebra_type::identity();
        m.value1 = p->size();
        m.value2 = p->get_cached();
        return m;
      }
      
      measured_type operator()(const value_type* lo, const value_type* hi) const {
        measured_type m = algebra_type::identity();
        for (auto p = lo; p < hi; p++)
          m = algebra_type::combine(m, operator()(*p));
        return m;
      }
      
      client_measure_type get_client_measure() const {
        return client_meas;
      }
      
      void set_client_measure(client_measure_type client_meas) {
        this->client_meas = client_meas;
      }
      
    };
    
    static void swap(measured_type& x, measured_type& y) {
      std::swap(x.value1, y.value1);
      std::swap(x.value2, y.value2);
    }
    
  };
  
  using middle_measure_type = typename middle_cache_type::measure_type;
  
  class size_access {
  public:
    using client_measured_type = chunk_measured_type;
    
    static constexpr bool enable_index_optimization = true;
    
    static size_type& size(middle_measured_type& m) {
      return m.value1;
    }
    static size_type csize(middle_measured_type m) {
      return m.value1;
    }
    static client_measured_type& client(middle_measured_type& m) {
      return m.value2;
    }
    static client_measured_type cclient(middle_measured_type m) {
      return m.value2;
    }
  };

#ifdef DEBUG_MIDDLE_SEQUENCE
  //  use chunk as middle sequence; for the purpose of testing the chunkedbag functor in isolation.
  static constexpr int middle_capacity = 1<<23;
  using chunk_pointer_queue_type = fixedcapacity::heap_allocated::ringbuffer_ptr<chunk_type*, middle_capacity>;
  using middle_annotation_type = annotation::annotation_builder<>;
  using middle_type = chunk<chunk_pointer_queue_type, middle_cache_type,
                            middle_annotation_type, Pointer_deleter, Pointer_deep_copier, size_access>;
#else
  static constexpr int middle_chunk_capacity = 32;
  using middle_type = Middle_sequence<chunk_type, middle_chunk_capacity, middle_cache_type,
      Pointer_deleter, Pointer_deep_copier, fixedcapacity::heap_allocated::ringbuffer_ptr, size_access>;
#endif

  using chunk_search_type = itemsearch::search_in_chunk<chunk_type, middle_algebra_type, size_access>;

};
  
/*---------------------------------------------------------------------*/
/* Instantiation for the bootstrapped chunked sequence */
  
namespace bootstrapped {
  
// Application of chunked bag to a configuration

template <class Item,
          int Chunk_capacity = 512,
          class Cache = cachedmeasure::trivial<Item, size_t>,
          class Item_alloc = std::allocator<Item>>
using bagopt = chunkedbagbase<basic_bag_configuration<Item, Chunk_capacity, Cache, fixedcapacity::heap_allocated::stack, bootchunkedseq::cdeque, Item_alloc>>;
  
} // end namespace

/*---------------------------------------------------------------------*/
/* Instantiation for the finger tree */
  
namespace ftree {
  
template <
  class Item,
  int Chunk_capacity = 512,
  class Cache = cachedmeasure::trivial<Item, size_t>,
  class Item_alloc = std::allocator<Item>>
using bagopt = chunkedbagbase<basic_bag_configuration<Item, Chunk_capacity, Cache, fixedcapacity::heap_allocated::stack, ::pasl::data::ftree::tftree, Item_alloc>>;
  
} // end namespace

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_CHUNKEDBAG_H_ */

