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
  
static constexpr int cache_align_szb = 128;
static constexpr int nb_children = 2;
static constexpr int max_height = 6;
static constexpr int saturation_upper_bound = 32;
static constexpr double sleep_time = 10000.0;
static constexpr int initial_height = 4;

constexpr int nb_leaves = 1 << max_height;
constexpr int nb_leaves_target = nb_leaves / 2;

class node {
private:
public:
  
  template <class T>
  static T* tagged_pointer_of(T* n) {
    return pasl::data::tagged::extract_value<T*>(n);
  }
  
  template <class T>
  static int tagged_tag_of(T* n) {
    return (int)pasl::data::tagged::extract_tag<long>(n);
  }
  
  template <class T>
  static T* tagged_tag_with(T* n, int t) {
    return pasl::data::tagged::create<T*, T*>(n, (long)t);
  }
  
  static constexpr int one_half = -1;
  static constexpr int root_node_tag = 1;
  
  using contents_type = struct {
    int c; // counter value
    int v; // version number
  };
  
  using child_pointer_type = std::atomic<node*>;
  static constexpr int child_pointer_szb = sizeof(child_pointer_type);
  using aligned_child_cell_type = std::aligned_storage<child_pointer_szb, cache_align_szb>::type;
  
  std::atomic<bool> saturated;
  char _padding1[cache_align_szb];
  std::atomic<contents_type> X;
  char _padding2[cache_align_szb];
  node* parent;
  char _padding3[cache_align_szb];
  aligned_child_cell_type children[nb_children];
  char _padding4[cache_align_szb];
  
  static bool is_root_node(const node* n) {
    return tagged_tag_of(n) == 1;
  }
  
  template <class Item>
  static node* create_root_node(Item x) {
    return (node*)tagged_tag_with(x, root_node_tag);
  }
  
  child_pointer_type& child_cell_at(std::size_t i) {
    assert((i == 0) || (i == 1));
    return *reinterpret_cast<child_pointer_type*>(children+i);
  }
  
public:
  
  node(node* _parent = nullptr) {
    parent = (_parent == nullptr) ? create_root_node(_parent) : _parent;
    contents_type init;
    init.c = 0;
    init.v = 0;
    X.store(init);
    for (int i = 0; i < nb_children; i++) {
      child_cell_at(i).store(nullptr);
    }
    saturated.store(false);
  }
  
  bool is_saturated() const {
    return saturated.load();
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
        succ = X.compare_exchange_strong(orig, next);
      }
      if (x.c == 0) {
        contents_type orig = x;
        contents_type next = x;
        next.c = one_half;
        next.v++;
        if (X.compare_exchange_strong(orig, next)) {
          succ = true;
          x.c = one_half;
          x.v++;
        }
      }
      if (succ && (x.v == saturation_upper_bound)) {
        saturated.store(true);
      }
      if (! succ) {
        pasl::util::microtime::microsleep(sleep_time);
      }
      if (x.c == one_half) {
        if (! is_root_node(parent)) {
          parent->increment();
        }
        contents_type orig = x;
        contents_type next = x;
        next.c = 1;
        if (! X.compare_exchange_strong(orig, next)) {
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
      if (X.compare_exchange_strong(orig, next)) {
        bool s = (x.c == 1);
        if (is_root_node(parent)) {
          return s;
        } else if (s) {
          return parent->decrement();
        } else {
          return false;
        }
      } else {
        pasl::util::microtime::microsleep(sleep_time);
      }
    }
  }
  
  node* try_create_child(int i, bool& created_new_child) {
    created_new_child = false;
    child_pointer_type& child = child_cell_at(i);
    node* orig = child.load();
    if (orig != nullptr) {
      return orig;
    }
    node* n = new node(this);
    if (child.compare_exchange_strong(orig, n)) {
      created_new_child = true;
      return n;
    } else {
      pasl::util::microtime::microsleep(sleep_time);
      delete n;
      return child.load();
    }
  }

  node* try_create_child(int i) {
    bool dummy;
    return try_create_child(i, dummy);
  }
  
  node* get_child(int i) {
    return child_cell_at(i).load();
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
  
class tree {
private:
  
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
  
  node* root;

  std::atomic<int> nb_at_leaves;

  node** leaves;
  
  void destroy(node* n) {
    if (n == nullptr) {
      return;
    }
    for (int i = 0; i < nb_children; i++) {
      destroy(n->get_child(i));
    }
    delete n;
  }

  int nb_nodes(node* n) {
    int nb = 1;
    for (int i = 0; i < nb_children; i++) {
      node* c = n->get_child(0);
      if (c != nullptr) {
        nb += nb_nodes(c);
      }
    }
    return nb;
  }

  void grow_to(int height, node* n) {
    if (height == 0) {
      return;
    }
    for (int i = 0; i < nb_children; i++) {
      node* c = n->try_create_child(i);
      grow_to(height - 1, c);
    }
  }

  unsigned int leaf_index_of_path(unsigned int path) {
    unsigned int result = path & ((1 << (max_height + 1)) - 1);
    assert( result >= 0);
    assert(result < nb_leaves);
    return result;
  }

  node*& leaf_node_from_path(unsigned int path) {
    assert(leaves != nullptr);
    return leaves[leaf_index_of_path(path)];
  }
  
public:
  
  using node_type = node;
  
  tree() {
    root = new node;
    grow_to(initial_height, root);
    nb_at_leaves.store(0);
    leaves = nullptr;
  }
  
  ~tree() {
    destroy(root);
    if (leaves != nullptr) {
      free(leaves);
    }
  }
  
  node* get_target_of_path(unsigned int path) {
    node* result = root;
    int height = 0;
    if (leaves != nullptr) {
      node* n = leaf_node_from_path(path);
      if (n != nullptr) {
        return n;
      }
    }
    while (true) {
      if (height >= max_height) {
        break;
      }
      if (! result->is_saturated()) {
        break;
      }
      int i = path & 1;
      bool created_new_child;
      result = result->try_create_child(i, created_new_child);
      if (created_new_child && (height + 1 == max_height)) {
        if ((nb_at_leaves.load() <= nb_leaves_target) && (++nb_at_leaves == nb_leaves_target)) {
          leaves = (node**)malloc(sizeof(node*) * nb_leaves);
          for (int i = 0; i < nb_leaves; i++) {
            leaves[i] = nullptr;
          }
        }
      }
      if ((leaves != nullptr) && (height + 1 == max_height) && (leaf_node_from_path(path) == nullptr)) {
        leaf_node_from_path(path) = result;
      }
      path = (path >> 1);
      height++;
    }
    return result;
  }
  
  template <class Item>
  node* get_target_of_value(Item x) {
    return get_target_of_path(random_path_for(x));
  }
  
  static void increment(node* target) {
    target->increment();
  }
  
  static bool decrement(node* target) {
    return target->decrement();
  }
  
  template <class Item>
  void set_root_annotation(Item x) {
    node::set_root_annotation(root, x);
  }
  
};

} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/


#endif /*! _PASL_PARUTIL_GSNZI_H_ */
