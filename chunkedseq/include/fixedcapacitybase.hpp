/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Implementations of various fixed-capacity buffers
 * \file fixedcapacitybase.hpp
 *
 */

#include <assert.h>
#include <memory>
#include <cstring>
#include <type_traits>
#include <algorithm>

#include "segment.hpp"

#ifndef _PASL_DATA_FIXEDCAPACITYBASE_H_
#define _PASL_DATA_FIXEDCAPACITYBASE_H_

namespace pasl {
namespace data {
namespace fixedcapacity {
namespace base {

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Array allocation */

template <class Item, int Capacity>
class heap_allocator {
private:
  
  class Deleter {
  public:
    void operator()(Item* items) {
      free(items);
    }
  };
  
  std::unique_ptr<Item[], Deleter> items;
  
  // to disable copying
  heap_allocator(const heap_allocator& other);
  heap_allocator& operator=(const heap_allocator& other);
  
public:
  
  using value_type = Item;
  using self_type = heap_allocator<Item, Capacity>;
  
  static constexpr int capacity = Capacity;
  
  heap_allocator() {
    Item* p = (value_type*)malloc(sizeof(value_type) * capacity);
    assert(p != NULL);
    items.reset(p);
  }
  
  // move assignment operator
  heap_allocator(self_type&& x) = default;
  self_type& operator=(self_type&& a) = default;
  
  value_type& operator[](int i) const {
    assert(items != NULL);
    assert(i >= 0);
    return items[i];
  }
  
  void swap(heap_allocator& other) {
    std::swap(items, other.items);
  }
  
};

template <class Item, int Capacity>
class inline_allocator {
private:
  
  mutable Item items[Capacity];
  
public:
  
  typedef Item value_type;
  
  static constexpr int capacity = Capacity;
  
  value_type& operator[](int n) const {
    assert(n >= 0);
    assert(n < capacity);
    return items[n];
  }
  
  void swap(inline_allocator& other) {
    value_type tmp_items[capacity];
    for (int i = 0; i < capacity; i++)
      tmp_items[i] = items[i];
    for (int i = 0; i < capacity; i++)
      items[i] = other.items[i];
    for (int i = 0; i < capacity; i++)
      other.items[i] = tmp_items[i];
  }
  
};

/*---------------------------------------------------------------------*/
/* Data movement */

/*! \brief Polymorphic array copy
 *
 * Copies `num` items from the location pointed to by `source`
 * directly to the memory block pointed to by `destination`.
 *
 * \tparam Type of the allocator object used to define the storage
 * allocation model.
 * \param destination Pointer to the destination array where the
 * content is to be copied
 * \param source Pointer to the source of data to be copied
 * \param num number of items to copy
 *
 */

template <class Alloc>
void copy(typename Alloc::pointer destination,
             typename Alloc::const_pointer source,
             typename Alloc::size_type num) {
  // ranges must not intersect
  assert(! (source+num >= destination+1 && destination+num >= source+1));
  std::copy(source, source+num, destination);
}

template <class Alloc>
void pblit(typename Alloc::const_pointer t1, int i1,
           typename Alloc::pointer t2, int i2,
           int nb) {
  copy<Alloc>(t2 + i2, t1 + i1, nb);
}

template <class Alloc>
void destroy_items(typename Alloc::pointer t, int i, int nb) {
  typedef typename Alloc::size_type size_type;
  Alloc alloc;
  for (size_type k = 0; k < nb; k++)
    alloc.destroy(&t[k + i]);
}

template <class Alloc>
void destroy_items(typename Alloc::pointer t, int nb) {
  destroy_items<Alloc>(t, 0, nb);
}

/*! \brief Polymorphic shift by position
 *
 * Moves the first `num` items of the given array pointed at by `t`
 * forward or backward in the same array by `shift_by`
 * positions. The direction of the shift is determined
 * by whether `shift_by` is positive or negative.
 *
 * To avoid overflows, the size of the array `t` shall be at least
 * `num + abs(shift_by)` items.
 *
 * \tparam Type of the allocator object used to define the storage
 * allocation model.
 * \param destination Pointer to the destination array where the
 * content is to be copied
 * \param source Pointer to the source of data to be copied
 * \param num number of items to copy
 *
 */
template <class Alloc>
void pshiftn(typename Alloc::pointer t,
             typename Alloc::size_type num,
             int shift_by) {
  if (shift_by == 0)
    return;
  if (shift_by < 0) {
    for (int i = 0; i < num; i++)
      t[i+shift_by] = t[i];
  } else {
    std::copy_backward(t, t + num, t + num + shift_by);
  }
}

/*! \brief Polymorphic fill range with value
 *
 * Behavior is exactly the same as that of `std::fill`.
 *
 */
template <class Alloc>
void pfill(typename Alloc::pointer first,
           typename Alloc::pointer last,
           typename Alloc::const_reference val) {
  std::fill(first, last, val);
}

/*---------------------------------------------------------------------*/
/* Data movement for fixed-capacity arrays with possible wraparound */

/* copy n elements from an array t1 of size capacity,
 * starting at index i1 and possibly wrapping around, into an
 * array t2 starting at index i2 and not wrapping around.
 */
template <class Alloc, int capacity>
void copy_data_wrap_src(typename Alloc::const_pointer t1, int i1,
                        typename Alloc::pointer t2, int i2,
                        int nb) {
  int j1 = i1 + nb;
  if (j1 <= capacity) {
    pblit<Alloc>(t1, i1, t2, i2, nb);
  } else {
    int na = capacity - i1;
    int i2_n = (i2 + na) % capacity;
    pblit<Alloc>(t1, i1, t2, i2, na);
    pblit<Alloc>(t1, 0, t2, i2_n, nb - na);
  }
}

/* copy n elements from an array t1 starting at index i1
 * and not wrapping around, into an array t2 of size capacity,
 * starting at index i2 and possibly wrapping around.
 */
template <class Alloc, int capacity>
void copy_data_wrap_dst(typename Alloc::const_pointer t1, int i1,
                        typename Alloc::pointer t2, int i2,
                        int nb) {
  int j2 = i2 + nb;
  if (j2 <= capacity) {
    pblit<Alloc>(t1, i1, t2, i2, nb);
  } else {
    int na = capacity - i2;
    int i1_n = (i1 + na) % capacity;
    pblit<Alloc>(t1, i1, t2, i2, na);
    pblit<Alloc>(t1, i1_n, t2, 0, nb - na);
  }
}

/* copy n elements from an array t1 starting at index i1
 * and possibly wrapping around, into an array t2 starting at index
 * i2 and possibly wrapping around. Both arrays are assumed to be
 * of size capacity.
 */
template <class Alloc, int capacity>
void copy_data_wrap_src_and_dst(typename Alloc::const_pointer t1, int i1,
                                typename Alloc::pointer t2, int i2,
                                int nb) {
  int j1 = i1 + nb;
  if (j1 <= capacity) {
    copy_data_wrap_dst<Alloc, capacity>(t1, i1, t2, i2, nb);
  } else {
    int na = capacity - i1;
    int i2_n = (i2 + na) % capacity;
    copy_data_wrap_dst<Alloc, capacity>(t1, i1, t2, i2, na);
    copy_data_wrap_src_and_dst<Alloc, capacity>(t1, 0, t2, i2_n, nb - na);
  }
}

/* calls the destructor of the first nb items starting at position i
 * in the circular buffer pointed to by t, possibly wrapping around.
 */
template <class Alloc, int capacity>
void destroy_items_wrap_target(typename Alloc::pointer t, int i, int nb) {
  int j = i + nb;
  if (j <= capacity) {
    destroy_items<Alloc>(t, i, nb);
  } else {
    int na = capacity - i;
    destroy_items<Alloc>(t, i, na);
    destroy_items<Alloc>(t, 0, nb - na);
  }
}

/*---------------------------------------------------------------------*/
/* Loops */

/*! \brief Loop body for array-initialization by constant
 *
 * Implements the interface \ref foreach_loop_body
 *
 * This loop body is a special case which writes a constant
 * value `v` to each cell in the container.
 */
template <class Alloc>
class const_foreach_body {
public:
  
