// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "dpsdatapar.hpp"
#include "geometry.hpp"
#include "geometryio.hpp"
#include "io.hpp"

#ifndef _PCTL_PBBS_HULL_H_
#define _PCTL_PBBS_HULL_H_

namespace pasl {
namespace pctl {

using intT = int;
using uintT = unsigned int;

template <class Item>
using iter = typename parray<Item>::iterator;

template <class Item>
using citer = typename parray<Item>::const_iterator;

template <class F1, class F2>
pair<intT,intT> split(iter<intT> A, intT n, F1 lf, F2 rf) {
  intT ll = 0, lm = 0;
  intT rm = n-1, rr = n-1;
  while (1) {
    while ((lm <= rm) && !(rf(A[lm]) > 0)) {
      if (lf(A[lm]) > 0) A[ll++] = A[lm];
      lm++;
    }
    while ((rm >= lm) && !(lf(A[rm]) > 0)) {
      if (rf(A[rm]) > 0) A[rr--] = A[rm];
      rm--;
    }
    if (lm >= rm) break;
    auto tmp = A[lm++];
    A[ll++] = A[rm--];
    A[rr--] = tmp;
  }
  intT n1 = ll;
  intT n2 = n-rr-1;
  return pair<intT,intT>(n1,n2);
}

intT seqQuickhull(iter<intT> I, point2d* P, intT n, intT l, intT r) {
  if (n < 2) return n;
  intT maxP = I[0];
  double maxArea = triArea(P[l],P[r],P[maxP]);
  for (intT i=1; i < n; i++) {
    intT j = I[i];
    double a = triArea(P[l],P[r],P[j]);
    if (a > maxArea) {
      maxArea = a;
      maxP = j;
    }
  }
  
  pair<intT,intT> nn = split(I, n, [&] (intT i) {
    return triArea(P[l], P[maxP], P[i]) > 0.0;
  }, [&] (intT i) {
    return triArea(P[maxP], P[r], P[i]) > 0.0;
  });
  intT n1 = nn.first;
  intT n2 = nn.second;
  
  intT m1, m2;
  m1 = seqQuickhull(I,      P, n1, l,   maxP);
  m2 = seqQuickhull(I+n-n2, P, n2, maxP,r);
  for (intT i=0; i < m2; i++) I[i+m1+1] = I[i+n-n2];
  I[m1] = maxP;
  return m1+1+m2;
}

controller_type quickhull_contr("quickhull");

intT quickHull(iter<intT> I, iter<intT> Itmp, iter<point2d> P, intT n, intT l, intT r) {
  intT result;
  par::cstmt(quickhull_contr, [&] { return n; }, [&] {
    if (n < 2) {
      result = seqQuickhull(I, P, n, l, r);
    } else {
      
      auto greater = [&] (double x, double y) {
        return x > y;
      };
      intT idx = (intT)max_index(I, I+n, (double)0.0, greater, [&] (intT, intT k) {
        return triArea(P[l], P[r], P[k]);
      });
      intT maxP = I[idx];
      
      intT n1 = (intT)dps::filter(I, I+n, Itmp, [&] (intT i) {
        return triArea(P[l], P[maxP], P[i]) > 0.0;
      });
      intT n2 = (intT)dps::filter(I, I+n, Itmp+n1, [&] (intT i) {
        return triArea(P[maxP], P[r], P[i]) > 0.0;
      });
      
      intT m1, m2;
      par::fork2([&] {
        m1 = quickHull(Itmp, I, P, n1, l, maxP);
      }, [&] {
        m2 = quickHull(Itmp+n1, I+n1, P, n2, maxP, r);
      });
      
      pmem::copy(Itmp, Itmp+m1, I);
      I[m1] = maxP;
      pmem::copy(Itmp+n1, Itmp+n1+m2, I+m1+1);
      result = m1+1+m2;
    }
  }, [&] {
    result = seqQuickhull(I, P, n, l, r);
  }); 
  return result;
}

parray<intT> hull(parray<point2d>& P) {
  intT n = (intT)P.size();
  auto combine = [&] (pair<intT,intT> l, pair<intT,intT> r) {
    intT minIndex =
    (P[l.first].x < P[r.first].x) ? l.first :
    (P[l.first].x > P[r.first].x) ? r.first :
    (P[l.first].y < P[r.first].y) ? l.first : r.first;
    intT maxIndex = (P[l.second].x > P[r.second].x) ? l.second : r.second;
    return make_pair(minIndex, maxIndex);
  };
  auto id = make_pair(0,0);
  auto minMax = level1::reducei(P.cbegin(), P.cend(), id, combine, [&] (long i, point2d) {
    return make_pair(i, i);
  });
  intT l = minMax.first;
  intT r = minMax.second;
  parray<bool> fTop(n);
  parray<bool> fBot(n);
  parray<intT> I(n);
  parray<intT> Itmp(n);
  parallel_for((intT)0, n, [&] (intT i) {
    Itmp[i] = i;
    double a = triArea(P[l],P[r],P[i]);
    fTop[i] = a > 0;
    fBot[i] = a < 0;
  });
  
  intT n1 = (intT)dps::pack(fTop.cbegin(), Itmp.cbegin(), Itmp.cend(), I.begin());
  intT n2 = (intT)dps::pack(fBot.cbegin(), Itmp.cbegin(), Itmp.cend(), I.begin()+n1);
  
  intT m1; intT m2;
  par::fork2([&] {
    m1 = quickHull(I.begin(), Itmp.begin(), P.begin(), n1, l, r);
  }, [&] {
    m2 = quickHull(I.begin()+n1, Itmp.begin()+n1, P.begin(), n2, r, l);
  });
  
  pmem::copy(I.cbegin(), I.cbegin()+m1, Itmp.begin()+1);
  pmem::copy(I.cbegin()+n1, I.cbegin()+n1+m2, Itmp.begin()+m1+2);
  
  Itmp[0] = l;
  Itmp[m1+1] = r;
  Itmp.resize(m1+2+m2);
  return Itmp;
}

} // end namespace
} // end namespace

#endif /*! _PCTL_PBBS_HULL_H_ */