/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file max.hpp
 * \brief Basic allocation and memory transfer operations
 *
 */

#include <limits>

#include "datapar.hpp"

#ifndef _PCTL_MAX_EXAMPLE_H_
#define _PCTL_MAX_EXAMPLE_H_

/***********************************************************************/

namespace pasl {
namespace pctl {
namespace max_ex {

int max(const parray<int>& xs) {
  int id = std::numeric_limits<int>::lowest();
  return reduce(xs.cbegin(), xs.cend(), id, [&] (int x, int y) {
    return std::max(x, y);
  });
}

int max0(const parray<parray<int>>& xss) {
  parray<int> id = { std::numeric_limits<int>::lowest() };
  auto weight = [&] (const parray<int>& xs) {
    return xs.size();
  };
  auto combine = [&] (const parray<int>& xs1,
                      const parray<int>& xs2) {
    parray<int> r = { std::max(max(xs1), max(xs2)) };
    return r;
  };
  parray<int> a =
    reduce(xss.cbegin(), xss.cend(), id, weight, combine);
  return a[0];
}

int max1(const parray<parray<int>>& xss) {
  auto lo = xss.cbegin();
  auto hi = xss.cend();
  int id = std::numeric_limits<int>::lowest();
  auto combine = [&] (int x, int y) {
    return std::max(x, y);
  };
  auto lift_comp = [&] (const parray<int>& xs) {
    return xs.size();
  };
  auto lift = [&] (const parray<int>& xs) {
    return max(xs);
  };
  return level1::reduce(lo, hi, id, combine, lift_comp, lift);
}

template <class Iter>
int max_seq(Iter lo, Iter hi) {
  int m = std::numeric_limits<int>::lowest();
  for (Iter i = lo; i != hi; i++) {
    const parray<int>& xs = *i;
    for (auto j = xs.cbegin(); j != xs.cend(); j++) {
      m = std::max(m, *j);
    }
  }
  return m;
}

int max2(const parray<parray<int>>& xss) {
  using const_iterator = typename parray<parray<int>>::const_iterator;
  const_iterator lo = xss.cbegin();
  const_iterator hi = xss.cend();
  int id = std::numeric_limits<int>::lowest();
  parray<long> w = weights(xss.size(), [&] (const parray<int>& xs) {
    return xs.size();
  });
  auto combine = [&] (int x, int y) {
    return std::max(x, y);
  };
  const_iterator b = lo;
  auto lift_comp_rng = [&] (const_iterator lo, const_iterator hi) {
    return w[hi - b] - w[lo - b];
  };
  auto lift_idx = [&] (int, const parray<int>& xs) {
    return max(xs);
  };
  auto seq_reduce_rng = [&] (const_iterator lo, const_iterator hi) {
    return max_seq(lo, hi);
  };
  return level2::reduce(lo, hi, id, combine, lift_comp_rng,
                                    lift_idx, seq_reduce_rng);
}
  
} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/


#endif /*! _PCTL_MAX_EXAMPLE_H_ */