  typedef Alloc allocator_type;
  typedef typename Alloc::value_type value_type;
  typedef typename Alloc::reference reference;
  typedef typename Alloc::const_reference const_reference;
  typedef typename Alloc::size_type size_type;
  
  value_type v;
  
  const_foreach_body(const_reference v) : v(v) { }
  
  void operator()(size_type i, reference dst) const {
    dst = v;
  }
  
};

/*! \brief Loop body for array tabulation
 *
 * Implements the interface \ref foreach_loop_body
 *
 * This loop body handles the general case.
 */
template <class Alloc, class Body>
class apply_foreach_body {
public:
  typedef Alloc allocator_type;
  typedef typename Alloc::size_type size_type;
  typedef typename Alloc::reference reference;
  
  Body body;
  size_type start;
  
  apply_foreach_body(const Body& body, size_type start = 0)
  : body(body), start(start) { }
  
  void operator()(size_type i, reference dst) const {
    body(dst);
  }
  
};

/*! \brief Polymorphic apply-to-each item
 *
 * Iteratively applies the client-supplied function
 *  body(k, t[0]); body(k+1, t[1]); ... body(k+num-1 t[num-1]);
 *
 * \tparam Type of the loop-body class. This class must implement
 * the interface defined by \ref foreach_loop_body.
 * \param t pointer to the start of the destination array
 * \param num number of cells to initialize
 * \param k logical start position
 * \param body loop-body function
 *
 */
template <class Body>
void papply(typename Body::allocator_type::pointer t,
            typename Body::allocator_type::size_type num,
            typename Body::allocator_type::size_type k,
            const Body& body) {
  typedef typename Body::allocator_type::size_type size_type;
  for (size_type i = 0; i < num; i++)
    body(k + i, t[i]);
}

/* special case which more efficiently initializes the array with a
 * constant value
 */
template <class Body>
void papply(typename Body::allocator_type::pointer t,
            typename Body::allocator_type::size_type num,
            typename Body::allocator_type::size_type k,
            const const_foreach_body<typename Body::allocator_type>& body) {
  typedef typename Body::allocator_type::value_type value_type;
  value_type val;
  body(0, val);
  pfill<typename Body::allocator_type>(t, t + num, val);
}

/* applies the client-supplied function to the first nb cells
 * starting at index i, possibly wrapping around.
 */
template <class Body, int capacity>
void papply_wrap_dst(typename Body::allocator_type::pointer t,
                     int i,
                     int nb,
                     typename Body::allocator_type::size_type k,
                     const Body& body) {
  int j = i + nb;
  if (j <= capacity) {
    papply<Body>(t + i, nb, k, body);
  } else {
    int na = capacity - i;
    papply<Body>(t + i, na, k, body);
    papply<Body>(t, nb - na, na, body);
  }
}

template <class Body, int capacity>
void papply_wrap_dst(typename Body::allocator_type::pointer t,
                     int i,
                     int nb,
                     const Body& body) {
  papply_wrap_dst<Body, capacity>(t, i, nb, 0, body);
}

/*---------------------------------------------------------------------*/
/* Ring buffer based on indices */

/*!
 * \class ringbuffer_idx
 * \brief Fixed-capacity ring buffer, using indices
 *
 * Container properties
 * ============
 *
 * \tparam Array_alloc Type of the allocator object used to define the
 * storage policy of the array that is used by the stack to store
 * the items of the stack.
 * \tparam Item_alloc Type of the allocator object used to define the
 * storage allocation model. By default, the allocator class template is
 * used, which defines the simplest memory allocation model and is
 * value-independent. Aliased as member type vector::allocator_type.
 *
 */
template <class Array_alloc,
class Item_alloc = std::allocator<typename Array_alloc::value_type> >
class ringbuffer_idx {
public:
  
  typedef int size_type;
  typedef typename Array_alloc::value_type value_type;
  typedef Item_alloc allocator_type;
  typedef segment<value_type*> segment_type;
  
  static constexpr int capacity = Array_alloc::capacity;
  
private:
  
  int fr;
  int sz;
  Item_alloc alloc;
  Array_alloc array;
  
public:
  
  ringbuffer_idx()
  : fr(0), sz(0) {
  }
  
  ringbuffer_idx(const ringbuffer_idx& other)
  : fr(other.fr), sz(other.sz) {
    copy_data_wrap_src_and_dst<Item_alloc, capacity>(&other.array[0], other.fr, &array[0], fr, other.sz);
  }
  
  ringbuffer_idx(size_type nb, const value_type& val) {
    pushn_back(const_foreach_body<Item_alloc>(val), nb);
  }
  
  ~ringbuffer_idx() {
    clear();
  }
  
  inline int size() const {
    return sz;
  }
  
  inline bool full() const {
    return size() == capacity;
  }
  
  inline bool empty() const {
    return size() == 0;
  }
  
