/*!
 * \file gsnzi.cpp
 * \brief Implementation of the Scalable Non-Zero Indicator
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
*/

#ifndef _PASL_PARUTIL_GSNZI_H_
#define _PASL_PARUTIL_GSNZI_H_

#include <atomic>
#include <array>
#include <type_traits>

#include "tagged.hpp"
#include "microtime.hpp"

/***********************************************************************/

namespace pasl {
namespace data {
namespace gsnzi {

// pre: sizeof(ty) <= align_szb
#define DECLARE_PADDED_FIELD(ty, name, align_szb)   \
  ty name;                                          \
  char _padding_for_ ## name[align_szb - sizeof(ty)]
  
namespace {
static constexpr int cache_align_szb = 128;
static constexpr int nb_children = 2;
static constexpr double sleep_time = 10000.0;
  
template <class T>
bool compare_exchange(std::atomic<T>& cell, T& expected, T desired) {
  if (cell.compare_exchange_strong(expected, desired)) {
    return true;
  }
  pasl::util::microtime::microsleep(sleep_time);
  return false;
}
  
template <class T>
T* tagged_pointer_of(T* n) {
  return pasl::data::tagged::extract_value<T*>(n);
}

template <class T>
int tagged_tag_of(T* n) {
  return (int)pasl::data::tagged::extract_tag<long>(n);
}

template <class T>
T* tagged_tag_with(T* n, int t) {
  return pasl::data::tagged::create<T*, T*>(n, (long)t);
}
} // end namespace
 
template <int saturation_upper_bound>
class node {
private:
  
  static constexpr int one_half = -1;
  static constexpr int root_node_tag = 1;
  
  using contents_type = struct {
    int c; // counter value
    int v; // version number
  };

  DECLARE_PADDED_FIELD(std::atomic<contents_type>, X, cache_align_szb);
  
  DECLARE_PADDED_FIELD(node*, parent, cache_align_szb);

  static bool is_root_node(const node* n) {
    return tagged_tag_of(n) == root_node_tag;
  }
  
  template <class Item>
  static node* create_root_node(Item x) {
    return (node*)tagged_tag_with(x, root_node_tag);
  }
  
public:
  
  // post: constructor zeros out all fields, except parent
  node(node* _parent = nullptr) {
    parent = (_parent == nullptr) ? create_root_node(_parent) : _parent;
    contents_type init;
    init.c = 0;
    init.v = 0;
    X.store(init);
  }
  
  bool is_saturated() const {
    return X.load().v >= saturation_upper_bound;
  }
  
  bool is_nonzero() const {
    return X.load().c > 0;
  }
  
  void increment() {
    bool succ = false;
    int undo_arr = 0;
    while (! succ) {
      contents_type x = X.load();
      if (x.c >= 1) {
        contents_type orig = x;
        contents_type next = x;
        next.c++;
        next.v++;
        succ = compare_exchange(X, orig, next);
      }
      if (x.c == 0) {
        contents_type orig = x;
        contents_type next = x;
        next.c = one_half;
        next.v++;
        if (compare_exchange(X, orig, next)) {
          succ = true;
          x.c = one_half;
          x.v++;
        }
      }
      if (x.c == one_half) {
        if (! is_root_node(parent)) {
          parent->increment();
        }
        contents_type orig = x;
        contents_type next = x;
        next.c = 1;
        if (! compare_exchange(X, orig, next)) {
          undo_arr++;
        }
      }
    }
    if (is_root_node(parent)) {
      return;
    }
    while (undo_arr > 0) {
      parent->decrement();
      undo_arr--;
    }
  }
  
  bool decrement() {
    while (true) {
      contents_type x = X.load();
      assert(x.c >= 1);
      contents_type orig = x;
      contents_type next = x;
      next.c--;
      if (compare_exchange(X, orig, next)) {
        bool s = (x.c == 1);
        if (is_root_node(parent)) {
          return s;
        } else if (s) {
          return parent->decrement();
        } else {
          return false;
        }
      }
    }
  }
  
