/*!
 * \file outset.hpp
 * \brief Implementation of the scalable outset data structure
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 */

#ifndef _PASL_DATA_OUTSET_H_
#define _PASL_DATA_OUTSET_H_

#include <atomic>
#include <array>
#include <type_traits>
#include <deque>

#include "tagged.hpp"
#include "microtime.hpp"
#include "perworker.hpp"

/***********************************************************************/

namespace pasl {
namespace data {
namespace outset {
  
namespace {
  
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

template <class T>
T* tagged_tag_with(int t) {
  return tagged_tag_with((T*)nullptr, t);
}

static constexpr int backoff_nb_cycles = 1l << 17;
template <class T>
bool compare_exchange(std::atomic<T>& cell, T& expected, T desired) {
  if (cell.compare_exchange_strong(expected, desired)) {
    return true;
  }
  pasl::util::microtime::wait_for(backoff_nb_cycles);
  return false;
}
  
template <class Item, int nb>
class static_cache_aligned_array {
private:
  
  static constexpr int cache_align_szb = 128;
  static constexpr int item_szb = sizeof(Item);
  
  using aligned_item_type = typename std::aligned_storage<item_szb, cache_align_szb>::type;
  
  aligned_item_type items[nb];
  
  Item& at(std::size_t i) {
    assert(i >= 0);
    assert(i < nb);
    return *reinterpret_cast<Item*>(items + i);
  }
  
public:
 
  Item& operator[](std::size_t i) {
    return at(i);
  }

  std::size_t size() const {
    return nb;
  }

  void init(Item x) {
    for (int i = 0; i < size(); i++) {
      at(i) = x;
    }
  }
  
};
  
static constexpr int finished_tag = 1;
  
} // end namespace

template <class Item, int capacity, bool multiadd>
class block {
public:
  
  using cell_type = std::atomic<Item>;
  
private:

  cell_type* start;
  std::atomic<cell_type*> head;
  
  static bool try_insert_item_at(cell_type& cell, Item x) {
    assert(multiadd);
    while (true) {
      Item y = cell.load();
      if (tagged_tag_of(y) == finished_tag) {
        return false;
      }
      assert(y == nullptr);
      Item orig = y;
      Item next = x;
      if (compare_exchange(cell, orig, next)) {
        return true;
      }
    }
  }
  
  static Item try_finish_item_at(cell_type& cell) {
    assert(multiadd);
    while (true) {
      Item y = cell.load();
      assert(tagged_tag_of(y) != finished_tag);
      if (y != nullptr) {
        return y;
      }
      Item orig = nullptr;
      Item next = tagged_tag_with((Item)  nullptr, finished_tag);
      if (compare_exchange(cell, orig, next)) {
        return nullptr;
      }
    }
  }
  
public:

  block() {
    assert(sizeof(Item) == sizeof(void*));
    start = (cell_type*)malloc(capacity * sizeof(cell_type));
    head.store(start);
    if (multiadd) {
      for (int i = 0; i < capacity; i++) {
        start[i].store(nullptr);
      }
    }
  }

  ~block() {
    free(start);
    start = nullptr;
    head.store(nullptr);
  }

  bool is_full() const {
    return head.load() >= (start + capacity);
  }
  
  using try_insert_result_type = enum {
    succeeded, failed_because_finish, failed_because_full
  };
  
  try_insert_result_type try_insert(Item x) {
    assert(x != nullptr);
    while (true) {
      cell_type* h = head.load();
      if (tagged_tag_of(h) == finished_tag) {
        return failed_because_finish;
      }
      if (h >= (start + capacity)) {
        return failed_because_full;
      }
      cell_type* orig = h;
      if (multiadd) {
        if (compare_exchange(head, orig, h + 1)) {
          if (! try_insert_item_at(*h, x)) {
            return failed_because_finish;
          }
          return succeeded;
        }
      } else {
        h->store(x);
        if (compare_exchange(head, orig, h + 1)) {
          return succeeded;
        }
      }
    }    
  }
  
  std::pair<cell_type*, cell_type*> finish_init() {
    while (true) {
      cell_type* h = head.load();
      cell_type* orig = h;
      cell_type* next = tagged_tag_with<cell_type>(nullptr, finished_tag);
      if (compare_exchange(head, orig, next)) {
        return std::make_pair(start, h);
      }
    }
  }
  
  template <class Visit>
  static void finish_rng(cell_type* lo, cell_type* hi, const Visit& visit) {
    for (cell_type* it = lo; it != hi; it++) {
      if (multiadd) {
        Item x = try_finish_item_at(*it);
        if (x != nullptr) {
          visit(x);
        }
      } else {
        Item x = it->load();
        assert(x != nullptr);
        visit(x);
      }
    }
  }
  
  template <class Visit>
  void finish(const Visit& visit) {
    auto rng = finish_init();
    finish_rng(rng.first, rng.second, visit);
  }

};
  
template <class Item, int branching_factor, int block_capacity>
class node {
public:

  using block_type = block<Item, block_capacity, false>;
  
  block_type items;
    
  static_cache_aligned_array<std::atomic<node*>, branching_factor> children;

  node() {
    for (int i = 0; i < branching_factor; i++) {
      children[i].store(nullptr);
    }
  }
  
};

template <class Item, int branching_factor, int block_capacity>
class tree {
public:

  using node_type = node<Item, branching_factor, block_capacity>;

  std::atomic<node_type*> root;

  tree() {
    root.store(new node_type);
  }

  template <class Random_int>
  node_type* try_insert(const Random_int& random_int) {
    node_type* new_node = nullptr;
    std::atomic<node_type*>* current = &root;
    while (true) {
      node_type* target = current->load();
      if (target == nullptr) {
        if (new_node == nullptr) {
          new_node = new node_type;
        }
        node_type* orig = nullptr;
        if (compare_exchange(*current, orig, new_node)) {
          break;
        }
        target = current->load();
      }
      if (tagged_tag_of(target) == finished_tag) {
        if (new_node != nullptr) {
          delete new_node;
          new_node = nullptr;
        }
        break;
      }
      int i = random_int(0, branching_factor);
      current = &(target->children[i]);
    }
    return new_node;
  }
    
};
  
template <class Item, int branching_factor, int block_capacity>
class outset {
public:

  static constexpr int max_nb_procs = 64;

  using tree_type = tree<Item, branching_factor, block_capacity>;
  using node_type = typename tree_type::node_type;
  using item_cell_type = typename node_type::block_type::cell_type;
  using item_iterator = item_cell_type*;
  
private:
  
  using block_type = typename node_type::block_type;
  using shortcuts_type = static_cache_aligned_array<block_type*, max_nb_procs>;

  static constexpr int small_block_capacity = 16;
  using small_block_type = block<Item, small_block_capacity, true>;

  small_block_type items;
  
  tree_type root;
  
  std::atomic<shortcuts_type*> shortcuts;

public:
  
  outset() {
    shortcuts.store(nullptr);
  }
  
  ~outset() {
    shortcuts_type* s = shortcuts.load();
    shortcuts.store(nullptr);
    if (s != nullptr) {
      delete s;
    }
  }

  template <class Random_int>
  bool insert(Item x, int my_id, const Random_int& random_int) {
    if (! items.is_full()) {
      auto status = items.try_insert(x);
      if (status == items.failed_because_finish) {
        return false;
      } else if (status == items.succeeded) {
        return true;
      }
    }
    shortcuts_type* s = shortcuts.load();
    if (s == nullptr) {
      shortcuts_type* orig = nullptr;
      shortcuts_type* next = new shortcuts_type;
      for (int i = 0; i < next->size(); i++) {
        node_type* n = root.try_insert(random_int);
        if (n == nullptr) { // failed due to finish()
          delete next;
          return false;
        }
        (*next)[i] = &(n->items);
      }
      if (! compare_exchange(shortcuts, orig, next)) {
        delete next;
      }
      s = shortcuts.load();
    }  
    assert(s != nullptr); 
    while (true) {
      block_type* b = (*s)[my_id];
      auto status = b->try_insert(x);
      if (status == b->failed_because_finish) {
        return false;
      } else if (status == b->succeeded) {
        return true;
      }
      assert(status == b->failed_because_full);
      node_type* n = root.try_insert(random_int);
      if (n == nullptr) {
        return false;
      }
      (*s)[my_id] = &(n->items);
    }
  }
  
  node_type* get_root() {
    return tagged_pointer_of(root.root.load());
  }
  
  template <class Visit>
  node_type* finish_init(const Visit& visit) {
    items.finish(visit);
    while (true) {
      node_type* n = root.root.load();
      node_type* orig = n;
      node_type* next = tagged_tag_with(n, finished_tag);
      if (compare_exchange(root.root, orig, next)) {
        return n;
      }
    }
  }
  
  template <class Visit, class Deque>
  static void finish_nb(const std::size_t nb, item_iterator& lo, item_iterator& hi, Deque& todo, const Visit& visit) {
    int k = 0;
    while ( (k < nb) && ((! todo.empty()) || ((hi - lo) > 0)) ) {
      if ((hi - lo) > 0) {
        item_iterator lo_next = std::min(hi, lo + (nb - k));
        block_type::finish_rng(lo, lo_next, visit);
        k += lo_next - lo;
        lo = lo_next;
      } else {
        assert(! todo.empty());
        node_type* current = todo.back();
        todo.pop_back();
        for (int i = 0; i < branching_factor; i++) {
          while (true) {
            node_type* child = current->children[i].load();
            assert(tagged_tag_of(child) == 0);
            node_type* orig = child;
            node_type* next = tagged_tag_with(child, finished_tag);
            if (compare_exchange(current->children[i], orig, next)) {
              if (child != nullptr) {
                todo.push_back(child);
              }
              break;
            }
          }
        }
        auto rng = current->items.finish_init();
        lo = rng.first;
        hi = rng.second;
      }
    }
  }
  
  static void deallocate_nb(const int nb, std::deque<node_type*>& todo) {
    int k = 0;
    while ((k < nb) && (! todo.empty())) {
      node_type* current = todo.back();
      todo.pop_back();
      for (int i = 0; i < branching_factor; i++) {
        node_type* child = tagged_pointer_of(current->children[i].load());
        if (child != nullptr) {
          todo.push_back(child);
        }
      }
      delete current;
      k++;
    }
  }

};
  
} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/


#endif /*! _PASL_DATA_OUTSET_H_ */
