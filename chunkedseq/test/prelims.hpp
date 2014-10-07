/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Data generation for randomize unit testing
 * \file prelims.hpp
 *
 */

#include <deque>
#include <vector>
#include <assert.h>
#include <map>

#include "atomic.hpp"
#include "chunkedseq.hpp"
#include "chunkedbag.hpp"
#include "trivbootchunkedseq.hpp"
#include "container.hpp"
#include "map.hpp"

#ifndef _PASL_DATA_TEST_PRELIMS_H_
#define _PASL_DATA_TEST_PRELIMS_H_

namespace pasl {
namespace data {
  
extern bool print_chunkedseq_verbose;
  
/*---------------------------------------------------------------------*/
/* Container pair */
  
template < class Key,
class T,
class Compare,
class Alloc
>
std::ostream& operator<<(std::ostream& out, const std::map<Key, T, Compare, Alloc>& xs);

template <class Key,
class Item,
class Compare,
class Key_swap,
class Alloc>
std::ostream& operator<<(std::ostream& out, const map::map<Key, Item, Compare, Key_swap, Alloc>& xs);

template <class Item>
std::ostream& operator<<(std::ostream& out, const pasl::data::stl::deque_seq<Item>& seq);
  
template <class T>
class default_container_same {
public:
  static bool same(const T& x, const T& y) {
    return x == y;
  }
};
  
template <class T>
class bag_container_same {
public:
  static bool same(const T& x, const T& y) {
    T xx(x);
    T yy(y);
    std::sort(xx.begin(), xx.end());
    std::sort(yy.begin(), yy.end());
    return xx == yy;
  }
};

// to be used to check consistency between two sequence containers
template <
  class Trusted_container, class Untrusted_container,
  class Untrusted_to_trusted,
  class Trusted_same=default_container_same<Trusted_container>
  >
class container_pair {
public:
  
  using trusted_container_type = Trusted_container;
  using untrusted_container_type = Untrusted_container;
  using trusted_same = Trusted_same;
  
  trusted_container_type trusted;
  untrusted_container_type untrusted;
  
  using self_type = container_pair<Trusted_container, Untrusted_container, Untrusted_to_trusted, Trusted_same>;
  
  container_pair() {
    assert(ok());
  }
  
  container_pair(const self_type& other)
  : trusted(other.trusted), untrusted(other.untrusted) {
    if (! ok()) {
      std::cout << trusted << std::endl;
      std::cout << Untrusted_to_trusted::conv(untrusted) << std::endl;
    }
    assert(ok());
  }
  
  static bool same(const Trusted_container& t, const Untrusted_container& u) {
    return Trusted_same::same(t, Untrusted_to_trusted::conv(u));
  }
  
  bool ok() const {
    bool size_same1 = trusted.size() == untrusted.size();
    bool size_same2 = trusted.size() == Untrusted_to_trusted::conv(untrusted).size();
    bool same1 = same(trusted, untrusted);
    untrusted.check();
    if (! size_same1)
      return false;
    if (! size_same2)
      return false;
    if (trusted.empty())
      assert(untrusted.empty());
    if (untrusted.empty())
      assert(trusted.empty());
    return same1;
  }
  
};
  
/*---------------------------------------------------------------------*/
/* Print routines */
  
static inline void print_dashes() {
  static constexpr int nb_dashes = 30;
  for (int i = 0; i < nb_dashes; i++)
    std::cout << "-";
  std::cout << std::endl;
}
  
template <class Item>
std::ostream& operator<<(std::ostream& out, const pasl::data::stl::deque_seq<Item>& seq) {
  return chunkedseq::extras::generic_print_container(out, seq);
}
  
template <class Item>
std::ostream& operator<<(std::ostream& out, const std::vector<Item>& seq) {
  out << "[";
  size_t sz = seq.size();
  for (auto it = seq.begin(); it != seq.end(); it++,sz--) {
    out << *it;
    if (sz != 1)
      out << ", ";
  }
  out << "]";
  return out;
}
  
template <class Item, int Chunk_capacity>
std::ostream& operator<<(std::ostream& out, const chunkedseq::bootchunkedseq::triv<Item, Chunk_capacity>& seq) {
  return generic_print_container(out, seq);
}
  
template < class Key,
class T,
class Compare,
class Alloc
>
std::ostream& operator<<(std::ostream& out, const std::map<Key, T, Compare, Alloc>& xs) {
  out << "[";
  size_t sz = xs.size();
  for (auto it = xs.begin(); it != xs.end(); it++, sz--) {
    auto v = *it;
    out << "(" << v.first << "," << v.second << ")";
    if (sz != 1)
      out << ",";
  }
  return out << "]";
}
  
template <class Key,
class Item,
class Compare,
class Key_swap,
class Alloc>
std::ostream& operator<<(std::ostream& out, const map::map<Key, Item, Compare, Key_swap, Alloc>& xs) {
  return xs.stream(out);
}
  
template <class T, class U, class C, class S>
std::ostream& operator<<(std::ostream& out, const container_pair<T, U, C, S>& cp) {
  out << std::endl;
  out << "trusted (sz= " << cp.trusted.size() << "):" << std::endl;
  out << cp.trusted << std::endl;
  out << "untrusted (sz= " << cp.untrusted.size() << "):" << std::endl;
  out << C::conv(cp.untrusted) << std::endl;
  out << C::conv(cp.untrusted).size() << std::endl;
//  out << cp.untrusted << std::endl;
  return out;
}
  
template <class T, class U, class C, class S>
bool check_and_print_container_pair(const container_pair<T, U, C, S>& cp, std::string msg = "") {
  if (cp.ok())
    return true;
  print_dashes();
  std::cout << "check on " << msg << " failed with:" << std::endl;
  std::cout << cp << std::endl;
  print_dashes();
  return false;
}
  
/*---------------------------------------------------------------------*/
/* Forward declarations */

template <class T, class U, class C, class S>
void generate(size_t& nb, container_pair<T, U, C, S>& dst);
  
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_TEST_PRELIMS_H_ */
