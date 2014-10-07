/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file container.hpp
 * \brief Several container data structures
 *
 */

#ifndef _PASL_DATA_CONTAINER_H_
#define _PASL_DATA_CONTAINER_H_

#include <vector>
#include <deque>
#ifdef HAVE_ROPE
#include <ext/rope>
#include <ext/pool_allocator.h> 
#endif
#ifdef HAVE_CORD
extern "C"
{
#include "gc.h"	/* For GC_INIT() only */ /* nodepend */
#include <cord.h>
}
#endif

#include "atomic.hpp"

/***********************************************************************/

namespace pasl {
namespace data {
  
/*---------------------------------------------------------------------*/
/* Stylized malloc/free */

template <class Value>
Value* mynew() {
  Value* res = (Value*)malloc(sizeof(Value));
  if (res == NULL)
    util::atomic::die("mynew returned NULL");
  return res;
}

template <class Value>
Value* mynew_array(size_t nb) {
  Value* res = (Value*)malloc(size_t(sizeof(Value)) * nb);
  if (res == NULL)
    util::atomic::die("mynew_array returned NULL");
  return res;
}

static inline void myfree(void* p) {
  free(p);
}
  
/*---------------------------------------------------------------------*/
/* Contiguous array interface; destructor does not deallocate 
 * underlying array
 */
  
template <class Item>
class pointer_seq {
public:
  
  typedef size_t size_type;
  typedef Item value_type;
  typedef pointer_seq<Item> self_type;
  
  value_type* array;
  size_type sz;
  
  pointer_seq()
  : array(NULL), sz(0) { }
  
  pointer_seq(value_type* array, size_type sz)
  : array(array), sz(sz) { }
  
  ~pointer_seq() {
    clear();
  }
  
  void clear() {
    sz = 0;
    array = NULL;
  }
  
  value_type& operator[](size_type ix) const {
    assert(ix >= 0);
    assert(ix < sz);
    return array[ix];
  }
  
  size_type size() const {
    return sz;
  }
  
  void swap(self_type& other) {
    std::swap(sz, other.sz);
    std::swap(array, other.array);
  }
  
  void alloc(size_type n) {
    util::atomic::die("operation not supported");
  }
  
  void set_data(value_type* data) {
    this->data = data;
  }
  
  value_type* data() const {
    return array;
  }
  
  template <class Body>
  void for_each(const Body& b) const {
    for (size_type i = 0; i < sz; i++)
      b(array[i]);
  }
  
};
  
/*---------------------------------------------------------------------*/
/* Contiguous array; destructor deallocates underlying array */

template <class Item>
class array_seq {
public:
  
  typedef array_seq<Item> self_type;
  typedef Item value_type;
  typedef size_t size_type;
  
private:
  
  size_type sz;
  value_type* array;
  
public:
  
  array_seq()
  : sz(0), array(NULL) { }
  
  ~array_seq() {
    clear();
  }
  
  void clear() {
    if (array != NULL)
      myfree(array);
    array = NULL;
  }
  
  value_type& operator[](size_type ix) const {
    assert(ix >= 0);
    assert(ix < sz);
    return array[ix];
  }
  
  size_type size() const {
    return sz;
  }
  
  void swap(self_type& other) {
    std::swap(sz, other.sz);
    std::swap(array, other.array);
  }
  
  void alloc(size_type n) {
    value_type* old_array = array;
    sz = n;
    array = mynew_array<value_type>(sz);
    if (old_array != NULL) {
      // TODO: use memcpy to copy old to new array
      util::atomic::die("todo");
      myfree(old_array);
    }
  }
  
  void set_data(value_type* data) {
    this->data = data;
  }
  
  value_type* data() const {
    return array;
  }
  
};
  
namespace stl {
  
/*---------------------------------------------------------------------*/
/* Wrapper for STL vector */

template <class Item>
class vector_seq {
public:
  
  typedef vector_seq<Item> self_type;
  typedef Item value_type;
  typedef size_t size_type;
  typedef vector_seq<Item> alias_type;
  
  mutable std::vector<value_type> vec;
  
  size_type size() const {
    return vec.size();
  }
  
  bool empty() const {
    return vec.empty();
  }
  
  
  Item& back() {
    return vec.back();
  }
  
