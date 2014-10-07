/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Defines an interface for attaching data to nodes in the
 * underlying tree structure of the chunked sequence
 * \file annotation.hpp
 *
 */

#include <assert.h>
#include <algorithm>

#include "tagged.hpp"

#ifndef _PASL_DATA_ANNOTATION_H_
#define _PASL_DATA_ANNOTATION_H_

namespace pasl {
namespace data {
namespace chunkedseq {
namespace annotation {

/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/* Optional support for parent pointers */
  
using parent_pointer_tag = enum {
  PARENT_POINTER_BOOTSTRAP_INTERIOR_NODE=0,
  PARENT_POINTER_BOOTSTRAP_LAYER_NODE=1,
  PARENT_POINTER_CHUNK=2,
  PARENT_POINTER_UNINITIALIZED=3
};
  
template <class Measured>
class with_parent_pointer {
public:

  using self_type = with_parent_pointer<Measured>;
  
private:
  
  mutable void* parent_ptr;
  mutable Measured prefix;
  mutable int depth;
  
public:
  
  static constexpr bool enabled = true;
  
  with_parent_pointer() {
    set_pointer(nullptr, PARENT_POINTER_UNINITIALIZED);
    depth = -1;
  }
  
  template <class Pointer>
  Pointer get_pointer() const {
    return tagged::extract_value<Pointer, void*>(parent_ptr);
  }
  
  parent_pointer_tag get_tag() const {
    long t = tagged::extract_tag<void*, void*>(parent_ptr);
    if (t == PARENT_POINTER_BOOTSTRAP_INTERIOR_NODE)
      return PARENT_POINTER_BOOTSTRAP_INTERIOR_NODE;
    else if (t == PARENT_POINTER_BOOTSTRAP_LAYER_NODE)
      return PARENT_POINTER_BOOTSTRAP_LAYER_NODE;
    else if (t == PARENT_POINTER_UNINITIALIZED)
      return PARENT_POINTER_UNINITIALIZED;
    else {
      assert(false);
      return PARENT_POINTER_UNINITIALIZED;
    }
  }
  
  int get_depth() const {
    return depth;
  }
  
  template <class Measured1>
  Measured1 get_prefix() const {
    return prefix;
  }
  
  template <class Pointer>
  void set_pointer(Pointer p, parent_pointer_tag t) const {
    parent_ptr = tagged::create<Pointer, void*>(p, t);
    assert(get_tag() == t);
  }
  
  void set_depth(int d) const {
    depth = d;
  }
  
  template <class Measured1>
  void set_prefix(Measured1 m) const {
    prefix = m;
  }
  
  void swap(self_type& other) {
    std::swap(parent_ptr, other.parent_ptr);
    std::swap(depth, other.depth);
    std::swap(prefix, other.prefix);
  }
  
};
  
class without_parent_pointer {
public:
  
  using self_type = without_parent_pointer;
  
  static constexpr bool enabled = false;
  
  template <class Pointer>
  Pointer get_pointer() const {
    return nullptr;
  }
  
  parent_pointer_tag get_tag() const {
    return PARENT_POINTER_UNINITIALIZED;
  }
  
  int get_depth() const {
    return -1;
  }
  
  template <class Measured>
  Measured get_prefix() const {
    return Measured();
  }
  
  template <class Pointer>
  void set_pointer(Pointer, parent_pointer_tag) const {
  }
  
  void set_depth(int) const {

  }
  
  template <class Measured>
  void set_prefix(Measured) const {
    
  }
  
  void swap(self_type&) {
    
  }
  
};

/*---------------------------------------------------------------------*/
/* Optional suppport for chains of inter-chunk pointers */
  
class with_chain {
public:
  
  using self_type = with_chain;
  
private:
  
  void* next;
  void* prev;
  
public:
  
  static constexpr bool enabled = true;
  
  with_chain()
  : next(nullptr), prev(nullptr) { }
  
  template <class Pointer>
  Pointer get_next() const {
    return (Pointer)next;
  }
  
  template <class Pointer>
  Pointer get_prev() const {
    return (Pointer)prev;
  }
  
  template <class Pointer>
  static void link(self_type& l1, self_type& l2, Pointer p1, Pointer p2) {
    l1.next = (void*)p2;
    l2.prev = (void*)p1;
  }
  
  template <class Pointer>
  static void unlink(self_type& l1, self_type& l2, Pointer p1, Pointer p2) {
    assert(l1.get_next<void*>() == (void*)p2);
    assert((void*)p1 == l2.get_prev<void*>());
    l1.next = nullptr;
    l2.prev = nullptr;
  }
  
  void swap(self_type& other) {
    std::swap(next, other.next);
    std::swap(prev, other.prev);
  }
  
};

class without_chain {
public:
  
  using self_type = without_chain;
  
  static constexpr bool enabled = false;
  
  template <class Pointer>
  Pointer get_next() const {
    return nullptr;
  }
  
  template <class Pointer>
  Pointer get_prev() const {
    return nullptr;
  }
  
  template <class Pointer>
  static void link(self_type& l1, self_type& l2, Pointer p1, Pointer p2) {
  }
  
  template <class Pointer>
  static void unlink(self_type& l1, self_type& l2, Pointer p1, Pointer p2) {
  }
  
  void swap(self_type& other) {
  }
  
};
  
/*---------------------------------------------------------------------*/
/* Optional support for cached measurements */

class without_measured {
public:
  
  using measured_type = struct { };
  using self_type = without_measured;
  
  static constexpr bool enabled = false;
  
  template <class Measured>
  Measured get_cached() const {
    return Measured();
  }
  
  template <class Measured>
  void set_cached(Measured m) const {
    
  }
  
  void swap(self_type& other) {
    
  }
  
};
  
class std_swap {
public:
  template <class T>
  static void swap(T& x, T& y) {
    std::swap(x, y);
  }
};
  
template <class Measured, class Swap_measured=std_swap>
class with_measured {
public:
  
  using measured_type = Measured;
  using self_type = with_measured<measured_type, Swap_measured>;
  
  static constexpr bool enabled = true;
  
  mutable measured_type cached;
  
  template <class Measured1>
  Measured1 get_cached() const {
    return cached;
  }
  
  template <class Measured1>
  void set_cached(Measured1 m) const {
    cached = m;
  }
  
  void swap(self_type& other) {
    Swap_measured::swap(cached, other.cached);
  }
  
};
  
/*---------------------------------------------------------------------*/
/* Annotation builder */

template <
  class Measured=without_measured,
  class Parent_pointer=without_parent_pointer,
  class Sibling_pointer=without_chain
>
class annotation_builder {
public:
  
  using self_type = annotation_builder<Measured, Parent_pointer, Sibling_pointer>;
  using cached_prefix_type = Measured;
  using parent_pointer_type = Parent_pointer;
  
  static constexpr bool finger_search_enabled = Measured::enabled && Parent_pointer::enabled;
  
  Measured prefix;
  Parent_pointer parent;
  Sibling_pointer sibling;
  
  void swap(self_type& other) {
    prefix.swap(other.prefix);
    parent.swap(other.parent);
    sibling.swap(other.sibling);
  }
  
};
  
/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_ANNOTATION_H_ */