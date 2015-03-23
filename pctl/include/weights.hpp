/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file weights.hpp
 * \brief The weights function
 *
 */

#include <limits.h>
#include <memory>
#include <utility>
#ifndef TARGET_MAC_OS
#include <malloc.h>
#endif
#include <algorithm>

#include "parray.hpp"

#ifndef _PCTL_WEIGHTS_H_
#define _PCTL_WEIGHTS_H_

namespace pasl {
namespace pctl {

/***********************************************************************/
  
namespace partial_sums {
  
controller_type contr("partial_sums");

long seq(const long* lo, const long* hi, long id, long* dst) {
  long x = id;
  for (const long* i = lo; i != hi; i++) {
    *dst = x;
    dst++;
    x += *i;
  }
  return x;
}
  
parray::parray<long> rec(const parray::parray<long>& xs) {
  const long k = 1024;
  long n = xs.size();
  long m = 1 + ((n - 1) / k);
  parray::parray<long> rs(n);
  par::cstmt(contr, [&] { return n; }, [&] {
    if (n <= k) {
      seq(xs.cbegin(), xs.cend(), 0, rs.begin());
    } else {
      parray::parray<long> sums(m);
      parallel_for(0l, m, [&] (long i) {
        long lo = i * k;
        long hi = std::min(lo + k, n);
        sums[i] = 0;
        for (long j = lo; j < hi; j++) {
          sums[i] += xs[j];
        }
      });
      parray::parray<long> scans = rec(sums);
      parallel_for(0l, m, [&] (long i) {
        long lo = i * k;
        long hi = std::min(lo + k, n);
        seq(xs.cbegin()+lo, xs.cbegin()+hi, scans[i], rs.begin()+lo);
      });
    }
  }, [&] {
    seq(xs.cbegin(), xs.cend(), 0, rs.begin());
  });
  return rs;
}
  
} // end namespace
  
template <class Weight>
long weights_seq(const Weight& weight, long lo, long hi, long id, long* dst) {
  long x = id;
  for (long i = lo; i != hi; i++) {
    *dst = x;
    dst++;
    x += weight(i);
  }
  return x;
}

namespace contr {
controller_type weights("weights");
}

template <class Weight>
parray::parray<long> weights(long n, const Weight& weight) {
  const long k = 1024;
  long m = 1 + ((n - 1) / k);
  long tot;
  parray::parray<long> rs(n + 1);
  par::cstmt(contr::weights, [&] { return n; }, [&] {
    if (n <= k) {
      tot = weights_seq(weight, 0, n, 0, rs.begin());
    } else {
      parray::parray<long> sums(m);
      parallel_for(0l, m, [&] (long i) {
        long lo = i * k;
        long hi = std::min(lo + k, n);
        sums[i] = 0;
        for (long j = lo; j < hi; j++) {
          sums[i] += weight(j);
        }
      });
      parray::parray<long> scans = partial_sums::rec(sums);
      parallel_for(0l, m, [&] (long i) {
        long lo = i * k;
        long hi = std::min(lo + k, n);
        weights_seq(weight, lo, hi, scans[i], rs.begin()+lo);
      });
      tot = rs[n-1] + weight(n-1);
    }
  }, [&] {
    tot = weights_seq(weight, 0, n, 0, rs.begin());
  });
  rs[n] = tot;
  return rs;
}

template <class Iter, class Body, class Comp>
void parallel_for(Iter lo, Iter hi, const Comp& comp, const Body& body) {
  parray::parray<long> w = weights(hi - lo, comp);
  auto comp_rng = [&] (long lo, long hi) {
    return w[hi] - w[lo];
  };
  range::parallel_for(lo, hi, comp_rng, body);
}

/***********************************************************************/

} // end namespace
} // end namespace


#endif /*! _PCTL_WEIGHTS_H_ */