  Item& front() {
    util::atomic::die("unsupported operation");
    return vec.back();
  }
  
  Item pop_back() {
    Item x = vec.back();
    vec.pop_back();
    return x;
  }
  
  Item pop_front() {
    util::atomic::die("unsupported operation");
    Item x = vec.back();
    vec.pop_back();
    return x;
  }
  
  void push_back(const Item& x) {
    vec.push_back(x);
  }
  
  void push_front(const Item& x) {
    util::atomic::die("unsupported operation");
  }
  
  void pushn_back(value_type* vs, size_type nb) {
    size_t sz = size();
    vec.resize(sz + nb);
    for (size_t i = 0; i < nb; i++)
      vec[i + sz] = vs[i];
  }
  
  void pushn_back(const value_type v, size_type nb) {
    for (size_t i = 0; i < nb; i++)
      push_back(v);
  }
  
  void popn_back(value_type* vs, size_type nb) {
    size_t sz = size();
    size_t st = sz - nb;
    for (size_t i = 0; i < nb; i++)
      vs[i] = vec[st + i];
    vec.resize(sz - nb);
  }
  
  value_type& operator[](size_type ix) const {
    return vec[ix];
  }
  
  value_type& operator[](int ix) const {
    return vec[ix];
  }
  
  template <class Loop_body>
  void for_each(const Loop_body& body) {
    for (size_t i = 0; i < size(); i++)
      body(vec[i]);
  }
  
  typedef typename std::vector<Item>::iterator iterator;
  iterator begin()  {
    return vec.begin();
  }
  
  iterator end()  {
    return vec.end();
  }
  
  iterator insert(iterator pos, const Item& val) {
    return vec.insert(pos, val);
  }
  
  void transfer_to_back(self_type& dst) {
    int nb = (int)dst.size();
    for (int i = 0; i < nb; i++)
      push_back(dst.vec[i]);
    dst.clear();
  }
  
  void transfer_from_back_to_front_by_position(self_type& dst, iterator it) {
    for (auto it2 = it; it2 != end(); it2++)
      dst.push_back(*it2);
    vec.erase(it, end());
  }
  
  void split_approximate(self_type& dst) {
    size_t mid = size() / 2;
    transfer_from_back_to_front_by_position(dst, begin() + mid);
  }
  
  value_type* data() const {
    return vec.data();
  }
  
  void swap(self_type& other) {
    vec.swap(other.vec);
  }
  
  void alloc(size_type n) {
    vec.resize(n);
  }
  
  void clear() {
    vec.clear();
  }
  
};

/*---------------------------------------------------------------------*/
/* Wrapper for STL deque */

template <class Item>
class deque_seq {
public:
  
  typedef deque_seq<Item> self_type;
  typedef Item value_type;
  typedef size_t size_type;
  
  mutable std::deque<value_type> deque;
  
  size_type size() const {
    return deque.size();
  }
  
  bool empty() const {
    return deque.empty();
  }
  
  Item& back() {
    return deque.back();
  }
  
  Item& front() {
    return deque.front();
  }
  
  Item pop_back() {
    Item x = deque.back();
    deque.pop_back();
    return x;
  }
  
  Item pop_front() {
    Item x = deque.front();
    deque.pop_front();
    return x;
  }
  
  void push_back(const Item& x) {
    deque.push_back(x);
  }
  
  void push_front(const Item& x) {
    deque.push_front(x);
  }
  
  void backn(value_type* dst, size_type nb) const {
    size_t sz = size();
    assert(nb <= sz);
    size_t off = sz - nb;
    for (size_t i = 0; i < nb; i++)
      dst[i] = deque[i+off];
  }
  
  void frontn(value_type* dst, size_type nb) const {
    size_t sz = size();
    assert(nb <= sz);
    for (size_t i = 0; i < nb; i++)
      dst[i] = deque[i];
  }
  
  void pushn_back(const value_type v, size_type nb) {
    for (size_t i = 0; i < nb; i++)
      push_back(v);
  }
  
  void pushn_back(value_type* vs, size_type nb) {
    size_t sz = size();
    deque.resize(sz + nb);
    for (size_t i = 0; i < nb; i++)
      deque[i + sz] = vs[i];
  }
  
