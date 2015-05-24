// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2010 Guy Blelloch and Harsha Vardhan Simhadri and the PBBS team
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

#include <iostream>
#include <algorithm>
#include "utils.hpp"
#include <math.h>
#include "quicksort.hpp"
#include "transpose.hpp"
#include "prandgen.hpp"

namespace pasl {
namespace pctl {
  
template<class E, class BinPred, class intT>
void mergeSeq (E* sA, E* sB, intT* sC, intT lA, intT lB, BinPred f) {
  if (lA==0 || lB==0) return;
  E *eA = sA+lA;
  E *eB = sB+lB;
  for (intT i=0; i <= lB; i++) sC[i] = 0;
  while(1) {
    while (f(*sA, *sB)) {(*sC)++; if (++sA == eA) return;}
    sB++; sC++;
    if (sB == eB) break;
    if (!(f(*(sB-1),*sB))) {
      while (!f(*sB, *sA)) {(*sC)++; if (++sA == eA) return;}
      sB++; sC++;
      if (sB == eB) break;
    }
  }
  *sC = eA-sA;
}

#define SSORT_THR 128
#define AVG_SEG_SIZE 2
#define PIVOT_QUOT 2

template <class E, class BinPred, class intT>
class samplesort_contr {
public:
  static controller_type contr;
};

template <class E, class BinPred, class intT>
controller_type samplesort_contr<E,BinPred,intT>::contr("samplesort");

template<class E, class BinPred, class intT>
void sampleSort (E* A, intT n, BinPred f) {
  using controller_type = samplesort_contr<E,BinPred,intT>;
  par::cstmt(controller_type::contr, [&] { return n; }, [&] {
    if (n < SSORT_THR) compSort(A, n, f);
    else {
      intT sq = (intT)(pow(n,0.5));
      intT rowSize = sq*AVG_SEG_SIZE;
      intT numR = (intT)ceil(((double)n)/((double)rowSize));
      intT numSegs = (sq-1)/PIVOT_QUOT;
      int overSample = 4;
      intT sampleSetSize = numSegs*overSample;
      // generate samples with oversampling
      parray<E> sampleSet(sampleSetSize, [&] (intT j) {
        intT o = prandgen::hashi(j)%n;
        return A[o];
      });
      //cout << "n=" << n << " num_segs=" << numSegs << endl;
      
      // sort the samples
      compSort(sampleSet.begin(), sampleSetSize, f);
      
      // subselect samples at even stride
      parray<E> pivots(numSegs-1, [&] (intT k) {
        intT o = overSample*k;
        return sampleSet[o];
      });
      //nextTime("samples");
      
      parray<E> B(numR*rowSize);
      parray<intT> segSizes(numR*numSegs);
      parray<intT> offsetA(numR*numSegs);
      parray<intT> offsetB(numR*numSegs);
      
      // sort each row and merge with samples to get counts
      range::parallel_for((intT)0, numR, [&] (intT lo, intT hi) { return (hi-lo)*rowSize; }, [&] (intT r) {
        intT offset = r * rowSize;
        intT size =  (r < numR - 1) ? rowSize : n - offset;
        sampleSort(A+offset, size, f);
        mergeSeq(A + offset, pivots.begin(), segSizes.begin() + r*numSegs, size, numSegs-1, f);
      });
      //nextTime("sort and merge");
      
      // transpose from rows to columns
      auto plus = [&] (intT x, intT y) {
        return x + y;
      };
      dps::scan(segSizes.begin(), segSizes.end(), (intT)0, plus, offsetA.begin(), forward_exclusive_scan);
      transpose(segSizes.begin(), offsetB.begin(), numR, numSegs);
      dps::scan(offsetB.begin(), offsetB.end(), (intT)0, plus, offsetB.begin(), forward_exclusive_scan);
      block_transpose(A, B.begin(), offsetA.begin(), offsetB.begin(), segSizes.begin(), numR, numSegs);
      pmem::copy(B.begin(), B.begin()+n, A);
      //nextTime("transpose");
      
      // sort the columns
      auto complexity_fct = [&] (intT lo, intT hi) {
        if (lo == hi) {
          return 0;
        } else if (hi < numSegs-1) {
          return offsetB[hi*numR] - offsetB[lo*numR];
        } else {
          return n - offsetB[lo*numR];
        }
      };
      range::parallel_for((intT)0, numSegs, complexity_fct, [&] (intT i) {
        intT offset = offsetB[i*numR];
        if (i == 0) {
          sampleSort(A, offsetB[numR], f); // first segment
        } else if (i < numSegs-1) { // middle segments
          // if not all equal in the segment
          if (f(pivots[i-1],pivots[i]))
            sampleSort(A+offset, offsetB[(i+1)*numR] - offset, f);
        } else { // last segment
          sampleSort(A+offset, n - offset, f);
        }
      });
      //nextTime("last sort");
    }
  }, [&] {
    compSort(A, n, f);
  });
}
  
} // end namespace
} // end namespace

#undef compSort
#define compSort(__A, __n, __f) (sampleSort(__A, __n, __f))