  inline bool partial() const {
    return !empty() && !full(); // TODO: could be optimized
  }
  
  inline void push_front(const value_type& x) {
    assert(! full());
    fr--;
    if (fr == -1)
      fr += capacity;
    sz++;
    alloc.construct(&array[fr], x);
  }
  
  inline void push_back(const value_type& x) {
    assert(! full());
    int bk = (fr + sz);
    if (bk >= capacity)
      bk -= capacity;
    sz++;
    alloc.construct(&array[bk], x);
  }
  
  inline value_type& front() const {
    assert(! empty());
    return array[fr];
  }
  
  inline value_type& back() const {
    assert(! empty());
    int bk = fr + sz - 1;
    if (bk >= capacity)
      bk -= capacity;
    return array[bk];
  }
  
  inline value_type pop_front() {
    assert(! empty());
    value_type v = front();
    alloc.destroy(&(front()));
    fr++;
    if (fr == capacity)
      fr = 0;
    sz--;
    return v;
  }
  
  inline value_type pop_back() {
    assert(! empty());
    value_type v = back();
    alloc.destroy(&(back()));
    sz--;
    return v;
  }
  
  /* DEPRECATED: using modulo is too slow
   inline void push_front(const value_type& x) {
   assert(! full());
   fr = (fr - 1 + capacity) % capacity;
   sz++;
   alloc.construct(&array[fr], x);
   }
   
   inline void push_back(const value_type& x) {
   assert(! full());
   int bk = (fr + sz) % capacity;
   sz++;
   alloc.construct(&array[bk], x);
   }
   
   inline value_type& front() const {
   assert(! empty());
   return array[fr];
   }
   
   inline value_type& back() const {
   assert(! empty());
   int bk = (fr + sz - 1) % capacity;
   return array[bk];
   }
   
   inline value_type pop_front() {
   assert(! empty());
   value_type v = front();
   alloc.destroy(&(front()));
   fr = (fr + 1) % capacity;
   sz--;
   return v;
   }
   
   inline value_type pop_back() {
   assert(! empty());
   value_type v = back();
   alloc.destroy(&(back()));
   sz--;
   return v;
   }
   */
  
  //! \todo: remove modulo operations below, using a "wrap" function.
  
  void frontn(value_type* dst, int nb) {
    assert(size() >= nb);
    copy_data_wrap_src<Item_alloc, capacity>(&array[0], fr, dst, 0, nb);
  }
  
  void backn(value_type* dst, int nb) {
    assert(size() >= nb);
    int i_new = (fr + sz - nb) % capacity;
    copy_data_wrap_src<Item_alloc, capacity>(&array[0], i_new, dst, 0, nb);
  }
  
  void pushn_front(const value_type* xs, int nb) {
    assert(nb + size() <= capacity);
    int fr_new = (fr - nb + capacity) % capacity;
    copy_data_wrap_dst<Item_alloc, capacity>(xs, 0, &array[0], fr_new, nb);
    fr = fr_new;
    sz += nb;
  }
  
  void pushn_back(const value_type* xs, int nb) {
    assert(nb + size() <= capacity);
    int i = (fr + sz) % capacity;
    copy_data_wrap_dst<Item_alloc, capacity>(xs, 0, &array[0], i, nb);
    sz += nb;
  }
  
  void popn_front(int nb) {
    assert(size() >= nb);
    destroy_items_wrap_target<Item_alloc, capacity>(&array[0], fr, nb);
    fr = (fr + nb) % capacity;
    sz -= nb;
  }
  
  template <class Body>
  void pushn_back(const Body& body, int nb) {
    assert(nb + size() <= capacity);
    int i = (fr + sz) % capacity;
    papply_wrap_dst<Body, capacity>(&array[0], i, nb, body);
    sz += nb;
  }
  
  void popn_back(int nb) {
    assert(size() >= nb);
    int i_new = (fr + sz - nb) % capacity;
    destroy_items_wrap_target<Item_alloc, capacity>(&array[0], i_new, nb);
    sz -= nb;
  }
  
  void popn_front(value_type* dst, int nb) {
    frontn(dst, nb);
    popn_front(nb);
  }
  
  void popn_back(value_type* dst, int nb) {
    backn(dst, nb);
    popn_back(nb);
  }
  
  void transfer_from_back_to_front(ringbuffer_idx& target, int nb) {
    int i1 = (fr + sz - nb) % capacity;
    int i2 = (target.fr - nb + capacity) % capacity;
    copy_data_wrap_src_and_dst<Item_alloc, capacity>(&array[0], i1, &target.array[0], i2, nb);
    sz -= nb;
    target.sz += nb;
    target.fr = i2;
  }
  
  void transfer_from_front_to_back(ringbuffer_idx& target, int nb) {
    int i1 = fr;
    int i2 = (target.fr + target.sz) % capacity;
    copy_data_wrap_src_and_dst<Item_alloc, capacity>(&array[0], i1, &target.array[0], i2, nb);
    sz -= nb;
    target.sz += nb;
    fr = (i1 + nb) % capacity;
  }
  
private:
  
  static value_type& subscript(value_type* array, int fr, int sz, int ix) {
    assert(array != NULL);
    assert(ix >= 0);
    assert(ix < sz);
    return array[(fr + ix) % capacity];
  }
  
public:
  
  value_type& operator[](int ix) const {
    return subscript(&array[0], fr, size(), ix);
  }
  
  value_type& operator[](size_t ix) const {
    return subscript(&array[0], fr, size(), (int)ix);
  }
  
  void clear() {
    popn_back(size());
  }
  
  void swap(ringbuffer_idx& other) {
    std::swap(fr, other.fr);
    std::swap(sz, other.sz);
    array.swap(other.array);
  }
  
  segment_type segment_by_index(int i) const {
    assert(i >= 0);
    assert(i < size());
    value_type* p = subscript(&array[0], fr, size(), i);
    value_type* fr_ptr = &front();
    value_type& bk_ptr = &back();
    return segment_of_ringbuffer(p, fr_ptr, bk_ptr, &array[0], capacity);
  }
  
  int index_of_pointer(const value_type* p) const {
    assert(p >= &array[0]);
    assert(p <= &array[capacity-1]);
    value_type* first = &array[fr];
    if (p < first) { // wraparound
      int nb1 = capacity - fr;
      int nb2 = (int)(p - &array[0]);
      return nb1 + nb2;
    } else {
      return (int)(p - first);
    }
  }
  
