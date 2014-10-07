/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Data generation for randomize unit testing
 * \file generators.hpp
 *
 */

#include "prelims.hpp" // must be loaded before quickcheck.hh
#include "quickcheck.hh"

#ifndef _PASL_DATA_TEST_GENERATORS_H_
#define _PASL_DATA_TEST_GENERATORS_H_

namespace pasl {
namespace data {
  
/* NOTE:
 * if you want your generator function to be seen by the quickcheck
 * framework, you must put a forward reference of the function first 
 * into prelims.hpp so that the forward reference appears before 
 * quickcheck.hh is included.
 */
  
template <class Item>
Item generate_value() {
  return quickcheck::generateInRange(Item(1), Item(1<<8));
}
  
static inline bool flip_coin() {
  bool b;
  quickcheck::generate(1, b);
  return b;
}
  
template <class T, class U, class C, class S>
void random_push_pop_sequence(size_t nb_items, container_pair<T, U, C, S>& dst) {
  using value_type = typename T::value_type;
  for (size_t i = 0; i < nb_items; i++) {
    value_type v = generate_value<value_type>();
    if (flip_coin()) {
      dst.trusted.push_back(v);
      dst.untrusted.push_back(v);
    } else {
      dst.trusted.push_front(v);
      dst.untrusted.push_front(v);
    }
  }
  assert(dst.ok());
}
  
template <class T, class U, class C>
void random_push_pop_sequence(size_t nb_items, container_pair<T, U, C, bag_container_same<T>>& dst) {
  using value_type = typename T::value_type;
  for (size_t i = 0; i < nb_items; i++) {
    value_type v = generate_value<value_type>();
    dst.trusted.push_back(v);
    dst.untrusted.push_back(v);
  }
  assert(dst.ok());
}
  
template <class T, class U, class C, class S>
void random_insert_sequence(size_t nb, container_pair<T, U, C, S>& dst) {
  using value_type = typename T::value_type;
  for (size_t i = 0; i < nb; i++) {
    int sz = (int)dst.trusted.size();
    int pos = (sz == 0) ? 0 : quickcheck::generateInRange(0, sz-1);
    value_type v;
    quickcheck::generate(1<<15, v);
    dst.trusted.insert(dst.trusted.begin() + pos, v);
    dst.untrusted.insert(dst.untrusted.begin() + pos, v);
  }
}
  
extern bool generate_by_insert;
  
template <class Item, class U, class C, class S>
void generate(size_t& nb, container_pair<pasl::data::stl::deque_seq<Item>, U, C, S>& dst) {
  if (generate_by_insert)
    random_insert_sequence(nb, dst);
  else
    random_push_pop_sequence(nb, dst);
}
  
template <class Key, class T, class Compare, class Alloc, class U, class C, class S>
void generate(size_t& nb, container_pair<std::map<Key,T,Compare,Alloc>, U, C, S>& dst) {
  for (size_t i = 0; i < nb; i++) {
    Key key = quickcheck::generateInRange(0, Key(1<<18));
    T val = generate_value<T>();
    dst.trusted[key] = val;
    dst.untrusted[key] = val;
  }
}
  
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_TEST_GENERATORS_H_ */