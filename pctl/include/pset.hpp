/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file parray.hpp
 * \brief Array-based implementation of sequences
 *
 */

#include <cmath>

#include "datapar.hpp"
#include "chunkedseq.hpp"

#ifndef _PCTL_PSET_BASE_H_
#define _PCTL_PSET_BASE_H_

namespace pasl {
namespace pctl {

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Parallel set */

namespace {
  
  template <class Item_ptr>
  class optional {
  public:
    
    using self_type = optional<Item_ptr>;
    
    Item_ptr item_ptr;
    bool no_item;
    
    optional()
    : item_ptr(nullptr), no_item(true) { }
    
    optional(Item_ptr item_ptr)
    : item_ptr(item_ptr), no_item(false) { }
    
    optional(const optional& other)
    : item_ptr(other.item_ptr), no_item(other.no_item) { }
    
    void swap(self_type& other) {
      std::swap(item_ptr, other.item_ptr);
      std::swap(no_item, other.no_item);
    }
    
  };
  
  template <class Item_ptr, class Measured>
  class get_last_item {
  public:
    
    using value_type = value_type_of<Item_ptr>;
    using measured_type = Measured;
    
    measured_type operator()(const value_type& v) const {
      return measured_type(&v);
    }
    
    measured_type operator()(const value_type* lo, const value_type* hi) const {
      if (hi - lo == 0) {
        return measured_type();
      } else {
        return measured_type(hi - 1);
      }
    }
  };

  template <class Option>
  class take_right_if_nonempty {
  public:
    
    using value_type = Option;
    
    static constexpr bool has_inverse = false;
    
    static value_type identity() {
      return value_type();
    }
    
    static value_type combine(value_type left, value_type right) {
      if (right.no_item) {
        return left;
      } else {
        return right;
      }
    }
    
    static value_type inverse(value_type x) {
      util::atomic::fatal([] {
        std::cout << "inverse operation not supported" << std::endl;
      });
      return identity();
    }
    
  };

  //! [pset_cache]
  template <class Item_ptr, class Size>
  class pset_cache {
  public:
    
    using size_type = Size;
    using value_type = Item_ptr;
    using optional_type = optional<value_type>;
    using algebra_type = take_right_if_nonempty<optional_type>;
    using measured_type = typename algebra_type::value_type; // = optional_type
    using measure_type = get_last_item<value_type, measured_type>;
    
    static void swap(measured_type& x, measured_type& y) {
      x.swap(y);
    }
    
  };

  
} // end namespace
  
template <
  class Item,
  class Compare = std::less<Item>,
  class Alloc = std::allocator<Item>,
  int chunk_capacity = 8
>
class pset {
public:
  
  using key_type = Item;
  using value_type = Item;
  using key_compare = Compare;
  using allocator_type = Alloc;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using difference_type = ptrdiff_t;
  using size_type = size_t;
  
private:
  
  using cache_type = pset_cache<const_pointer, size_type>;
  using container_type = data::chunkedseq::bootstrapped::deque<value_type, chunk_capacity, cache_type>;
  using option_type = typename cache_type::measured_type;
  
public:
  
  using iterator = typename container_type::iterator;
  using const_iterator = typename container_type::const_iterator;
  
private:
  
  // invariant: items in seq are sorted in ascending order by their key values
  mutable container_type seq;
  mutable iterator it;
  
  static bool option_compare(option_type lhs, option_type rhs) {
    if (lhs.no_item && rhs.no_item) {
      return false;
    }
    if (lhs.no_item) {
      return true;
    }
    if (rhs.no_item) {
      return false;
    }
    key_compare comp;
    return comp(*lhs.item_ptr, *rhs.item_ptr);
  }
  
  static bool same_key(const key_type& lhs, const key_type& rhs) {
    key_compare comp;
    return (!comp(lhs, rhs)) && (!comp(rhs, lhs));
  }
  
  static bool same_option(const option_type& lhs, const option_type& rhs) {
    return ((!option_compare(lhs, rhs)) && (!option_compare(rhs, lhs)));
  }
  
  /* returns either
   *   - the position of the first item in the
   *     sequence `seq` that is either larger than
   *     or equal to `k`, if there is such a position
   *   - the position one past the end of `seq`, otherwise
   */
  iterator first_larger_or_eq(const key_type& k) const {
    option_type target(&k);
    it.search_by([target] (const option_type& key) {
      return option_compare(target, key) || same_option(target, key);
    });
    return it;
  }
  
public:
  
  pset() {
    it = seq.begin();
  }
  
  pset(const pset& other)
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
    assert(false);
    iterator it = first_larger_or_eq(k);
    return (same_key(*it, k)) ? it : seq.end();
  }
  
  std::pair<iterator,bool> insert(const value_type& val) {
    bool already = false;
    it = first_larger_or_eq(val);
    if (it == seq.end()) {
      // val is currently the largest item in the container
      std::cout << val << std::endl;
      seq.push_back(val);
    } else if (same_key(*it, val)) {
      // val is present in the container
      Item x = *it;
      std::cout << x << std::endl;
      already = true;
    } else {
      // val is currently not yet present in the container
      Item x = *it;
      std::cout << x << std::endl;
      it = seq.insert(it, val);
    }
    return std::make_pair(it, already);
  }
  
  void erase(iterator it) {
    if (it == seq.end()) {
      return;
    }
    if (it + 1 == seq.end()) {
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
      out << v.first;
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
  
  const_iterator cbegin() const {
    return seq.cbegin();
  }
  
  const_iterator cend() const {
    return seq.cend();
  }
  
  void check() const {
    seq.check();
  }
  
};

/***********************************************************************/

} // end namespace
} // end namespace

#endif /*! _PCTL_PSET_BASE_H_ */
