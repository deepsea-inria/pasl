/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file parray.hpp
 * \brief Array-based implementation of sequences
 *
 */

#include <cmath>

#include "prim.hpp"

#ifndef _PARRAY_PCTL_PARRAY_BASE_H_
#define _PARRAY_PCTL_PARRAY_BASE_H_

namespace pasl {
namespace pctl {
namespace parray {

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Parallel array */

template <class Item, class Alloc = std::allocator<Item>>
class parray {
public:
  
  using value_type = Item;
  using reference = value_type&;
  using const_reference = const value_type&;
  using allocator_type = Alloc;
  
private:
  
  class Deleter {
  public:
    void operator()(value_type* ptr) {
      free(ptr);
    }
  };
  
  std::unique_ptr<value_type[], Deleter> ptr;
  long sz = -1l;
  
  void alloc() {
    assert(sz >= 0);
    value_type* p = prim::alloc_array<value_type>(sz);
    assert(p != nullptr);
    ptr.reset(p);
  }
  
  void destroy() {
    if (sz < 1)
      return;
    prim::pdelete<Item, Alloc>(&operator[](0), &operator[](sz-1)+1);
    sz = 0;
  }
  
  void fill(long n, const value_type& val) {
    destroy();
    sz = n;
    alloc();
    if (sz < 1)
      return;
    prim::fill(&operator[](0), &operator[](sz-1)+1, val);
  }
  
  void check(long i) const {
    assert(ptr != nullptr);
    assert(i >= 0);
    assert(i < sz);
  }
  
public:
  
  parray(long sz = 0)
  : sz(sz) {
    resize(sz);
  }
  
  parray(long sz, const value_type& val)
  : sz(sz) {
    resize(sz, val);
  }
  
  parray(std::initializer_list<value_type> xs)
  : sz(xs.size()) {
    alloc();
    long i = 0;
    for (auto it = xs.begin(); it != xs.end(); it++)
      ptr[i++] = *it;
  }
  
  ~parray() {
    destroy();
  }
  
  parray(const parray& other) {
    sz = other.size();
    alloc();
    prim::copy(&other[0], &other[sz-1]+1, &ptr[0]);
  }
  
  parray& operator=(parray&& other) {
    ptr = std::move(other.ptr);
    sz = std::move(other.sz);
    other.sz = 0l; // redundant?
    return *this;
  }
  
  value_type& operator[](long i) {
    check(i);
    return ptr[i];
  }
  
  const value_type& operator[](long i) const {
    check(i);
    return ptr[i];
  }
  
  long size() const {
    return sz;
  }
  
  void swap(parray& other) {
    ptr.swap(other.ptr);
    std::swap(sz, other.sz);
  }
  
  void resize(long n, const value_type& val) {
    fill(n, val);
  }
  
  void resize(long n) {
    value_type val;
    resize(n, val);
  }
  
};

/*---------------------------------------------------------------------*/
/* Slice */

template <class PArray_pointer>
class slice {
public:
  
  PArray_pointer pointer;
  long lo;
  long hi;
  
  slice()
  : pointer(nullptr), lo(0), hi(0) { }
  
  slice(PArray_pointer _pointer)
  : pointer(_pointer), lo(0), hi(_pointer->size()) { }
  
  slice(long _lo, long _hi, PArray_pointer _pointer=nullptr) {
    if (_pointer != nullptr)
      pointer = _pointer;
    assert(lo >= 0);
    assert(hi <= pointer->size());
    lo = _lo;
    hi = _hi;
  }
  
};

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif /*! _PARRAY_PCTL_PARRAY_BASE_H_ */
