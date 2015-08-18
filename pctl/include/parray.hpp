/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file parray.hpp
 * \brief Array-based implementation of sequences
 *
 * \todo 
 * - only propagate the allocator on swap calls if propagate_on_container_swap
 *   is true
 */

#ifndef _PCTL_PARRAY_BASE_H_
#define _PCTL_PARRAY_BASE_H_

#include "callable.hpp"
#include "pmem.hpp"
#include <cmath>
#include <memory>

namespace pasl {
namespace pctl {

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Parallel array */
  
template <class Item, class Allocator = std::allocator<Item>>
class parray {
public:
  
  using value_type = Item;
  using allocator_type = Allocator;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = pointer;
  using const_iterator = const_pointer;
  
private:
  using allocator_traits = std::allocator_traits<allocator_type>;
  
  struct deleter : allocator_type {
    using allocator_type::allocator_type;
    
    void operator()(pointer ptr) {
      allocator_traits::deallocate(*static_cast<allocator_type*>(this),
                                   ptr, size);
    }
    
    deleter(const deleter& other) noexcept
    : allocator_type(allocator_traits::
                     select_on_container_copy_construction(static_cast<
                                                             const allocator_type&
                                                           >(other)))
    , size(other.size) {}
    
    deleter(deleter&& other) noexcept {
      size = other.size;
      other.size = 0;
    }
    
    deleter& operator=(const deleter& other) noexcept {
      if(allocator_traits::propagate_on_copy_assignment::value)
        allocator_type::operator=(static_cast<allocator_type&>(other));
      size = other.size;
      return *this;
    }
    
    deleter& operator=(deleter&& other) noexcept {
      if(allocator_traits::propagate_on_container_move_assignment::value)
        allocator_type::operator=(static_cast<allocator_type&&>(other));
      size = std::move(other.size);
      other.size = 0;
      return *this;
    }
    
    size_type size = 0;
  };
  
  std::unique_ptr<value_type[], deleter> content;
  
  allocator_type& get_allocator() noexcept {
    return static_cast<allocator_type&>(content.get_deleter());
  }
  
  const allocator_type& get_allocator() const noexcept {
    return static_cast<const allocator_type&>(content.get_deleter());
  }
  
  void set_size(size_type sz) noexcept {
    content.get_deleter().size = sz;
  }
  
  void allocate(size_type n) {
    set_size(n);
    assert(size() >= 0);
    allocator_type& alloc = get_allocator();
    pointer p = allocator_traits::allocate(alloc, size());
    assert(p != nullptr);
    content.reset(p);
  }
  
  void destroy() noexcept {
    pmem::pdelete(begin(), end(), get_allocator());
    set_size(0);
  }
  
  void realloc(size_type n) {
    destroy();
    allocate(n);
  }
  
  void fill(size_type n, const_reference value) {
    realloc(n);
    pmem::fill(begin(), end(), value, get_allocator());
  }
  
  void check(size_type i) const {
    assert(content.get() != nullptr);
    assert(i < size());
    (void) i; // avoid unused parameter warning
  }
  
public:
  
  explicit parray()
  : parray(allocator_type()) {}
  
  explicit parray(const allocator_type& allocator)
  : content(nullptr, deleter(allocator)) {}
  
  explicit parray(size_type size, const_reference val,
                  const allocator_type& allocator = allocator_type())
  : content(nullptr, deleter(allocator)) {
    fill(size, val);
  }
  
  explicit parray(size_type size,
                  const allocator_type& allocator = allocator_type())
  : content(nullptr, deleter(allocator)) {
    fill(size, value_type());
  }
  
  template<
    class Provider,
    class Selector =
    typename std::enable_if<
               pmem::is_valid_provider<Provider, value_type, iterator>::value
             >::type
  >
  parray(size_type size, Provider&& body,
         const allocator_type& allocator = allocator_type())
  : content(nullptr, deleter(allocator)) {
    tabulate(size, std::forward<Provider>(body));
  }
  