  template <class Body>
  void for_each(const Body& body) const {
    int bk = fr + sz;
    if (bk <= capacity) {
      for (int i = fr; i < bk; i++)
        body(array[i]);
    } else {
      for (int i = fr; i < capacity; i++)
        body(array[i]);
      for (int i = 0; i < (bk - capacity); i++)
        body(array[i]);
    }
  }
  
  template <class Body>
  void for_each_segment(int lo, int hi, const Body& body) {
    if (hi - lo <= 0 || hi < 1)
      return;
    segment_type seg1 = segment_by_index(int(lo));
    segment_type seg2 = segment_by_index(int(hi - 1));
    if (seg1.begin == seg2.begin) {
      body(seg1.middle, seg2.middle + 1);
    } else {
      body(seg1.middle, seg1.end);
      body(seg2.begin, seg2.middle + 1);
    }
  }
  
};

/*---------------------------------------------------------------------*/
/* Ring buffer based on pointers */

/*!
 *  \class ringbuffer_ptr
 *  \brief Fixed-capacity ring buffer
 *
 * Container properties
 * ============
 *
 * \warning The capacity equals `Array_alloc::capacity - 1`, because the
 * ringbuffer always leaves one cell empty in order to distinguish between
 * the empty and the full container.
 *
 * \tparam Array_alloc Type of the allocator object used to define the
 * storage policy of the array that is used by the stack to store
 * the items of the stack.
 * \tparam Item_alloc Type of the allocator object used to define the
 * storage allocation model. By default, the allocator class template is
 * used, which defines the simplest memory allocation model and is
 * value-independent. Aliased as member type vector::allocator_type.
 *
 */

template <class Array_alloc,
class Item_alloc = std::allocator<typename Array_alloc::value_type> >
class ringbuffer_ptr {
public:
  
  typedef typename Array_alloc::value_type value_type;
  static constexpr int capacity = Array_alloc::capacity - 1;
  // max number of items that can be stored
  // recall that at least one cell must be empty
  
private:
  
  value_type* fr; // address of the cell storing the front item
  value_type* bk; // address of the cell storing the back item
  Item_alloc alloc;
  Array_alloc array;
  
  // when container empty, bk is just one cell before back (up to wrap-around)
  // when container is full, there is exactly one empty cell between bk and fr
  
  // number of cells in the array
  static constexpr int nb_cells = Array_alloc::capacity;
  
  // address of the first cell of the array
  inline value_type* beg() const {
    return &array[0];
  }
  
  // address of the last cell of the array
  inline value_type* end() const {
    return &array[nb_cells-1];
  }
  
  inline void check() const {
    assert(bk >= beg());
    assert(fr >= beg());
    assert(bk <= end());
    assert(fr <= end());
  }
  
  inline void init() {
    fr = beg();
    bk = end();
  }
  
  inline value_type* next(value_type* p) const {
    if (p == end())
      return beg();
    else
      return p + 1;
  }
  
  inline value_type* nextn(value_type* p, int nb) const {
    assert(0 <= nb && nb <= nb_cells);
    value_type* q = p + nb;
    if (q <= end())
      return q;
    else
      return q - nb_cells;
  }
  
  inline value_type* prev(value_type* p) const {
    if (p == beg())
      return end();
    else
      return p - 1;
  }
  
  inline value_type* prevn(value_type* p, int nb) const {
    assert(0 <= nb && nb <= nb_cells);
    value_type* q = p - nb;
    if (q >= beg())
      return q;
    else
      return q + nb_cells;
  }
  
  // Remark: "ix" denotes an index in the array, whereas "i" is an index in the buffer
  inline int array_index_of_pointer(const value_type* p) const {
    assert(p >= beg());
    assert(p <= end());
    int ix = (int) (p - beg());
    assert(0 <= ix);
    assert (ix < nb_cells);
    return ix;
  }
  
public:
  
  typedef int size_type;
  typedef Item_alloc allocator_type;
  typedef segment<value_type*> segment_type;
  typedef ringbuffer_ptr self_type;
  
  ringbuffer_ptr() {
    init();
  }
  
  ringbuffer_ptr(const ringbuffer_ptr& other) {
    init();
    int other_sz = other.size();
    if (other_sz == 0)
      return;
    int other_ix = other.array_index_of_pointer(other.fr);
    int ix = array_index_of_pointer(fr);
    copy_data_wrap_src_and_dst<allocator_type, nb_cells>(other.beg(), other_ix, beg(), ix, other_sz);
    bk = nextn(bk, other_sz);
    check();
  }
  
  ringbuffer_ptr(size_type nb, const value_type& val) {
    init();
    pushn_back(const_foreach_body<Item_alloc>(val), nb);
    check();
  }
  
  ~ringbuffer_ptr() {
    check();
    clear();
    check();
  }
  
  // converts an index in the buffer into a pointer on a cell
  inline value_type* pointer_of_index(int i) const {
    assert(0 <= i && i < size());
    return nextn(fr, i);
  }
  
  // converts a pointer on a cell into an index in the buffer;
  // remark: if the array is empty, then the return value is unspecified
  inline int index_of_pointer(const value_type* p) const {
    assert(p >= beg());
    assert(p <= end());
    int i = int(p - fr);
    if (i < 0)
      i += nb_cells;
    assert(0 <= i);
    assert(i < nb_cells); // we purposely allow i >= size() here
    assert(i <= size());
    return i;
  }
  
  inline int size() const {
    int nb = int(bk - fr) + 1;
    if (nb < 0)
      nb += nb_cells;
    if (nb == nb_cells)
      return 0;
    else
      return nb;
  }
  
  inline bool full() const {
    return (bk + 2 == fr) || (bk + 2 - nb_cells == fr);
    // very slow:
    //   return (size() == capacity);
    // alternative:
    //   return (nextn(bk, 2) == fr);
  }
  
  inline bool empty() const {
    return (bk + 1 == fr) || (bk + 1 - nb_cells == fr);
    // very slow:
    //   return (size() == 0);
    // slow:
    // int nb = bk - fr + 1;
    // return (nb == 0) || (nb == nb_cells);
  }
  
  inline bool partial() const {
    return !empty() && !full(); // TODO: could be optimized
  }
  
  inline value_type& front() const {
    assert(! empty());
    return *fr;
  }
  
  inline value_type& back() const {
    assert(! empty());
    return *bk;
  }
  
  inline void push_front(const value_type& x) {
    assert(! full());
    fr = prev(fr);
    alloc.construct(fr, x);
  }
  
