/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief STL-style map data structure
 * \file map.hpp
 *
 */

#include <iostream>
#include <fstream>
#include <map>

#include "atomic.hpp"
#include "chunkedseq.hpp"

#ifndef _PASL_DATA_MAP_H_
#define _PASL_DATA_MAP_H_

namespace pasl {
namespace data {
namespace map {
  
/***********************************************************************/

/*---------------------------------------------------------------------*/
//! [option]
template <class Item, class Item_swap>
class option {
public:
  
  using self_type = option<Item, Item_swap>;
  
  Item item;
  bool no_item;
  
  option()
  : item(), no_item(true) { }
  
  option(const Item& item)
  : item(item), no_item(false) { }
  
  option(const option& other)
  : item(other.item), no_item(other.no_item) { }
  
  void swap(option& other) {
    Item_swap::swap(item, other.item);
    std::swap(no_item, other.no_item);
  }
  
  bool operator<(const self_type& y) const {
    if (no_item && y.no_item)
      return false;
    if (no_item)
      return true;
    if (y.no_item)
      return false;
    return item < y.item;
  }
    
  bool operator>=(const self_type& y) const {
    return ! (*this < y);
  }
  
};
//! [option]
  
/*---------------------------------------------------------------------*/
//! [get_key_of_last_item]

template <class Item, class Measured>
class get_key_of_last_item {
public:
  
  using value_type = Item;
  using key_type = typename value_type::first_type;
  using measured_type = Measured;
  
  measured_type operator()(const value_type& v) const {
    key_type key = v.first;
    return measured_type(key);
  }
  
  measured_type operator()(const value_type* lo, const value_type* hi) const {
    if (hi - lo == 0)
      return measured_type();
    const value_type* last = hi - 1;
    key_type key = last->first;
    return measured_type(key);
  }
};
//! [get_key_of_last_item]

/*---------------------------------------------------------------------*/
//! [take_right_if_nonempty]
template <class Option>
class take_right_if_nonempty {
public:
  
  using value_type = Option;
  
  static constexpr bool has_inverse = false;
  
  static value_type identity() {
    return value_type();
  }

  static value_type combine(value_type left, value_type right) {
    if (right.no_item)
      return left;
    return right;
  }
  
  static value_type inverse(value_type x) {
    util::atomic::fatal([] {
      std::cout << "inverse operation not supported" << std::endl;
    });
    return identity();
  }
  
};
//! [take_right_if_nonempty]
  
/*---------------------------------------------------------------------*/

//! [map_cache]
template <class Item, class Size, class Key_swap>
class map_cache {
public:
  
  using size_type = Size;
  using value_type = Item;
  using key_type = typename value_type::first_type;
  using option_type = option<key_type, Key_swap>;
  using algebra_type = take_right_if_nonempty<option_type>;
  using measured_type = typename algebra_type::value_type; // = option_type
  using measure_type = get_key_of_last_item<value_type, measured_type>;
  
  static void swap(measured_type& x, measured_type& y) {
    x.swap(y);
  }
  
};
//! [map_cache]

/*---------------------------------------------------------------------*/
//! [swap]
template <class Item>
class std_swap {
public:
  
  static void swap(Item& x, Item& y) {
    std::swap(x, y);
  }
  
};
//! [swap]
  
/*---------------------------------------------------------------------*/
//! [map]
template <class Key,
          class Item,
          class Compare = std::less<Key>,
          class Key_swap = std_swap<Key>,
          class Alloc = std::allocator<std::pair<const Key, Item> >,
          int chunk_capacity = 8
          >
class map {
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
  using key_swap_type = Key_swap;
  
private:
  
  using cache_type = map_cache<value_type, size_type, key_swap_type>;
  using container_type = chunkedseq::bootstrapped::deque<value_type, chunk_capacity, cache_type>;
  using option_type = typename cache_type::measured_type;
  
public:
  
  using iterator = typename container_type::iterator;
  
private:
  
  // invariant: items in seq are sorted in ascending order by their key values
  mutable container_type seq;
  mutable iterator it;
  
  iterator upper(const key_type& k) const {
    option_type target(k);
    it.search_by([target] (const option_type& key) {
      return target >= key;
    });
    return it;
  }
  
public:
  
  map() {
    it = seq.begin();
  }
  
  map(const map& other)
  : seq(other.seq) {
    it = seq.begin();
  }
  
  size_type size() const {
    return seq.size();
  }
  
  bool empty() const {
    return size() == 0;
  }
  
  iterator find(const key_type& k) const {
    iterator it = upper(k);
    return ((*it).first == k) ? it : seq.end();
  }
  
  mapped_type& operator[](const key_type& k) const {
    it = upper(k);
    if (it == seq.end()) {
      // key k is larger than any key current in seq
      value_type val;
      val.first = k;
      seq.push_back(val);
      it = seq.end()-1;
    } else if ((*it).first != k) {
      // iterator it points to the first key that is less than k
      value_type val;
      val.first = k;
      it = seq.insert(it, val);
    }
    return (*it).second;
  }
  
  void erase(iterator it) {
    if (it == seq.end())
      return;
    if (it == seq.end()-1) {
      seq.pop_front();
      return;
    }
    seq.erase(it, it+1);
  }
  
  size_type erase(const key_type& k) {
    size_type nb = seq.size();
    erase(find(k));
    return nb - seq.size();
  }
  
  std::ostream& stream(std::ostream& out) const {
    out << "[";
    size_type sz = size();
    seq.for_each([&] (value_type v) {
      out << "(" << v.first << "," << v.second << ")";
      if (sz-- != 1)
        out << ",";
    });
    return out << "]";
  }
  
  iterator begin() const {
    return seq.begin();
  }
  
  iterator end() const {
    return seq.end();
  }
  
  void check() const {
    seq.check();
  }
  
};
//! [map]
  
/***********************************************************************/
  
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_MAP_H_ */