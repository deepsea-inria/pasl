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

#include <algorithm>
#include "utils.hpp"
#include "geometry.hpp"
#include "hull.hpp"
using namespace std;

#ifndef _PCTL_PBBS_SEQHULL_H_
#define _PCTL_PBBS_SEQHULL_H_

namespace pasl {
namespace pctl {
namespace sequential {

template <class ET, class F>
pair<intT,intT> split(ET* A, intT n, F lf, F rf) {
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
    ET tmp = A[lm++];
    A[ll++] = A[rm--];
    A[rr--] = tmp;
  }
  intT n1 = ll;
  intT n2 = n-rr-1;
  return pair<intT,intT>(n1,n2);
}

struct aboveLine {
  intT l, r;
  point2d* P;
  aboveLine(point2d* _P, intT _l, intT _r) : P(_P), l(_l), r(_r) {}
  bool operator() (intT i) {return triArea(P[l], P[r], P[i]) > 0.0;}
};

intT serialQuickHull(intT* I, point2d* P, intT n, intT l, intT r) {
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
  
  pair<intT,intT> nn = split(I, n, aboveLine(P,l,maxP), aboveLine(P,maxP,r));
  intT n1 = nn.first;
  intT n2 = nn.second;
  
  intT m1, m2;
  m1 = serialQuickHull(I,      P, n1, l,   maxP);
  m2 = serialQuickHull(I+n-n2, P, n2, maxP,r);
  for (intT i=0; i < m2; i++) I[i+m1+1] = I[i+n-n2];
  I[m1] = maxP;
  return m1+1+m2;
}

pair<intT*,intT> hull(point2d* P, intT n) {
  intT* I = newA(intT, n);
  for (intT i=0; i < n; i++) I[i] = i;
  
  intT l = 0;
  intT r = 0;
  for (intT i=1; i < n; i++) {
    if (P[i].x > P[r].x) r = i;
    if (P[i].x < P[l].x || ((P[i].x == P[l].x) && P[i].y < P[l].y))
      l = i;
  }
  
  pair<intT,intT> nn = split(I, n, aboveLine(P, l, r), aboveLine(P, r, l));
  intT n1 = nn.first;
  intT n2 = nn.second;
  
  intT m1 = serialQuickHull(I, P, n1, l, r);
  intT m2 = serialQuickHull(I+n-n2, P, n2, r, l);
  
  for (intT i=m1; i > 0; i--) I[i] = I[i-1];
  for (intT i=0; i < m2; i++) I[i+m1+2] = I[i+n-n2];
  I[0] = l;
  I[m1+1] = r;
  return make_pair(I,m1+2+m2);
}
  
parray<intT> hull(parray<point2d>& points) {
  auto res = hull(points.begin(), points.size());
  parray<intT> idxs(res.first, res.first+res.second);
  free(res.first);
  return idxs;
}
  
} // end namespace
} // end namespace
} // end namespace


#endif /*! _PCTL_PBBS_SEQHULL_H_ */