  inline void push_back(const value_type& x) {
    assert(! full());
    bk = next(bk);
    alloc.construct(bk, x);
  }
  
  inline value_type pop_front() {
    assert(! empty());
    value_type v = *fr;
    alloc.destroy(fr);
    fr = next(fr);
    return v;
  }
  
  inline value_type pop_back() {
    assert(! empty());
    value_type v = *bk;
    alloc.destroy(bk);
    bk = prev(bk);
    return v;
  }
  
  void frontn(value_type* dst, int nb) const {
    assert(0 <= nb && nb <= size());
    int ix = array_index_of_pointer(fr);
    copy_data_wrap_src<Item_alloc, nb_cells>(beg(), ix, dst, 0, nb);
  }
  
  void backn(value_type* dst, int nb) const {
    assert(0 <= nb && nb <= size());
    if (nb == 0)
      return;
    value_type* p_start = prevn(bk, nb-1); // nb-1 because bk is inclusive
    int ix = array_index_of_pointer(p_start);
    copy_data_wrap_src<Item_alloc, nb_cells>(beg(), ix, dst, 0, nb);
  }
  
  void pushn_front(const value_type* src, int nb) {
    assert(0 <= nb && nb + size() <= capacity);
    fr = prevn(fr, nb);
    int ix = array_index_of_pointer(fr);
    copy_data_wrap_dst<Item_alloc, nb_cells>(src, 0, beg(), ix, nb);
  }
  
  void pushn_back(const value_type* src, int nb) {
    assert(0 <= nb && nb + size() <= capacity);
    int ix = array_index_of_pointer(next(bk)); // next(bk) because bk is inclusive
    copy_data_wrap_dst<Item_alloc, nb_cells>(src, 0, beg(), ix, nb);
    bk = nextn(bk, nb);
  }
  
  //! \todo: add a comment on the signature of body
  template <class Body>
  void pushn_back(const Body& body, int nb) {
    assert(0 <= nb && nb + size() <= capacity);
    int ix = array_index_of_pointer(next(bk)); // next(bk) because bk is inclusive
    papply_wrap_dst<Body, nb_cells>(beg(), ix, nb, body);
    bk = nextn(bk, nb);
  }
  
  void popn_front(int nb) {
    assert(0 <= nb && nb <= size());
    int ix = array_index_of_pointer(fr);
    destroy_items_wrap_target<Item_alloc, nb_cells>(beg(), ix, nb);
    fr = nextn(fr, nb);
  }
  
  void popn_back(int nb) {
    assert(0 <= nb && nb <= size());
    bk = prevn(bk, nb);
    int ix = array_index_of_pointer(next(bk)); // next(bk) because bk is inclusive
    destroy_items_wrap_target<Item_alloc, nb_cells>(beg(), ix, nb);
  }
  
  void popn_front(value_type* dst, int nb) {
    frontn(dst, nb);
    popn_front(nb);
  }
  
  void popn_back(value_type* dst, int nb) {
    backn(dst, nb);
    popn_back(nb);
  }
  
  void transfer_from_back_to_front(ringbuffer_ptr& target, int nb) {
    assert(0 <= nb && nb <= size());
    bk = prevn(bk, nb);
    target.fr = target.prevn(target.fr, nb);
    int i1 = array_index_of_pointer(next(bk));
    int i2 = target.array_index_of_pointer(target.fr);
    copy_data_wrap_src_and_dst<Item_alloc, nb_cells>(beg(), i1, target.beg(), i2, nb);
    check();
    target.check();
  }
  
  void transfer_from_front_to_back(ringbuffer_ptr& target, int nb) {
    assert(0 <= nb && nb <= size());
    int i1 = array_index_of_pointer(fr);
    int i2 = target.array_index_of_pointer(target.next(target.bk));
    copy_data_wrap_src_and_dst<Item_alloc, nb_cells>(beg(), i1, target.beg(), i2, nb);
    fr = nextn(fr, nb);
    target.bk = target.nextn(target.bk, nb);
    check();
    target.check();
  }
  
  value_type& operator[](int i) const {
    return *(pointer_of_index(i));
  }
  
  value_type& operator[](size_t i) const {
    return (*this)[int(i)];
  }
  
  void clear() {
    popn_back(size());
  }
  
  void swap(ringbuffer_ptr& other) {
    std::swap(fr, other.fr);
    std::swap(bk, other.bk);
    array.swap(other.array);
  }
  
  segment_type segment_by_index(int i) const {
    assert(i >= 0);
    assert(i < size());
    value_type* p = pointer_of_index(i);
    return segment_of_ringbuffer(p, fr, bk, beg(), nb_cells);
  }
  
  //! \todo: comment on the signature of body
  template <class Body>
  void for_each(const Body& body) const {
    auto _body = apply_foreach_body<Item_alloc, Body>(body);
    int ix = array_index_of_pointer(fr);
    int sz = size();
    papply_wrap_dst<typeof(_body), nb_cells>(beg(), ix, sz, 0, _body);
  }
  
  //! \todo: comment on the signature of body
  template <class Body>
  void for_each_segment(int lo, int hi, const Body& body) const {
    /* new code --tofix:
     if (hi - lo <= 0 || hi < 1)
     return;
     assert(0 <= lo && lo < hi && hi <= size()); // correct if did not return above
     Size first = lo;
     Size last = hi-1;
     value_type* p_first = pointer_of_index(int(first));
     value_type* p_last = pointer_of_index(int(last));
     if (bk >= fr) {
     body(p_first, p_last + 1);
     } else {
     body(p_first, end() + 1);
     body(beg(), p_last + 1);
     }
     */
    if (hi - lo <= 0 || hi < 1)
      return;
    segment_type seg1 = segment_by_index(int(lo));
    segment_type seg2 = segment_by_index(int(hi - 1));
    if (seg1.begin == seg2.begin) {
      body(seg1.middle, seg2.middle + 1);
    } else {
      body(seg1.middle, seg1.end);
      body(seg2.begin, seg2.middle + 1);
    }
  }
  
};

/*---------------------------------------------------------------------*/
/* Ring buffer based on pointers */

/*!
 *  \class ringbuffer_ptrx
 *  \brief Fixed-capacity ring buffer
 *
 * Container properties
 * ============
 *
 * \warning The capacity equals `Array_alloc::capacity - 1`, because the
 * ringbuffer always leaves one cell empty in order to distinguish between
 * the empty and the full container.
 *
 * \tparam Array_alloc Type of the allocator object used to define the
 * storage policy of the array that is used by the stack to store
 * the items of the stack.
 * \tparam Item_alloc Type of the allocator object used to define the
 * storage allocation model. By default, the allocator class template is
 * used, which defines the simplest memory allocation model and is
 * value-independent. Aliased as member type vector::allocator_type.
 *
 */

template <class Array_alloc,
class Item_alloc = std::allocator<typename Array_alloc::value_type> >
class ringbuffer_ptrx {
public:
  
