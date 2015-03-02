/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file weights.hpp
 * \brief
 *
 */

#include <limits.h>
#include <memory>
#include <utility>
#ifndef TARGET_MAC_OS
#include <malloc.h>
#endif
#include <algorithm>

#include "parraybase.hpp"

#ifndef _PARRAY_PCTL_WEIGHTS_H_
#define _PARRAY_PCTL_WEIGHTS_H_

namespace pasl {
namespace pctl {

/***********************************************************************/
  
controller_type long_scan_contr("long_scan");

long long_scan_seq(const long* lo, const long* hi, long id, long* dst) {
  long x = id;
  for (const long* i = lo; i != hi; i++) {
    *dst = x;
    dst++;
    x += *i;
  }
  return x;
}

parray::parray<long> long_scan_rec(const parray::parray<long>& xs) {
  const long k = 1024;
  long n = xs.size();
  long m = 1 + ((n - 1) / k);
  parray::parray<long> rs(n);
  par::cstmt(long_scan_contr, [&] { return n; }, [&] {
    if (n <= k) {
      long_scan_seq(xs.cbegin(), xs.cend(), 0, rs.begin());
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
      parray::parray<long> scans = long_scan_rec(sums);
      parallel_for(0l, m, [&] (long i) {
        long lo = i * k;
        long hi = std::min(lo + k, n);
        long_scan_seq(xs.cbegin()+lo, xs.cbegin()+hi, scans[i], rs.begin()+lo);
      });
    }
  }, [&] {
    long_scan_seq(xs.cbegin(), xs.cend(), 0, rs.begin());
  });
  return rs;
}

template <class Weight>
long long_scan_seq(const Weight& weight, long lo, long hi, long id, long* dst) {
  long x = id;
  for (long i = lo; i != hi; i++) {
    *dst = x;
    dst++;
    x += weight(i);
  }
  return x;
}
  
controller_type weights_contr("weights");

template <class Weight>
parray::parray<long> weights(long n, const Weight& weight) {
  const long k = 1024;
  long m = 1 + ((n - 1) / k);
  long tot;
  parray::parray<long> rs(n + 1);
  par::cstmt(weights_contr, [&] { return n; }, [&] {
    if (n <= k) {
      tot = long_scan_seq(weight, 0, n, 0, rs.begin());
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
      parray::parray<long> scans = long_scan_rec(sums);
      parallel_for(0l, m, [&] (long i) {
        long lo = i * k;
        long hi = std::min(lo + k, n);
        long_scan_seq(weight, lo, hi, scans[i], rs.begin()+lo);
      });
      tot = rs[n-1] + weight(n-1);
    }
  }, [&] {
    tot = long_scan_seq(weight, 0, n, 0, rs.begin());
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


#endif /*! _PARRAY_PCTL_WEIGHTS_H_ */