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

long max(const parray<long>& xs) {
  long id = std::numeric_limits<long>::lowest();
  return pasl::pctl::reduce(xs.cbegin(), xs.cend(), id, [&] (long x, long y) {
    return std::max(x, y);
  });
}

long max0(const parray<parray<long>>& xss) {
  parray<long> id = { std::numeric_limits<long>::lowest() };
  auto weight = [&] (const parray<long>& xs) {
    return xs.size();
  };
  auto combine = [&] (const parray<long>& xs1,
                      const parray<long>& xs2) {
    parray<long> r = { std::max(max(xs1), max(xs2)) };
    return r;
  };
  parray<long> a =
    pasl::pctl::reduce(xss.cbegin(), xss.cend(), id, weight, combine);
  return a[0];
}

long max1(const parray<parray<long>>& xss) {
  using iterator = typename parray<parray<long>>::const_iterator;
  auto combine = [&] (long x, long y) {
    return std::max(x, y);
  };
  auto lift_comp = [&] (const parray<long>& xs) {
    return xs.size();
  };
  auto lift = [&] (const parray<long>& xs) {
    return max(xs);
  };
  auto lo = xss.cbegin();
  auto hi = xss.cend();
  long id = std::numeric_limits<long>::lowest();
  return pasl::pctl::level1::reduce(lo, hi, id, combine, lift_comp, lift);
}

template <class Iter>
long max_seq(Iter lo_xs, Iter hi_xs) {
  long m = std::numeric_limits<long>::lowest();
  for (Iter it_xs = lo_xs; it_xs != hi_xs; it_xs++) {
    const parray<long>& xs = *it_xs;
    for (auto it_x = xs.cbegin(); it_x != xs.cend(); it_x++) {
      m = std::max(m, *it_x);
    }
  }
  return m;
}

long max2(const parray<parray<long>>& xss) {
  using iterator = typename parray<parray<long>>::const_iterator;
  parray<long> w = pasl::pctl::weights(xss.size(), [&] (const parray<long>& xs) {
    return xs.size();
  });
  auto lift_comp_rng = [&] (iterator lo_xs, iterator hi_xs) {
    long lo = lo_xs - xss.cbegin();
    long hi = hi_xs - xss.cbegin();
    return w[hi] - w[lo];
  };
  auto combine = [&] (long x, long y) {
    return std::max(x, y);
  };
  auto lift = [&] (long, const parray<long>& xs) {
    return max(xs);
  };
  auto seq_reduce_rng = [&] (iterator lo_xs, iterator hi_xs) {
    return max_seq(lo_xs, hi_xs);
  };
  iterator lo_xs = xss.cbegin();
  iterator hi_xs = xss.cend();
  long id = std::numeric_limits<long>::lowest();
  return pasl::pctl::level2::reduce(lo_xs, hi_xs, id, combine, lift_comp_rng,
                                    lift, seq_reduce_rng);
}
  
} // end namespace

/***********************************************************************/


#endif /*! _PCTL_MAX_EXAMPLE_H_ */