  typedef typename Array_alloc::value_type value_type;
  static constexpr int capacity = Array_alloc::capacity - 1;
  // max number of items that can be stored
  // recall that at least one cell must be empty
  
private:
  
  static constexpr int nbcells = Array_alloc::capacity; // number of cells
  
  value_type* fr; // empty cell before front item
  value_type* bk; // empty cell after back item
  Item_alloc alloc;
  Array_alloc array;
  
  // returns pointer to first allocated cell
  inline value_type* beg() const {
    return &array[0];
  }
  
  // returns pointer to last allocated cell
  inline value_type* end() const {
    return &array[nbcells-1];
  }
  
  void check(value_type* fr, value_type* bk) const {
    assert(bk >= beg());
    assert(fr >= beg());
    assert(bk <= end());
    assert(fr <= end());
  }
  
  void init() {
    fr = &array[nbcells-1];
    bk = &array[0];
    check(fr, bk);
  }
  
  int array_index_of_front(value_type* fr) const {
    return array_index_of_pointer(addr_of_front(fr));
  }
  
  int array_index_of_back(value_type* bk) const {
    return array_index_of_pointer(addr_of_back(bk));
  }
  
  int array_index_of_pointer(const value_type* p) const {
    assert(p >= beg());
    assert(p <= end());
    return (int)(p - beg());
  }
  
  value_type* addr_of_front(value_type* fr) const {
    value_type* addr;
    if (fr == end())
      addr = beg();
    else
      addr = fr + 1;
    return addr;
  }
  
  value_type* addr_of_back(value_type* bk) const {
    value_type* addr;
    if (bk == beg())
      addr = end();
    else
      addr = bk - 1;
    return addr;
  }
  
  inline value_type* alloc_front(value_type* fr) {
    check(fr, bk);
    if (fr == beg())
      fr = end();
    else
      fr--;
    check(fr, bk);
    return fr;
  }
  
  inline value_type* alloc_back(value_type* bk) const {
    check(fr, bk);
    if (bk == end())
      bk = beg();
    else
      bk++;
    check(fr, bk);
    return bk;
  }
  
  inline value_type* dealloc_front(value_type* fr) const {
    check(fr, bk);
    if (fr == end())
      fr = beg();
    else
      fr++;
    check(fr, bk);
    return fr;
  }
  
  inline value_type* dealloc_back(value_type* bk) {
    check(fr, bk);
    if (bk == beg())
      bk = end();
    else
      bk--;
    check(fr, bk);
    return bk;
  }
  
  inline value_type* allocn_front(value_type* fr, int nb) const {
    assert(nb + size() <= capacity);
    check(fr, bk);
    value_type* new_fr = fr - nb;
    if (new_fr < beg())
      new_fr = end() - (beg() - new_fr - 1);
    check(fr, bk);
    return new_fr;
  }
  
  inline value_type* allocn_back(value_type* bk, int nb) const {
    assert(nb + size() <= capacity);
    check(fr, bk);
    value_type* new_bk = bk + nb;
    if (new_bk > end())
      new_bk = beg() + (new_bk - end() - 1);
    check(fr, bk);
    return new_bk;
  }
  
  inline value_type* deallocn_front(value_type* fr, int nb) const {
    assert(size() >= nb);
    check(fr, bk);
    value_type* new_fr = fr + nb;
    if (new_fr > end())
      new_fr = beg() + (new_fr - end() - 1);
    check(fr, bk);
    return new_fr;
  }
  
  inline value_type* deallocn_back(value_type* bk, int nb) const {
    assert(size() >= nb);
    check(fr, bk);
    value_type* new_bk = bk - nb;
    if (new_bk < beg())
      new_bk = end() - (beg() - new_bk - 1);
    check(fr, bk);
    return new_bk;
  }
  
public:
  
  typedef int size_type;
  typedef Item_alloc allocator_type;
  typedef segment<value_type*> segment_type;
  typedef ringbuffer_ptrx self_type;
  
  ringbuffer_ptrx() {
    init();
  }
  
#define DEBUG_RINGBUFFER 0
  
  ringbuffer_ptrx(const ringbuffer_ptrx& other) {
    init();
    int sz = other.size();
    int other_ix = other.array_index_of_front(other.fr);
#if DEBUG_RINGBUFFER==0
    int ix = array_index_of_front(fr);
    bk = allocn_back(bk, sz);
#else
    fr = beg() + other.array_index_of_pointer(other.fr);
    bk = beg() + other.array_index_of_pointer(other.bk);
    int ix = array_index_of_front(fr);
#endif
    copy_data_wrap_src_and_dst<allocator_type, nbcells>(other.beg(), other_ix, beg(), ix, sz);
    check(fr, bk);
  }
  
  ringbuffer_ptrx(size_type nb, const value_type& val) {
    init();
    pushn_back(const_foreach_body<Item_alloc>(val), nb);
    check(fr, bk);
  }
  
  ~ringbuffer_ptrx() {
    check(fr, bk);
    clear();
    check(fr, bk);
  }
  
  inline int size() const {
    if (bk > fr)
      return int(bk-fr)-1;
    else
      return int(bk-(fr-nbcells))-1;
  }
  
  inline bool full() const {
    return bk == fr;
  }
  
  inline bool empty() const {
    return (bk-fr == 1) || (bk-(fr-nbcells) == 1);
  }
  
  inline void push_front(const value_type& x) {
    assert(! full());
    alloc.construct(fr, x);
    fr = alloc_front(fr);
  }
  
  inline void push_back(const value_type& x) {
    assert(! full());
    alloc.construct(bk, x);
    bk = alloc_back(bk);
  }
  
  inline value_type& front() const {
    assert(! empty());
    return *addr_of_front(fr);
  }
  
  inline value_type& back() const {
    assert(! empty());
    return *addr_of_back(bk);
  }
  
  inline value_type pop_front() {
    assert(! empty());
    fr = dealloc_front(fr);
    value_type v = *fr;
    alloc.destroy(fr);
    return v;
  }
  