  template <class Item>
  static void set_root_annotation(node* n, Item x) {
    node* m = n;
    assert(! is_root_node(m));
    while (! is_root_node(m->parent)) {
      m = m->parent;
    }
    assert(is_root_node(m->parent));
    m->parent = create_root_node(x);
  }
  
  template <class Item>
  static Item get_root_annotation(node* n) {
    node* m = n;
    while (! is_root_node(m)) {
      m = m->parent;
    }
    assert(is_root_node(m));
    return (Item)tagged_pointer_of(m);
  }
  
};
  
#undef DECLARE_PADDED_FIELD
  
template <
  int max_height = 6,  // must be > 1
  int saturation_upper_bound = (1<<(max_height-1)) // any constant fraction of 2^max_height
>
class tree {
public:
  
  using node_type = node<saturation_upper_bound>;
  
private:
  
  static constexpr int nb_leaves = 1 << max_height;
  static constexpr int heap_size = 2 * nb_leaves;
  
  static constexpr int loading_heap_tag = 1;
  
  static unsigned int hashu(unsigned int a) {
    a = (a+0x7ed55d16) + (a<<12);
    a = (a^0xc761c23c) ^ (a>>19);
    a = (a+0x165667b1) + (a<<5);
    a = (a+0xd3a2646c) ^ (a<<9);
    a = (a+0xfd7046c5) + (a<<3);
    a = (a^0xb55a4f09) ^ (a>>16);
    return a;
  }
  
  template <class Item>
  static unsigned int random_path_for(Item x) {
    union {
      Item x;
      long b;
    } bits;
    bits.x = x;
    return std::abs((int)hashu((unsigned int)bits.b));
  }
  
  node_type root;
  
  // if (heap.load() != nullptr), then we have a representation of a tree of height
  // max_height, using the array-based binary tree representation
  std::atomic<node_type*> heap;
  
  // only called once
  // pre: assume tagged_tag_of(heap.load()) == loading_heap_tag
  // post: heap.load() points to an array of pre-initialized SNZI nodes
  void create_heap() {
    assert(tagged_tag_of(heap.load()) == loading_heap_tag);
    size_t szb = heap_size * sizeof(node_type);
    node_type* h = (node_type*)malloc(szb);
    // std::memset(h, 0, szb);
    // cells at indices 0 and 1 are not used
    for (int i = 2; i < 4; i++) {
      new (&h[i]) node_type(&root);
    }
    for (int i = 4; i < heap_size; i++) {
      new (&h[i]) node_type(&h[i / 2]);
    }
    heap.store(h);
  }
  
public:
  
  tree() {
    assert(max_height > 1);
    heap.store(nullptr);
  }
  
  ~tree() {
    node_type* h = heap.load();
    assert(tagged_tag_of(h) != loading_heap_tag);
    if (h != nullptr) {
      free(heap);
    }
  }
  
  bool is_nonzero() const {
    return root.is_nonzero();
  }
  
  node_type* get_target_of_path(unsigned int path) {
    node_type* h = heap.load();
    if ((h != nullptr) && (tagged_tag_of(h) != loading_heap_tag)) {
      int i = (1 << max_height) + (path & ((1 << max_height) - 1));
      assert(i >= 2 && i < heap_size);
      return &h[i];
    } else if ((h == nullptr) && (root.is_saturated())) {
      node_type* orig = nullptr;
      node_type* next = tagged_tag_with<node_type>(nullptr, loading_heap_tag);
      if (compare_exchange(heap, orig, next)) {
        create_heap();
      }
    }
    return &root;
  }
  
  template <class Item>
  node_type* get_target_of_value(Item x) {
    return get_target_of_path(random_path_for(x));
  }
  
  static void increment(node_type* target) {
    target->increment();
  }
  
  static bool decrement(node_type* target) {
    return target->decrement();
  }
  
  template <class Item>
  void set_root_annotation(Item x) {
    node_type::set_root_annotation(&root, x);
  }
  
};

} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/


#endif /*! _PASL_PARUTIL_GSNZI_H_ */
