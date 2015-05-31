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

#include "utils.hpp"
#include "dpsdatapar.hpp"

#include <limits.h>

#if defined(LONG)
typedef long intT;
typedef unsigned long uintT;
#define INT_T_MAX LONG_MAX
#else
typedef int intT;
typedef unsigned int uintT;
#define INT_T_MAX INT_MAX
#endif

#ifndef SPECULATIVE_FOR_H_
#define SPECULATIVE_FOR_H_

namespace pasl {
namespace pctl {
  
using intT = int;

struct reservation {
  intT r;
  reservation() : r(INT_T_MAX) {}
  void reserve(intT i) { utils::writeMin(&r, i); }
  bool reserved() { return (r < INT_T_MAX);}
  void reset() {r = INT_T_MAX;}
  bool check(intT i) { return (r == i);}
  bool checkReset(intT i) {
    if (r==i) { r = INT_T_MAX; return 1;}
    else return 0;
  }
};

inline void reserveLoc(intT& x, intT i) {utils::writeMin(&x,i);}

template <class S>
intT speculative_for(S step, intT s, intT e, int granularity,
                     bool hasState=1, int maxTries=-1) {
  if (maxTries < 0) maxTries = 100 + 200*granularity;
  intT maxRoundSize = (e-s)/granularity+1;
  parray<intT> I(maxRoundSize);
  parray<intT> Ihold(maxRoundSize);
  parray<bool> keep(maxRoundSize);
  parray<S> state;
  if (hasState) {
    state.resize(maxRoundSize, step);
  }
  
  int round = 0;
  intT numberDone = s; // number of iterations done
  intT numberKeep = 0; // number of iterations to carry to next round
  intT totalProcessed = 0;
  
  while (numberDone < e) {
    //cout << "numberDone=" << numberDone << endl;
    if (round++ > maxTries) {
      std::cout << "speculativeLoop: too many iterations, increase maxTries parameter" << std::endl;
      abort();
    }
    intT size = std::min(maxRoundSize, e - numberDone);
    totalProcessed += size;
    
    if (hasState) {
      parallel_for((intT)0, size, [&] (intT i) {
        if (i >= numberKeep) I[i] = numberDone + i;
        keep[i] = state[i].reserve(I[i]);
      });
    } else {
      parallel_for((intT)0, size, [&] (intT i) {
        if (i >= numberKeep) I[i] = numberDone + i;
        keep[i] = step.reserve(I[i]);
      });
    }
    
    if (hasState) {
      parallel_for((intT)0, size, [&] (intT i) {
        if (keep[i]) keep[i] = !state[i].commit(I[i]);
      });
    } else {
      parallel_for((intT)0, size, [&] (intT i) {
        if (keep[i]) keep[i] = !step.commit(I[i]);
      });
    }
    
    // keep edges that failed to hook for next round
    numberKeep = (intT)dps::pack(keep.begin(), I.begin(), I.begin()+size, Ihold.begin());
    I.swap(Ihold);
    numberDone += size - numberKeep;
  }
  return totalProcessed;
}
  
} // end namespace
} // end namespace

#endif /*! SPECULATIVE_FOR_H_ */