  inline value_type pop_back() {
    assert(! empty());
    bk = dealloc_back(bk);
    value_type v = *bk;
    alloc.destroy(bk);
    return v;
  }
  
  void frontn(value_type* dst, int nb) const {
    assert(size() >= nb);
    int ix_fr = array_index_of_front(fr);
    copy_data_wrap_src<Item_alloc, nbcells>(beg(), ix_fr, dst, 0, nb);
  }
  
  void backn(value_type* dst, int nb) const {
    assert(size() >= nb);
    value_type* new_bk = deallocn_back(bk, nb);
    int ix_bk = array_index_of_pointer(new_bk);
    copy_data_wrap_src<Item_alloc, nbcells>(beg(), ix_bk, dst, 0, nb);
  }
  
  void pushn_front(const value_type* xs, int nb) {
    assert(nb + size() <= capacity);
    fr = allocn_front(fr, nb);
    int ix_fr = array_index_of_front(fr);
    copy_data_wrap_dst<Item_alloc, nbcells>(xs, 0, beg(), ix_fr, nb);
  }
  
  void pushn_back(const value_type* xs, int nb) {
    assert(nb + size() <= capacity);
    int ix_bk = array_index_of_pointer(bk);
    copy_data_wrap_dst<Item_alloc, nbcells>(xs, 0, beg(), ix_bk, nb);
    bk = allocn_back(bk, nb);
  }
  
  void popn_front(int nb) {
    assert(size() >= nb);
    int ix_fr = array_index_of_front(fr);
    destroy_items_wrap_target<Item_alloc, nbcells>(beg(), ix_fr, nb);
    fr = deallocn_front(fr, nb);
  }
  
  template <class Body>
  void pushn_back(const Body& body, int nb) {
    assert(nb + size() <= capacity);
    int ix_bk = array_index_of_pointer(bk);
    papply_wrap_dst<Body, nbcells>(beg(), ix_bk, nb, body);
    bk = allocn_back(bk, nb);
  }
  
  void popn_back(int nb) {
    assert(size() >= nb);
    bk = deallocn_back(bk, nb);
    int ix_bk = array_index_of_pointer(bk);
    destroy_items_wrap_target<Item_alloc, nbcells>(beg(), ix_bk, nb);
  }
  
  void popn_front(value_type* dst, int nb) {
    frontn(dst, nb);
    popn_front(nb);
  }
  
  void popn_back(value_type* dst, int nb) {
    backn(dst, nb);
    popn_back(nb);
  }
  
  void transfer_from_back_to_front(ringbuffer_ptrx& target, int nb) {
    bk = deallocn_back(bk, nb);
    target.fr = target.allocn_front(target.fr, nb);
    int i1 = array_index_of_pointer(bk);
    int i2 = target.array_index_of_front(target.fr);
    copy_data_wrap_src_and_dst<Item_alloc, nbcells>(beg(), i1, target.beg(), i2, nb);
    check(fr, bk);
    target.check(target.fr, target.bk);
  }
  
  void transfer_from_front_to_back(ringbuffer_ptrx& target, int nb) {
    int i1 = array_index_of_front(fr);
    int i2 = target.array_index_of_pointer(target.bk);
    copy_data_wrap_src_and_dst<Item_alloc, nbcells>(beg(), i1, target.beg(), i2, nb);
    fr = deallocn_front(fr, nb);
    target.bk = target.allocn_back(target.bk, nb);
    check(fr, bk);
    target.check(target.fr, target.bk);
  }
  
  value_type& operator[](int ix) const {
    return *(segment_by_index(ix).middle);
  }
  
  value_type& operator[](size_t ix) const {
    return (*this)[int(ix)];
  }
  
  void clear() {
    popn_back(size());
  }
  
  void swap(ringbuffer_ptrx& other) {
    std::swap(fr, other.fr);
    std::swap(bk, other.bk);
    array.swap(other.array);
  }
  
  int array_index_of_logical_index(int ix) const {
    value_type* on_front_item = addr_of_front(fr);
    value_type* proj_addr_of_ix = on_front_item + ix;
    int array_index_of_ix =
    (proj_addr_of_ix <= end())
    ? int(proj_addr_of_ix - beg())
    : ix - int(end() - fr);
    return array_index_of_ix;
  }
  
  segment_type segment_by_index(int ix) const {
    assert(ix >= 0);
    assert(ix < size());
    value_type* p = &array[array_index_of_logical_index(ix)];
    value_type* on_fr = addr_of_front(fr);
    value_type* on_bk = addr_of_back(bk);
    return segment_of_ringbuffer(p, on_fr, on_bk, beg(), nbcells);
  }
  
  int index_of_pointer(const value_type* p) const {
    assert(p >= beg());
    assert(p <= end());
    value_type* on_front_item = addr_of_front(fr);
    int ix;
    if (p >= on_front_item) {
      ix = int(p - on_front_item);
    } else { // wraparound
      int ix1 = int(end() - fr);
      int ix2 = int(p - beg());
      ix = ix1 + ix2;
    }
    assert(ix >= 0);
    // assert(ix < size());
    return ix;
  }
  
  template <class Body>
  void for_each(const Body& body) const {
    /* SIMPLE VERSION
     size_t nb = size();
     value_type* p = fr + 1;
     while (p != bk) {
     if (p == &array[nbcells])
     p = beg();
     body(*p);
     p++;
     }
     */
    auto _body = apply_foreach_body<Item_alloc, Body>(body);
    int ix_fr = array_index_of_front(fr);
    int sz = size();
    papply_wrap_dst<typeof(_body), nbcells>(beg(), ix_fr, sz, 0, _body);
  }
  
  template <class Body>
  void for_each_segment(int lo, int hi, const Body& body) const {
    /*
     if (hi - lo <= 0 || hi < 1)
     return;
     segment_type seg1 = segment_by_index(int(lo));
     segment_type seg2 = segment_by_index(int(hi));
     if (seg1.begin == seg2.begin) {
     body(seg1.middle, seg2.middle + 1);
     } else {
     body(seg1.middle, seg1.end);
     body(seg2.begin, seg2.middle + 1);
     }
     */
    if (hi - lo <= 0 || hi < 1)
      return;
    segment_type seg1 = segment_by_index(int(lo));
    segment_type seg2 = segment_by_index(int(hi - 1));
    if (seg1.begin == seg2.begin) {
      body(seg1.middle, seg2.middle + 1);
    } else {
      body(seg1.middle, seg1.end);
      body(seg2.begin, seg2.middle + 1);
    }
  }
  
};

/*---------------------------------------------------------------------*/
/* Stack */

//! [fixedcapacitystack]
/*! 
 *  \class stack
 *  \brief Fixed-capacity contiguous stack
 *
 * Although supported, pushes and pops on the front of the
 * container take time linear in the size of the container.
 *
 * Container properties
 * ============
 *
 * \tparam Array_alloc Type of the allocator object used to define the
 * storage policy of the array that is used by the stack to store
 * the items of the stack.
 * \tparam Item_alloc Type of the allocator object used to define the
 * storage allocation model. By default, the allocator class template is 
 * used, which defines the simplest memory allocation model and is 
 * value-independent. Aliased as member type vector::allocator_type.
 */
template <class Array_alloc,
class Item_alloc = std::allocator<typename Array_alloc::value_type> >
class stack {
public:
  