  template<
    class WorkEstimator, class Provider,
    class Selector =
    typename std::enable_if<
      pmem::is_valid_estimator<WorkEstimator, iterator>::value &&
      pmem::is_valid_provider<Provider, value_type, iterator>::value
    >::type
  >
  parray(size_type size, WorkEstimator&& body_comp, Provider&& body,
         const allocator_type& allocator = allocator_type())
  : content(nullptr, deleter(allocator)) {
    tabulate(size, std::forward<WorkEstimator>(body_comp),
             std::forward<Provider>(body));
  }
  
  parray(std::initializer_list<value_type> xs,
         const allocator_type& allocator = allocator_type())
  : content(nullptr, deleter(allocator)) {
    allocate(xs.size());
    auto low = xs.begin();
    pmem::tabulate(begin(), end(), [low](size_type i) {
      return *(low + i);
    }, get_allocator());
  }
  
  parray(const parray& other)
  : content(nullptr, deleter(other.content.get_deleter())) {
    allocate(other.size());
    using namespace pmem::_detail;
    if(!is_trivially_copyable<value_type>::value)
      pmem::tabulate(begin(), end(), [&other](size_type i) {
        return other[i];
      }, get_allocator());
    else
      pmem::copy(other.cbegin(), other.cend(), begin());
  }
  
  parray(parray&& other) = default;
  
  parray(iterator lo, iterator hi) {
    difference_type n = hi - lo;
    if (n < 0)
      return;
    allocate(n);
    using namespace pmem::_detail;
    if(!is_trivially_copyable<value_type>::value)
      pmem::tabulate(begin(), end(), [lo](size_type i) {
        return *(lo + i);
      }, get_allocator());
    else
      pmem::copy(lo, hi, begin());
  }
  
  ~parray() noexcept {
    destroy();
  }
  
  parray& operator=(const parray& other) {
    if (&other == this)
      return *this;
    realloc(other.size());
    using namespace pmem::_detail;
    if(is_trivially_copyable<value_type>::value)
      pmem::copy(other.cbegin(), other.cend(), begin());
    else
      pmem::tabulate(begin(), end(), [&other](size_type i) {
        return other[i];
      }, get_allocator());
    return *this;
  }
  
  parray& operator=(parray&& other) = default;
  
  reference operator[](size_type i) {
    check(i);
    return content[i];
  }
  
  const_reference operator[](size_type i) const {
    check(i);
    return content[i];
  }
  
  size_type size() const {
    return content.get_deleter().size;
  }
  
  void swap(parray& other) {
    std::swap(content, other.content);
  }
  
  void resize(size_type n, const_reference val) {
    if (n == size())
      return;
    parray<value_type, allocator_type> tmp(n, val, get_allocator());
    swap(tmp);
    size_type m = std::min(tmp.size(), size());
    assert(size() >= m);
    pmem::copy(tmp.cbegin(), tmp.cbegin() + m, begin());
  }
  
  void resize(size_type n) {
    resize(n, value_type());
  }
  
  void clear() noexcept {
    destroy();
  }
  
  template <class Body>
  auto tabulate(size_type n, Body&& body) ->
  typename std::enable_if<
             pmem::is_valid_provider<Body, value_type, iterator>::value
           >::type {
    realloc(n);
    pmem::tabulate(begin(), end(), std::forward<Body>(body), get_allocator());
  }
  
  template <class Body_comp_rng, class Body>
  auto tabulate(size_type n, Body_comp_rng&& body_comp_rng, Body&& body) ->
  typename std::enable_if<
             pmem::is_valid_estimator<Body_comp_rng, iterator>::value &&
             pmem::is_valid_provider<Body, value_type, iterator>::value
           >::type {
    realloc(n);
    pmem::tabulate(begin(), end(), std::forward<Body_comp_rng>(body_comp_rng),
                   std::forward<Body>(body), get_allocator());
  }
  
  iterator begin() noexcept {
    return &content[0];
  }
  
  const_iterator begin() const noexcept {
    return cbegin();
  }
  
  const_iterator cbegin() const noexcept {
    return &content[0];
  }
  
  iterator end() noexcept {
    return &content[size()];
  }
  
  const_iterator end() const noexcept {
    return cend();
  }
  
  const_iterator cend() const noexcept {
    return &content[size()];
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