  void pushn_front(value_type* vs, size_type nb) {
    vs += nb;
    for (size_t i = 0; i < nb; i++) {
      vs--;
      push_front(*vs);
    }
  }
  
  void popn_back(value_type* vs, size_type nb) {
    size_t sz = size();
    assert(nb <= sz);
    size_t st = sz - nb;
    for (size_t i = 0; i < nb; i++)
      vs[i] = deque[i + st];
    deque.resize(sz - nb);
  }
  
  void popn_front(value_type* vs, size_type nb) {
    assert(nb <= size());
    for (size_t i = 0; i < nb; i++)
      vs[i] = deque[i];
    for (size_t i = 0; i < nb; i++)
      pop_front();
  }
  
  void clear() {
    deque.clear();
  }
  
  value_type& operator[](size_type ix) const {
    assert(ix >= 0);
    assert(ix < size());
    return deque[ix];
  }
  
  value_type& operator[](int ix) const {
    assert(ix >= 0);
    assert(ix < size());
    return deque[ix];
  }
  
  template <class Loop_body>
  void for_each(const Loop_body& body) const {
    for (auto it = deque.begin(); it != deque.end(); it++)
      body(*it);
  }
  
  typedef typename std::deque<Item>::iterator iterator;
  iterator begin() const  {
    return deque.begin();
  }
  
  iterator end() const {
    return deque.end();
  }
  
  iterator insert(iterator pos, const Item& val) {
    return deque.insert(pos, val);
  }
  
  iterator erase(iterator first, iterator last) {
    return deque.erase(first, last);
  }
  
  void split(size_type n, self_type& dst) {
    auto it = begin() + n;
    for (auto it2 = it; it2 != end(); it2++)
      dst.push_back(*it2);
    deque.erase(it, end());
  }
  
  void split_approximate(self_type& other) {
    split(size() / 2, other);
  }
  
  void concat(self_type& dst) {
    int nb = (int)dst.size();
    for (int i = 0; i < nb; i++)
      push_back(dst.deque[i]);
    dst.clear();
  }
  
  void swap(self_type& other) {
    deque.swap(other.deque);
  }
  
  void alloc(size_type n) {
    deque.resize(n);
  }
  
  value_type* data() const {
    util::atomic::die("not supported");
    return NULL;
  }
  
  bool operator==(const self_type& other) const {
    return deque == other.deque;
  }
  
};
  
/*---------------------------------------------------------------------*/
/* Wrapper for STL rope */
  
#ifdef HAVE_ROPE
template<class Item>
class rope_seq {
public:
  typedef __gnu_cxx::rope<Item, __gnu_cxx::__pool_alloc<Item> > container_t;
  typedef Item value_type;
  typedef typename container_t::size_type size_type;
  mutable container_t v;
  using self_type = rope_seq<Item>;
  
  rope_seq() {}
  
  rope_seq(size_t nb, value_type val) {
    v.append(nb, val);
  }
  
  size_t size() const {
    return v.size();
  }
  
  bool empty() const {
    return v.empty();
  }
  
  void push_back(Item i) {
    v.push_back(i);
  }
  
  Item pop_back() {
    Item i = v.back();
    v.pop_back();
    return i;
  }
  
  void push_front(Item i) {
    v.push_front(i);
  }
  
  Item pop_front() {
    Item i = v.front();
    v.pop_front();
    return i;
  }
  
  typedef typename container_t::iterator iterator;
  iterator begin() const {
    return v.mutable_begin();
  }
  
  iterator end() const {
    return v.mutable_end();
  }
  
  iterator insert(iterator pos, const Item& val) {
    return v.insert(pos, val);
  }
  
  void clear() {
    v.clear();
  }
  
  void pushn_back(value_type* vs, size_t nb) {
    for (size_t i = 0; i < nb; i++)
      push_back(vs[i]);
  }
  
  void pushn_back(const value_type v, size_t nb) {
    for (size_t i = 0; i < nb; i++)
      push_back(v);
  }
  
  void popn_back(value_type* vs, size_t nb) {
    for (size_t i = 0; i < nb; i++)
      pop_back();
  }
  
  value_type operator[](int ix) const {
    return v[ix];
  }
  
