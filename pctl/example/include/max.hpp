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

namespace max_ex {
  
template <class Item>
using parray = pasl::pctl::parray<Item>;

int max(const parray<int>& xs) {
  int id = std::numeric_limits<int>::lowest();
  return pasl::pctl::reduce(xs.cbegin(), xs.cend(), id, [&] (int x, int y) {
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
    pasl::pctl::reduce(xss.cbegin(), xss.cend(), id, weight, combine);
  return a[0];
}

int max1(const parray<parray<int>>& xss) {
  auto combine = [&] (int x, int y) {
    return std::max(x, y);
  };
  auto lift_comp = [&] (const parray<int>& xs) {
    return xs.size();
  };
  auto lift = [&] (const parray<int>& xs) {
    return max(xs);
  };
  auto lo = xss.cbegin();
  auto hi = xss.cend();
  int id = std::numeric_limits<int>::lowest();
  return pasl::pctl::level1::reduce(lo, hi, id, combine, lift_comp, lift);
}

template <class Iter>
int max_seq(Iter lo_xs, Iter hi_xs) {
  int m = std::numeric_limits<int>::lowest();
  for (Iter it_xs = lo_xs; it_xs != hi_xs; it_xs++) {
    const parray<int>& xs = *it_xs;
    for (auto it_x = xs.cbegin(); it_x != xs.cend(); it_x++) {
      m = std::max(m, *it_x);
    }
  }
  return m;
}

int max2(const parray<parray<int>>& xss) {
  using iterator = typename parray<parray<int>>::const_iterator;
  parray<long> w = pasl::pctl::weights(xss.size(), [&] (const parray<int>& xs) {
    return xs.size();
  });
  auto lift_comp_rng = [&] (iterator lo_xs, iterator hi_xs) {
    long lo = lo_xs - xss.cbegin();
    long hi = hi_xs - xss.cbegin();
    return w[hi] - w[lo];
  };
  auto combine = [&] (int x, int y) {
    return std::max(x, y);
  };
  auto lift = [&] (int, const parray<int>& xs) {
    return max(xs);
  };
  auto seq_reduce_rng = [&] (iterator lo_xs, iterator hi_xs) {
    return max_seq(lo_xs, hi_xs);
  };
  iterator lo_xs = xss.cbegin();
  iterator hi_xs = xss.cend();
  int id = std::numeric_limits<int>::lowest();
  return pasl::pctl::level2::reduce(lo_xs, hi_xs, id, combine, lift_comp_rng,
                                    lift, seq_reduce_rng);
}
  
} // end namespace

/***********************************************************************/


#endif /*! _PCTL_MAX_EXAMPLE_H_ */