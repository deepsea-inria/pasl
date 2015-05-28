/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file quickcheck.hpp
 * \brief Quickcheck initialization
 *
 */

#include "datapar.hpp"

#ifndef _PCTL_QUICKCHECK_H_
#define _PCTL_QUICKCHECK_H_

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Forward declarations */

namespace pasl {
namespace pctl {
  
template <class Container>
class container_wrapper {
public:
  Container c;
};

template <class Container>
std::ostream& operator<<(std::ostream& out, const container_wrapper<Container>& c);

template <class Container>
void generate(size_t nb, container_wrapper<Container>& c);
  
void generate(size_t, scan_type& st);
  
template <class Iter>
bool same_sequence(Iter xs_lo, Iter xs_hi, Iter ys_lo, Iter ys_hi) {
  if (xs_hi-xs_lo != ys_hi-ys_lo) {
    return false;
  }
  Iter ys_it = ys_lo;
  for (Iter xs_it = xs_lo; xs_it != xs_hi; xs_it++, ys_it++) {
    if (*xs_it != *ys_it) {
      return false;
    }
  }
  return  true;
}
  
template <class Iter>
bool same_set(Iter xs_lo, Iter xs_hi, Iter ys_lo, Iter ys_hi) {
  std::sort(xs_lo, xs_hi);
  std::sort(ys_lo, ys_hi);
  return same_sequence(xs_lo, xs_hi, ys_lo, ys_hi);
}
  
} // end namespace
} // end namespace

// This header must be included just after the forward declarations
#include "quickcheck.hh"

template <class Property>
void checkit(int nb_tests, std::string msg) {
  assert(nb_tests > 0);
  quickcheck::check<Property>(msg.c_str(), nb_tests);
}

/***********************************************************************/


#endif /*! _PCTL_QUICKCHECK_H_ */