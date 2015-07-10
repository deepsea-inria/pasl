/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file pmap.hpp
 * \brief Parallel comparison-based dynamic dictionary
 *
 */

#include <cmath>

#include "pset.hpp"

#ifndef _PCTL_PMAP_BASE_H_
#define _PCTL_PMAP_BASE_H_

namespace pasl {
namespace pctl {

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Parallel map */
  
template <
  class Key,
  class Item,
  class Compare = std::less<Key>,
  class Alloc = std::allocator<std::pair<Key, Item>>,
  int chunk_capacity = 8
>
class pmap {
public:
  
  using key_type = Key;
  using mapped_type = Item;
  using value_type = std::pair<key_type, mapped_type>;
  using key_compare = Compare;
  using allocator_type = Alloc;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using difference_type = ptrdiff_t;
  using size_type = size_t;
  class value_compare {
  public:
    
    bool operator()(const value_type& lhs, const value_type& rhs) const {
      Compare comp;
      return comp(lhs.first, rhs.first);
    }
    
  };
  using pset_type = pset<value_type, value_compare, allocator_type, chunk_capacity>;
  using iterator = typename pset_type::iterator;
  using const_iterator = typename pset_type::const_iterator;
  
  pset_type set;
  
  pmap() { }
  
  ~pmap() {
    clear();
  }
  
  pmap(const pmap& other)
  : set(other.set) { }
  
  pmap(pmap&& other)
  : set(std::move(other.set)) { }
  
  pmap(std::initializer_list<value_type> xs)
  : set(xs) { }
  
  template <class Iter>
  pmap(Iter lo, Iter hi)
  : set(lo, hi) { }
  
  pmap(long sz, const std::function<value_type(long)>& body)
  : set(sz, body) { }
  
  pmap(long sz,
       const std::function<long(long)>& body_comp,
       const std::function<value_type(long)>& body)
  : set(sz, body_comp, body) { }
  
  size_type size() const {
    return set.size();
  }
  
  bool empty() const {
    return size() == 0;
  }
  
  
  iterator find(const key_type& k) {
    return set.find(std::make_pair(k, mapped_type()));
  }
  
  const_iterator find(const key_type& k) const {
    return set.find(std::make_pair(k, mapped_type()));
  }
  
  std::pair<iterator,bool> insert(const value_type& val) {
    return set.insert(val);
  }
  
  void erase(iterator it) {
    set.erase(it);
  }
  
  size_type erase(const key_type& k) {
    set.erase(std::make_pair(k, mapped_type()));
  }
  
  mapped_type& operator[] (const key_type& k) {
    mapped_type tmp;
    value_type val = std::make_pair(k, tmp);
    set.insert(val);
    auto it = set.find(val);
    return (*it).second;
  }
  
  void merge(pmap& other) {
    set.merge(other.set);
  }
  
  void intersect(pmap& other) {
    set.intersect(other.set);
  }
  
  void diff(pmap& other) {
    set.diff(other.set);
  }
  
  void clear() {
    set.clear();
  }
  
  iterator begin() const {
    return set.begin();
  }
  
  iterator end() const {
    return set.end();
  }
  
  const_iterator cbegin() const {
    return set.cbegin();
  }
  
  const_iterator cend() const {
    return set.cend();
  }
  
};
  
} // end namespace
} // end namepace

#endif /*! _PCTL_PMAP_BASE_H_ */