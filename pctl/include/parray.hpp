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
  
  void realloc(long n) {
    destroy();
    alloc(n);
  }
  
  void fill(long n, const value_type& val) {
    realloc(n);
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
    tabulate(sz, body);
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
  
  ~parray() {
    destroy();
  }
  
  parray& operator=(const parray& other) {
    if (&other == this) {
      return *this;
    }
    realloc(other.size());
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
  void tabulate(long n, const Body& body) {
    realloc(n);
    parallel_for(0l, n, [&] (long i) {
      ptr[i] = body(i);
    });
  }
  
  template <class Body, class Body_comp_rng>
  void tabulate(long n, const Body_comp_rng& body_comp_rng, const Body& body) {
    realloc(n);
    parallel_for(0l, n, body_comp_rng, [&] (long i) {
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
  
/*---------------------------------------------------------------------*/
/* Weighted parallel array */
  
namespace weighted {
  
template <class Item>
class unary {
public:

  void resize(const Item*, const Item*) {
    
  }
  
  const long* begin() const {
    return nullptr;
  }
  
  const long* end() const {
    return nullptr;
  }
  
  void swap(unary&) {
    
  }
  
};
  
template <class Pointer>
class vector_const_iterator {
public:
  
  using iterator_category = std::random_access_iterator_tag;
  using value_type = value_type_of<Pointer>;
  using difference_type = long;
  using pointer = Pointer;
  using reference = reference_of<Pointer>;
  
protected:
  
  Pointer item_pointer;
  const long* weight_pointer;
  
public:
  
  Pointer get_ptr() const    {  return   item_pointer;  }
  
  explicit vector_const_iterator(Pointer ptr, const long* weight_pointer)
  : item_pointer(ptr), weight_pointer(weight_pointer) {}
   
public:
  
  vector_const_iterator()
  : item_pointer(nullptr), weight_pointer(nullptr) {
  }
  
  reference operator*()   const {
    return *item_pointer;
  }
  
  const value_type* operator->()  const {
    return item_pointer;
  }
  
  reference operator[](difference_type off) const {
    return item_pointer[off];
  }
  
  const long* get_weight_ptr() const {
    return weight_pointer;
  }
  
  vector_const_iterator& operator++() {
    ++item_pointer;
    ++weight_pointer;
    return *this;
  }
  
  vector_const_iterator operator++(int) {
    Pointer tmp = item_pointer;
    const long* tmp_weight = weight_pointer;
    ++*this;
    return vector_const_iterator(tmp, tmp_weight);
  }
  
  vector_const_iterator& operator--() {
    --item_pointer;
    --weight_pointer;
    return *this;
  }
  
  vector_const_iterator operator--(int) {
    Pointer tmp = item_pointer;
    const long* tmp_weight = weight_pointer;
    --*this;
    return vector_const_iterator(tmp, tmp_weight);
  }
  
  vector_const_iterator& operator+=(difference_type off) {
    item_pointer += off;
    weight_pointer += off;
    return *this;
  }
  
  vector_const_iterator operator+(difference_type off) const {
    return vector_const_iterator(item_pointer+off, weight_pointer+off);
  }
  
  friend vector_const_iterator operator+(difference_type off, const vector_const_iterator& right) {
    return vector_const_iterator(off + right.item_pointer, off + right.weight_pointer);
  }
  
  vector_const_iterator& operator-=(difference_type off) {
    item_pointer -= off;
    weight_pointer -= off;
    return *this;
  }
  
  vector_const_iterator operator-(difference_type off) const {
    return vector_const_iterator(item_pointer-off, weight_pointer-off);
  }
  
  difference_type operator-(const vector_const_iterator& right) const {
    return item_pointer - right.item_pointer;
  }
  
  bool operator==   (const vector_const_iterator& r)  const {
    return item_pointer == r.item_pointer;
  }
  
  bool operator!=   (const vector_const_iterator& r)  const {
    return item_pointer != r.item_pointer;
  }
  
  bool operator<    (const vector_const_iterator& r)  const {
    return item_pointer < r.item_pointer;
  }
  
  bool operator<=   (const vector_const_iterator& r)  const {
    return item_pointer <= r.item_pointer;
  }
  
  bool operator>    (const vector_const_iterator& r)  const {
    return item_pointer > r.item_pointer;
  }
  
  bool operator>=   (const vector_const_iterator& r)  const {
    return item_pointer >= r.item_pointer;
  }
};
  
template <class Pointer>
class vector_iterator :  public vector_const_iterator<Pointer> {
public:
  
  explicit vector_iterator(Pointer ptr, const long* weight_pointer)
  : vector_const_iterator<Pointer>(ptr, weight_pointer)
  {}
  
public:
  
  using iterator_category = std::random_access_iterator_tag;
  using value_type = value_type_of<Pointer>;
  using difference_type = long;
  using pointer = Pointer;
  using reference = reference_of<Pointer>;
  
  vector_iterator() {}
  
  reference operator*()  const {
    return *this->item_pointer;
  }
  
  value_type* operator->() const {
    return this->item_pointer;
  }
  
  reference operator[](difference_type off) const {
    return this->item_pointer[off];
  }
  
  long weight_of(difference_type off = 0) const {
    return this->weight_pointer[off];
  }
  
  vector_iterator& operator++() {
    ++this->item_pointer;
    ++this->weight_pointer;
    return *this;
  }
  
  vector_iterator operator++(int) {
    pointer tmp = this->item_pointer;
    const long* tmp_weight = this->weight_pointer;
    ++*this;
    return vector_iterator(tmp, tmp_weight);
  }
  
  vector_iterator& operator--() {
    --this->item_pointer;
    return *this;
  }
  
  vector_iterator operator--(int) {
    vector_iterator tmp = *this;
    --*this;
    return vector_iterator(tmp);
  }
  
  vector_iterator& operator+=(difference_type off) {
    this->item_pointer += off;
    this->weight_pointer += off;
    return *this;
  }
  
  vector_iterator operator+(difference_type off) const {
    return vector_iterator(this->item_pointer+off, this->weight_pointer+off);
  }
  
  friend vector_iterator operator+(difference_type off, const vector_iterator& right) {
    return vector_iterator(off + right.item_pointer, off + right.weight_pointer);
  }
  
  vector_iterator& operator-=(difference_type off) {
    this->item_pointer -= off;
    this->weight_pointer -= off;
    return *this;
  }
  
  vector_iterator operator-(difference_type off) const {
    return vector_iterator(this->item_pointer-off, this->weight_pointer-off);
  }
  
  difference_type operator-(const vector_const_iterator<Pointer>& right) const {
    return static_cast<const vector_const_iterator<Pointer>&>(*this) - right;
  }
  
};
  
template <class Iterator>
long weight_of(Iterator lo, Iterator hi) {
  const long* weight_lo = lo.get_weight_ptr();
  const long* weight_hi = hi.get_weight_ptr();
  if (weight_lo == nullptr || weight_hi == nullptr) {
    return hi.get_ptr() - lo.get_ptr();
  } else {
    return *(weight_hi-1) - *weight_lo;
  }
}

template <
  class Item,
  class Weight = unary<Item>,
  class Alloc = std::allocator<Item>
>
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
  using iterator = vector_iterator<pointer>;
  using const_iterator = vector_const_iterator<const_pointer>;
  
private:
  
  Weight weight;
  pasl::pctl::parray<Item> items;
  
  void compute_weight() {
    weight.resize(cbegin().get_ptr(), cend().get_ptr());
  }
  
public:
  
  parray(const Weight& weight, long sz = 0)
  : weight(weight), items(sz) {
    compute_weight();
  }
  
  parray(const Weight& weight, long sz, const value_type& val)
  : weight(weight), items(val) {
    compute_weight();
  }
  
  parray(const Weight& weight,
         long sz, const std::function<value_type(long)>& body)
  : weight(weight), items(sz, body) {
    compute_weight();
  }
  
  parray(const Weight& weight,
         long sz,
         const std::function<long(long)>& body_comp,
         const std::function<value_type(long)>& body)
  : weight(weight), items(sz, body_comp, body) {
    assert(false);
    compute_weight();
  }
  
  parray(const Weight& weight,
         long sz,
         const std::function<long(long,long)>& body_comp_rng,
         const std::function<value_type(long)>& body)
  : weight(weight), items(sz, body_comp_rng, body) {
    assert(false);
    compute_weight();
  }
  
  parray(const Weight& weight,
         std::initializer_list<value_type> xs)
  : weight(weight), items(xs) {
    compute_weight();
  }
  
  parray(const parray& other)
  : weight(other.weight), items(other.items) {
    compute_weight();
  }
  
  parray(const Weight& weight,
         iterator lo, iterator hi)
  : weight(weight), items(lo, hi) {
    compute_weight();
  }
  
  parray& operator=(const parray& other) {
    if (&other == this) {
      return *this;
    }
    items = other.items;
    compute_weight();
    return *this;
  }
  
  parray& operator=(parray&& other) {
    items = std::move(other.items);
    weight.resize(cbegin(), cbegin());
    weight.swap(other.weight);
    return *this;
  }
  
  value_type& operator[](long i) {
    return items[i];
  }
  
  const value_type& operator[](long i) const {
    return items[i];
  }
  
  long size() const {
    return items.size();
  }
  
  void swap(parray& other) {
    items.swap(other.items);
    weight.swap(other.weight);
  }
  
  void resize(long n, const value_type& val) {
    items.resize(n, val);
    compute_weight();
  }
  
  void resize(long n) {
    value_type val;
    resize(n, val);
  }
  
  void clear() {
    resize(0);
  }
  
  template <class Body>
  void tabulate(long n, const Body& body) {
    items.tabulate(n, body);
    compute_weight();
  }
  
  template <class Body, class Body_comp_rng>
  void tabulate(long n, const Body_comp_rng& body_comp_rng, const Body& body) {
    items.tabulate(n, body_comp_rng, body);
    compute_weight();
  }
  
  iterator begin() const {
    return iterator(items.begin(), weight.begin());
  }
  
  const_iterator cbegin() const {
    return const_iterator(items.cbegin(), weight.begin());
  }
  
  iterator end() const {
    return iterator(items.end(), weight.end());
  }
  
  const_iterator cend() const {
    return const_iterator(items.cend(), weight.end());
  }
  
};

} // end namespace

/***********************************************************************/

} // end namespace
} // end namespace

#endif /*! _PCTL_PARRAY_BASE_H_ */