  using size_type = int;
  using value_type = typename Array_alloc::value_type;
  using allocator_type = Item_alloc;
  using segment_type = segment<value_type*>;
  
  static constexpr int capacity = Array_alloc::capacity;
  
private:
  
  size_type bk;     // position of the last allocated cell
  Item_alloc alloc;
  Array_alloc array;
  
  static value_type& subscript(value_type* array, size_type bk, size_type ix) {
    assert(array != NULL);
    assert(ix >= 0);
    assert(ix <= bk);
    return array[ix];
  }
  
public:
  
  stack()
  : bk(-1) { }
  
  stack(const stack& other)
  : bk(-1) {
    if (other.size() == 0)
      return;
    const value_type* ptr = &other[0];
    pushn_back(ptr, other.size());
  }
  
  stack(size_type nb, const value_type& val) {
    pushn_back(const_foreach_body<Item_alloc>(val), nb);
  }
  
  ~stack() {
    clear();
  }
  
  inline size_type size() const {
    return bk + 1;
  }
  
  inline bool full() const {
    return size() == capacity;
  }
  
  inline bool empty() const {
    return size() == 0;
  }
  
  inline bool partial() const {
    return !empty() && !full(); // TODO: could be optimized
  }
  
  inline void push_front(const value_type& x) {
    assert(! full());
    pshiftn<Item_alloc>(&array[0], size(), +1);
    alloc.construct(&array[0], x);
    bk++;
  }
  
  inline void push_back(const value_type& x) {
    assert(! full());
    bk++;
    alloc.construct(&array[bk], x);
  }
  
  inline value_type& front() const {
    assert(! empty());
    return array[0];
  }
  
  inline value_type& back() const {
    assert(! empty());
    return array[bk];
  }
  
  inline value_type pop_front() {
    assert(! empty());
    value_type v = front();
    alloc.destroy(&(front()));
    pshiftn<Item_alloc>(&array.operator[](1), size() - 1, -1);
    bk--;
    return v;
  }
  
  inline value_type pop_back() {
    assert(! empty());
    value_type v = back();
    alloc.destroy(&(back()));
    bk--;
    return v;
  }
  
  void frontn(value_type* dst, size_type nb) {
    assert(size() >= nb);
    copy<Item_alloc>(dst, &array[0], nb);
  }
  
  void backn(value_type* dst, size_type nb) {
    assert(size() >= nb);
    copy<Item_alloc>(dst, &array.operator[](size() - nb), nb);
  }
  
  void pushn_front(const value_type* xs, size_type nb) {
    assert(nb + size() <= capacity);
    pshiftn<Item_alloc>(&array[0], size(), +nb);
    copy<Item_alloc>(&array[0], xs, nb);
    bk += nb;
  }
  
  void pushn_back(const value_type* xs, size_type nb) {
    assert(nb + size() <= capacity);
    copy<Item_alloc>(&array.operator[](size()), xs, nb);
    bk += nb;
  }
  
  template <class Body>
  void pushn_back(const Body& body, size_type nb) {
    assert(nb + size() <= capacity);
    papply<Body>(&array[bk+1], nb, 0, body);
    bk += nb;
  }
  
  void popn_front(size_type nb) {
    assert(size() >= nb);
    destroy_items<Item_alloc>(&array[0], nb);
    pshiftn<Item_alloc>(&array.operator[](nb), size() - nb, -nb);
    bk -= nb;
  }
  
  void popn_back(size_type nb) {
    assert(size() >= nb);
    destroy_items<Item_alloc>(&array[0], size() - nb, nb);
    bk -= nb;
  }
  
  void popn_front(value_type* dst, size_type nb) {
    frontn(dst, nb);
    popn_front(nb);
  }
  
  void popn_back(value_type* dst, size_type nb) {
    backn(dst, nb);
    popn_back(nb);
  }
  
  void transfer_from_back_to_front(stack& target, size_type nb) {
    assert(nb <= size());
    assert(target.size() + nb <= capacity);
    pshiftn<Item_alloc>(&target.array[0], target.size(), +nb);
    popn_back(&target.array[0], nb);
    target.bk += nb;
  }
  
  void transfer_from_front_to_back(stack& target, size_type nb) {
    assert(nb <= size());
    assert(target.size() + nb <= capacity);
    popn_front(&target.array.operator[](target.size()), nb);
    target.bk += nb;
  }
  
  value_type& operator[](size_type ix) const {
    return subscript(&array[0], bk, ix);
  }
  
  value_type& operator[](size_t ix) const {
    return subscript(&array[0], bk, (size_type)ix);
  }
  
  void clear() {
    popn_back(size());
  }
  
  void swap(stack& other) {
    array.swap(other.array);
    std::swap(bk, other.bk);
  }
  
  size_type index_of_last_item() const {
    size_type sz = size();
    return (sz > 0) ? sz - 1 : sz;
  }
  
  segment_type segment_by_index(size_type ix) const {
    segment_type seg;
    seg.begin = &array[0];
    seg.middle = &array[ix];
    seg.end = &array[index_of_last_item()] + 1;
    return seg;
  }
  
  size_type index_of_pointer(const value_type* p) const {
    assert(p >= &array[0]);
    assert(p <= &array[index_of_last_item()] + 1);
    return (size_type)(p - &array[0]);
  }
  
  template <class Body>
  void for_each(const Body& body) const {
    for (size_type i = 0; i < size(); i++)
      body((*this)[i]);
  }
  
  template <class Body>
  void for_each_segment(size_type lo, size_type hi, const Body& body) const {
    body(&array[size_type(lo)], &array[size_type(hi)]);
  }
  
};
//! [fixedcapacitystack]

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_FIXEDCAPACITYBASE_H_ */
