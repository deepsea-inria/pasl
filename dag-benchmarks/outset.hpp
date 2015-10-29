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
  
};
  
template <class Item>
class dynamic_cache_aligned_array {
private:
  
  static constexpr int cache_align_szb = 128;
  static constexpr int item_szb = sizeof(Item);
  
  using aligned_item_type = typename std::aligned_storage<item_szb, cache_align_szb>::type;
  
  aligned_item_type* items;
  std::size_t nb;
  
  Item& at(std::size_t i) {
    assert(i >= 0);
    assert(i < nb);
    return *reinterpret_cast<Item*>(items + i);
  }
  
public:
  
  dynamic_cache_aligned_array(std::size_t nb)
  : nb(nb) {
    std::size_t szb = nb * sizeof(aligned_item_type);
    items = (aligned_item_type*)malloc(szb);
    for (std::size_t i = 0; i < nb; i++) {
      new (&at(i)) Item();
    }
  }
  
  ~dynamic_cache_aligned_array() {
    for (std::size_t i = 0; i < nb; i++) {
      delete (&at(i));
    }
    free(items);
  }
  
  Item& operator[](std::size_t i) {
    return at(i);
  }
  
  std::size_t size() const {
    return nb;
  }
  
};
  
} // end namespace
  
template <class Item, int branching_factor>
class node {
public:
  
  static constexpr int finished_tag = 1;

  dynamic_cache_aligned_array<std::atomic<Item>> items;
  
  static_cache_aligned_array<std::atomic<node*>, branching_factor> children;

  node(std::size_t nb_items)
  : items(nb_items) { }
  
  std::size_t nb_items() const {
    return items.size();
  }
  
};
  
int nb_items_for_depth(int depth) {
  if (depth == 0) {
    return 4;
  } else if (depth == 1) {
    return 64;
  } else if (depth == 2) {
    return 512;
  } else {
    return 1024;
  }
}
  
template <class Item, int branching_factor, class Random_int>
bool insert(std::atomic<node<Item, branching_factor>*>& root,
            Item x,
            const Random_int& random_int) {
  using node_type = node<Item, branching_factor>;
  node_type* new_node = nullptr;
  std::atomic<node_type*>* current = &root;
  int depth = 0;
  auto delete_new_node_if_needed = [&] {
    if (new_node != nullptr) {
      delete new_node;
      new_node = nullptr;
    }
  };
  while (true) {
    node_type* target = current->load();
    if (target == nullptr) {
      if (new_node == nullptr) {
        int nb = nb_items_for_depth(depth);
        new_node = new node_type(nb);
        int i = random_int(0, nb);
        new_node->items[i].store(x);
      }
      node_type* orig = nullptr;
      if (compare_exchange(*current, orig, new_node)) {
        return true;
      }
      target = current->load();
    }
    if (tagged_tag_of(target) == node_type::finished_tag) {
      delete_new_node_if_needed();
      return false;
    }
    int i = random_int(0, target->nb_items());
    Item y = target->items[i].load();
    if (y == nullptr) {
      Item orig = nullptr;
      if (compare_exchange(target->items[i], orig, x)) {
        delete_new_node_if_needed();
        return true;
      }
    } else if (tagged_tag_of(y) == node_type::finished_tag) {
      delete_new_node_if_needed();
      return false;
    } else {
      int i = random_int(0, branching_factor);
      current = &(target->children[i]);
      depth++;
    }
  }
}
  
template <class Item, int branching_factor>
void finish_partial(std::atomic<node<Item, branching_factor>>* freelist,
                    std::deque<node<Item, branching_factor>*>& todo,
                    int max_nb_todo) {
  
}
  
template <class Item, int branching_factor>
void delete_partial(std::deque<node<Item, branching_factor>*>& todo,
                    int max_nb_todo) {
  
}
  
} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/


#endif /*! _PASL_DATA_OUTSET_H_ */
