/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file io.hpp
 * \brief Basic allocation and memory transfer operations
 *
 */

#include "pchunkedseq.hpp"
#include "parray.hpp"
#include "pset.hpp"
#include "pmap.hpp"

#ifndef _PCTL_IO_H_
#define _PCTL_IO_H_

namespace pasl {
namespace pctl {

/***********************************************************************/

template <class Item>
std::ostream& operator<<(std::ostream& out, const parray<Item>& xs) {
  using size_type = typename parray<Item>::size_type;
  out << "{ ";
  size_type sz = xs.size();
  for (size_type i = 0; i < sz; i++) {
    out << xs[i];
    if (i+1 < sz)
      out << ", ";
  }
  out << " }";
  return out;
}
  
template <class Item, class Weight>
std::ostream& operator<<(std::ostream& out, const weighted::parray<Item, Weight>& xs) {
  using size_type = typename weighted::parray<Item>::size_type;
  out << "{ ";
  size_type sz = xs.size();
  for (size_type i = 0; i < sz; i++) {
    out << xs[i];
    if (i+1 < sz)
      out << ", ";
  }
  out << " }";
  return out;
}
  
template <class Item>
std::ostream& operator<<(std::ostream& out, const pchunkedseq<Item>& xs) {
  using size_type = typename pchunkedseq<Item>::size_type;
  out << "{ ";
  size_type sz = xs.seq.size();
  for (size_type i = 0; i < sz; i++) {
    out << xs.seq[i];
    if (i+1 < sz)
      out << ", ";
  }
  out << " }";
  return out;
}

template <class Item>
std::ostream& operator<<(std::ostream& out, const pset<Item>& xs) {
  out << "{ ";
  for (auto it = xs.cbegin(); it != xs.cend(); it++) {
    Item x = *it;
    out << x;
    if (it+1 != xs.cend())
      out << ", ";
  }
  out << " }";
  return out;
}
  
template <class Key, class Item>
std::ostream& operator<<(std::ostream& out, const pmap<Key, Item>& xs) {
  out << "{ ";
  for (auto it = xs.cbegin(); it != xs.cend(); it++) {
    Key k = (*it).first;
    Item x = (*it).second;
    out << "std::make_pair(" << k << ", " << x << ")";
    if (it+1 != xs.cend())
      out << ", ";
  }
  out << " }";
  return out;
}

/***********************************************************************/

} // end namespace
} // end namespace


#endif /*! _PCTL_IO_H_ */