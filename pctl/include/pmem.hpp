/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file pmem.hpp
 * \brief Basic allocation and memory transfer operations
 *
 */

#include <limits.h>
#include <memory>
#include <utility>
#ifndef TARGET_MAC_OS
#include <malloc.h>
#endif
#include <algorithm>

#include "ploop.hpp"

#ifndef _PARRAY_PCTL_PRIM_H_
#define _PARRAY_PCTL_PRIM_H_

/***********************************************************************/

namespace pasl {
namespace pctl {
namespace pmem {
  
/*---------------------------------------------------------------------*/
/* Primitive memory operations */

template <class Iter, class Item>
void fill(Iter lo, Iter hi, const Item& val) {
  range::parallel_for(lo, hi, [&] (Iter lo, Iter hi) { return hi - lo; }, [&] (Iter i) {
    std::fill(i, i+1, val);
  }, [&] (Iter lo, Iter hi) {
    std::fill(lo, hi, val);
  });
}
  
template <class Iter, class Output_iterator>
void copy(Iter lo, Iter hi, Output_iterator dst) {
  range::parallel_for(lo, hi, [&] (Iter lo, Iter hi) { return hi - lo; }, [&] (Iter i) {
    std::copy(i, i + 1, dst + (i - lo));
  }, [&] (Iter lo2, Iter hi2) {
    std::copy(lo2, hi2, dst + (lo2 - lo));
  });
}

template <class Item, class Alloc>
void pdelete(Item* lo, Item* hi) {
  parallel_for(lo, hi, [&] (Item* p) {
    Alloc alloc;
    alloc.destroy(p);
  });
}
  
/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace


#endif /*! _PARRAY_PCTL_PRIM_H_ */