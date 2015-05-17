/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file prandgen.hpp
 * \brief Parallel random container generator
 *
 */

#include "pchunkedseq.hpp"
#include "parray.hpp"

#ifndef _PCTL_PRANDGEN_H_
#define _PCTL_PRANDGEN_H_

/***********************************************************************/

namespace pasl {
namespace pctl {
namespace prandgen {
  
/*---------------------------------------------------------------------*/
/* Hash function */
  
inline unsigned int hashu(unsigned int a) {
  a = (a+0x7ed55d16) + (a<<12);
  a = (a^0xc761c23c) ^ (a>>19);
  a = (a+0x165667b1) + (a<<5);
  a = (a+0xd3a2646c) ^ (a<<9);
  a = (a+0xfd7046c5) + (a<<3);
  a = (a^0xb55a4f09) ^ (a>>16);
  return a;
}
  
#define HASH_MAX_INT ((unsigned) 1 << 31)

static inline int hashi(int i) {
  return hashu(i) & (HASH_MAX_INT-1);}

static inline double hashd(int i) {
  return ((double) hashi(i)/((double) HASH_MAX_INT));}

template <class T> T hash(int i) {
  if (typeid(T) == typeid(int)) {
    return hashi(i);
  } else if (typeid(T) == typeid(unsigned int)) {
    return hashu(i);
  } else if (typeid(T) == typeid(double)) {
    return hashd(i);
  } else {
    pasl::util::atomic::die("bogus");
    return 0;
  }
}
  
/*---------------------------------------------------------------------*/
/* General-purpose container generators */
  
template <class Item, class Generator>
parray<Item> gen_parray(long n, const Generator& g) {
  parray<Item> tmp(n, [&] (long i) {
    return g(i, hashu((unsigned int)i));
  });
  return tmp;
}
  
template <class Item, class Generator>
pchunkedseq<Item> gen_pchunkedseq(long n, const Generator& g) {
  pchunkedseq<Item> tmp(n, [&] (long i) {
    return g(i, hashu((unsigned int)i));
  });
  return tmp;
}

/*---------------------------------------------------------------------*/
/* Generators for containers of integral values */
  
template <class Integ>
Integ in_range(Integ val, Integ lo, Integ hi) {
  Integ n = hi - lo;
  Integ val2 = val % n;
  return val2 + lo;
}

template <class Integ>
parray<Integ> gen_integ_parray(long n, Integ lo, Integ hi) {
  return gen_parray<Integ>(n, [&] (long, int hash) {
    return in_range((Integ)hash, lo, hi);
  });
}

template <class Integ>
pchunkedseq<Integ> gen_integ_pchunkedseq(long n, Integ lo, Integ hi) {
  return gen_pchunkedseq<Integ>(n, [&] (long, int hash) {
    return in_range((Integ)hash, lo, hi);
  });
}
  
} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/


#endif /*! _PCTL_PRANDGEN_H_ */