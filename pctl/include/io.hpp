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

#ifndef _PCTL_IO_H_
#define _PCTL_IO_H_

namespace pasl {
namespace pctl {

/***********************************************************************/

template <class Item>
std::ostream& operator<<(std::ostream& out, const parray<Item>& xs) {
  out << "{ ";
  size_t sz = xs.size();
  for (long i = 0; i < sz; i++) {
    out << xs[i];
    if (i+1 < sz)
      out << ", ";
  }
  out << " }";
  return out;
}
  
template <class Item>
std::ostream& operator<<(std::ostream& out, const pchunkedseq<Item>& xs) {
  out << "{ ";
  size_t sz = xs.seq.size();
  for (long i = 0; i < sz; i++) {
    out << xs.seq[i];
    if (i+1 < sz)
      out << ", ";
  }
  out << " }";
  return out;
}

/***********************************************************************/

} // end namespace
} // end namespace


#endif /*! _PCTL_IO_H_ */