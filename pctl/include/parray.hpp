             /* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file parray.hpp
 * \brief Array-based implementation of sequences
 *
 */

#include <cmath>

#include "pmem.hpp"

#ifndef _PCTL_PARRAY_BASE_H_
#define _PCTL_PARRAY_BASE_H_

namespace pasl {
namespace pctl {

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Parallel array */

template <class Item, class Alloc = std::allocator<Item>>
class parray {
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
  
private:
  
  class Deleter {
  public:
    void operator()(value_type* ptr) {
      free(ptr);
    }
  };
  
  std::unique_ptr<value_type[], Deleter> ptr;
  long sz = 0L;
  
  void alloc(long n) {
    sz = n;
    assert(sz >= 0);
    value_type* p = (value_type*)malloc(sz * sizeof(value_type));
    assert(p != nullptr);
    ptr.reset(p);
  }
  
  void destroy() {
    pmem::pdelete<Item, Alloc>(begin(), end());
    sz = 0;
  }
  
  void fill(long n, const value_type& val) {
    destroy();
    alloc(n);
    pmem::fill(begin(), end(), val);
  }
  
  void check(long i) const {
    assert(ptr != nullptr);
    assert(i >= 0);
    assert(i < sz);
  }
  
public:
  
  parray(long sz = 0) {
    value_type val;
    fill(sz, val);
  }
  
  parray(long sz, const value_type& val) {
    fill(sz, val);
  }
  
  parray(long sz, const std::function<value_type(long)>& body)
  : sz(0) {
    rebuild(sz, body);
  }
  
  parray(long sz,
         const std::function<long(long)>& body_comp,
         const std::function<value_type(long)>& body)
  : sz(0) {
    assert(false);
  }
  
  parray(long sz,
         const std::function<long(long,long)>& body_comp_rng,
         const std::function<value_type(long)>& body)
  : sz(0) {
    assert(false);
  }
  
  parray(std::initializer_list<value_type> xs) {
    alloc(xs.size());
    long i = 0;
    for (auto it = xs.begin(); it != xs.end(); it++) {
      new (&ptr[i++]) value_type(*it);
    }
  }
  
  ~parray() {
    destroy();
  }
  
  parray(const parray& other) {
    alloc(other.size());
    pmem::copy(other.cbegin(), other.cend(), begin());
  }
  
  parray(iterator lo, iterator hi) {
    long n = hi - lo;
    if (n < 0) {
      return;
    }
    alloc(n);
    pmem::copy(lo, hi, begin());
  }
  
  parray& operator=(const parray& other) {
    if (&other == this) {
      return *this;
    }
    alloc(other.size());
    pmem::copy(other.cbegin(), other.cend(), begin());
    return *this;
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
    if (n == sz) {
      return;
    }
    parray<Item> tmp(n, val);
    swap(tmp);
    long m = std::min(tmp.size(), size());
    assert(size() >= m);
    pmem::copy(tmp.cbegin(), tmp.cbegin()+m, begin());
  }
  
  void resize(long n) {
    value_type val;
    resize(n, val);
  }
  
  void clear() {
    resize(0);
  }
  
  template <class Body>
  void rebuild(long n, const Body& body) {
    clear();
    alloc(n);
    parallel_for(0l, n, [&] (long i) {
      ptr[i] = body(i);
    });
  }
  
  template <class Body, class Body_comp>
  void rebuild(long n, const Body_comp& body_comp, const Body& body) {
    clear();
    alloc(n);
    parallel_for(0l, n, body_comp, [&] (long i) {
      ptr[i] = body(i);
    });
  }
  
  iterator begin() const {
    return &ptr[0];
  }
  
  const_iterator cbegin() const {
    return &ptr[0];
  }
  
  iterator end() const {
    return &ptr[size()];
  }
  
  const_iterator cend() const {
    return &ptr[size()];
  }
  
};

/***********************************************************************/

} // end namespace
} // end namespace

#endif /*! _PCTL_PARRAY_BASE_H_ */
