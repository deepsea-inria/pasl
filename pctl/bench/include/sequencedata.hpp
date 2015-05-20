/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file sequencedata.hpp
 * \brief Parallel random container generator
 *
 */

#include "prandgen.hpp"
#include "utils.hpp"

#ifndef _PCTL_SEQUENCEDATA_H_
#define _PCTL_SEQUENCEDATA_H_

/***********************************************************************/

namespace pasl {
namespace pctl {
namespace sequencedata {
  
long hash(long i) {
  return (long)prandgen::hashu((unsigned int)i);
}
  
template <class Item>
parray<Item> rand(long s, long e) {
  return parray<Item>(e-s, [&] (long i) {
    return prandgen::hash<Item>(i+s);
  });
}
  
template <class intT>
parray<intT> rand_int_range(intT s, intT e, intT m) {
  return prandgen::gen_integ_parray(e-s, s, m);
}
  
template <class Item>
parray<Item> almost_sorted(long s, long e, long nb_swaps) {
  long n = e - s;
  parray<Item> result(n, [&] (long i) {
    return (Item)i;
  });
  for (long i = 0; i < nb_swaps; i++) {
    std::swap(result[hash(2*i)%n], result[hash(2*i+1)%n]);
  }
  return result;
}
  
template <class Item>
parray<Item> all_same(long n, Item x) {
  return parray<Item>(n, x);
}
  
template <class Item>
parray<Item> exp_dist(int s, int e) {
  int n = e - s;
  int lg = utils::log2Up(n)+1;
  return parray<Item>(n, [&] (long i) {
    int range = (1 << (hash(2*(i+s))%lg));
    return prandgen::hash<Item>((int)(range + hash(2*(i+s)+1)%range));
  });
}
  
} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/


#endif /*! _PCTL_SEQUENCEDATA_H_ */