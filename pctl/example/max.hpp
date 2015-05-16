/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file max.hpp
 * \brief Basic allocation and memory transfer operations
 *
 */

#include "pchunkedseq.hpp"
#include "parray.hpp"
#include "datapar.hpp"

#ifndef _PCTL_MAX_EXAMPLE_H_
#define _PCTL_MAX_EXAMPLE_H_

/***********************************************************************/

template <class Item>
using parray = pasl::pctl::parray<Item>;

long max(const parray<long>& xs) {
  return pasl::pctl::reduce(xs.cbegin(), xs.cend(), LONG_MIN, [&] (long x, long y) {
    return std::max(x, y);
  });
}

long max0(const parray<parray<long>>& xss) {
  parray<long> id = { LONG_MIN };
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
  auto lift_comp = [&] (iterator it_xs) {
    return it_xs->size();
  };
  auto lift = [&] (iterator it_xs) {
    return max(*it_xs);
  };
  auto lo = xss.cbegin();
  auto hi = xss.cend();
  return pasl::pctl::level1::reduce(lo, hi, 0, combine, lift_comp, lift);
}

template <class Iter>
long max_seq(Iter lo_xs, Iter hi_xs) {
  long m = LONG_MIN;
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
  auto lift = [&] (long, iterator it_xs) {
    return max(*it_xs);
  };
  auto seq_lift = [&] (iterator lo_xs, iterator hi_xs) {
    return max_seq(lo_xs, hi_xs);
  };
  iterator lo_xs = xss.cbegin();
  iterator hi_xs = xss.cend();
  return pasl::pctl::level2::reduce(lo_xs, hi_xs, 0, combine, lift_comp_rng, lift, seq_lift);
}

/***********************************************************************/


#endif /*! _PCTL_MAX_EXAMPLE_H_ */