  value_type operator[](size_t ix) const {
    return v[ix];
  }
  
  template <class Loop_body>
  void for_each(const Loop_body& body) {
    for (size_t i = 0; i < size(); i++)
      body(i, v[i]);
  }
  
  void concat(rope_seq& r) {
    v.append(r.v);
    r.v.erase(r.v.mutable_begin(), r.v.mutable_end());
  }

  void split(size_type n, self_type& dst) {
    auto it = begin() + n;
    auto r = v.substr(it, v.mutable_end());
    v.erase(it, v.mutable_end());
    dst.v.append(r);
  }
  
  void split_approximate(rope_seq& dst) {
    assert(size() >= 2);
    size_t mid = size() / 2;
    split(begin() + mid, dst);
  }
  
  void swap(rope_seq& other) {
    v.swap(other.v);
  }
  
};
#endif
  
} // end namespace stl
  
/*---------------------------------------------------------------------*/
/* Wrapper for the cord data structure 
 * 
 * This data structure is included in the Boehm–Demers–Weiser garbage 
 * collector.
 */
  
//! \warning Safe use of cord requires a call to `GC_INIT()` prior to use
  
#ifdef HAVE_CORD
template<class Item>
class cord_wrapper {
public:
  typedef CORD container_t;
  typedef Item value_type;
  typedef size_t size_type;
  
  mutable container_t v;
  
  static container_t chars(size_t nb, value_type val) {
    return CORD_chars(val.get_char(), nb);
  }
  
  value_type fetch(size_t pos) const {
    assert(pos < size());
    return value_type(CORD_fetch(v, (int)pos));
  }
  
  cord_wrapper() {
    v = CORD_EMPTY;
  }
  
  cord_wrapper(size_t nb, value_type val) {
    v = chars(nb, val);
  }
  
  bool empty() const {
    return v == CORD_EMPTY;
  }
  
  void push_back(Item i) {
    v = CORD_cat(v, chars(1, i));
  }
  
  Item pop_back() {
    Item i = fetch(size() - 1);
    v = CORD_substr(v, 0, size() - 1);
    return i;
  }
  
  void push_front(Item i) {
    v = CORD_cat(chars(1, i), v);
  }
  
  Item pop_front() {
    Item i = fetch(0);
    v = CORD_substr(v, 1, size() - 1);
    return i;
  }
  
  size_t size() const {
    return CORD_len(v);
  }
  
  typedef int iterator;
  
  iterator begin() const {
    return 0;
  }
  
  iterator end() const {
    return size() + 1;
  }
  
  iterator insert(iterator pos, const Item& val) {
    cord_wrapper dst;
    transfer_from_back_to_front_by_position(dst, pos);
    dst.push_front(val);
    transfer_to_back(dst);
  }
  
  void clear() {
    v = CORD_EMPTY;
  }
  
  void pushn_back(value_type* vs, size_t nb) {
    for (size_t i = 0; i < nb; i++)
      push_back(vs[i]);
  }
  
  void pushn_back(const value_type v, size_t nb) {
    for (size_t i = 0; i < nb; i++)
      push_back(v);
  }
  
  void popn_back(value_type* vs, size_t nb) {
    for (size_t i = 0; i < nb; i++)
      pop_back();
  }
  
  value_type operator[](int ix) const {
    return fetch((size_t)ix);
  }
  
  value_type operator[](size_t ix) const {
    return fetch(ix);
  }
  
  template <class Loop_body>
  void for_each(const Loop_body& body) {
    for (size_t i = 0; i < size(); i++)
      body(i, v[i]);
  }
  
  void transfer_to_back(cord_wrapper& r) {
    v = CORD_cat(v, r.v);
    r.v = CORD_EMPTY;
  }
  
  void transfer_from_back_to_front_by_position(cord_wrapper& dst, iterator it) {
    dst.v = CORD_substr(v, it, size());
    v = CORD_substr(v, 0, it);
  }
  
  void swap(cord_wrapper& other) {
    std::swap(v, other.v);
  }
  
};
#endif
  
} // end namespace data
} // end namespace pasl


/***********************************************************************/

#endif /*! _PASL_DATA_CONTAINER_H_ */
