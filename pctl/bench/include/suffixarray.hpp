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

// This is a parallel version of the algorithm described in
//  Juha Karkkainen and Peter Sanders.
//  Simple linear work suffix array construction.
//  Proc. ICALP 2003.  pp 943
// It includes code for finding the LCP
//   Written by Guy Blelloch and Julian Shun

#include <iostream>
#include "blockradixsort.hpp"
//#include "merge.hpp"
#include "psort.hpp"
#include "utils.hpp"
#include "rangemin.hpp"

#ifndef _PCTL_PBBS_SUFFIXARRAY_H_
#define _PCTL_PBBS_SUFFIXARRAY_H_

namespace pasl {
namespace pctl {
    
using namespace std;

bool isSorted(intT *SA, intT *s, intT n);

// Radix sort a pair of integers based on first element
template <class intT>
void radixSortPair(pair<intT,intT> *A, intT n, intT m) {
  intSort::iSort(A,n,m,[&] (std::pair<intT,intT>& x) { return x.first; });
}

inline bool leq(intT a1, intT a2,   intT b1, intT b2) {
  return(a1 < b1 || a1 == b1 && a2 <= b2);
}

inline bool leq(intT a1, intT a2, intT a3, intT b1, intT b2, intT b3) {
  return(a1 < b1 || a1 == b1 && leq(a2,a3, b2,b3));
}

struct compS {
  intT* _s;
  intT* _s12;
  compS(intT* s, intT* s12) : _s(s), _s12(s12) {}
  int operator () (intT i, intT j) {
    if (i%3 == 1 || j%3 == 1)
      return leq(_s[i],_s12[i+1], _s[j],_s12[j+1]);
    else
      return leq(_s[i],_s[i+1],_s12[i+2], _s[j],_s[j+1],_s12[j+2]);
  }
};

//struct mod3is1 { bool operator() (intT i) {return i%3 == 1;}};

inline intT computeLCP(intT* LCP12, intT* rank, myRMQ & RMQ,
                       intT j, intT k, intT* s, intT n){
  
  intT rank_j=rank[j]-2;
  intT rank_k=rank[k]-2;
  if(rank_j > rank_k) {swap(rank_j,rank_k);} //swap for RMQ query
  
  intT l = ((rank_j == rank_k-1) ? LCP12[rank_j]
            : LCP12[RMQ.query(rank_j,rank_k-1)]);
  
  intT lll = 3*l;
  if (s[j+lll] == s[k+lll]) {
    if (s[j+lll+1] == s[k+lll+1]) return lll + 2;
    else return lll + 1;
  }
  return lll;
}

// This recursive version requires s[n]=s[n+1]=s[n+2] = 0
// K is the maximum value of any element in s
void suffixArrayRec(intT* s, intT n, intT K, bool findLCPs,
                    parray<intT>& SA, parray<intT>& LCP) {
  n = n+1;
  intT n0=(n+2)/3, n1=(n+1)/3, n12=n-n0;
  intT bits = utils::logUp(K);
  parray<pair<intT,intT>> C(n12);
  
  // if 3 chars fit into an int then just do one radix sort
  if (bits < 11) {
    parallel_for((intT)0, n12, [&] (intT i) {
      intT j = 1+(i+i+i)/2;
      C[i].first = (s[j] << 2*bits) + (s[j+1] << bits) + s[j+2];
      C[i].second = j;
    });
    radixSortPair(C.begin(), n12, (intT) 1 << 3*bits);
    
    // otherwise do 3 radix sorts, one per char
  } else {
    parallel_for((intT)0, n12, [&] (intT i) {
      intT j = 1+(i+i+i)/2;
      C[i].first = s[j+2];
      C[i].second = j;
    });
    // radix sort based on 3 chars
    radixSortPair(C.begin(), n12, K);
    parallel_for((intT)0, n12, [&] (intT i) {
      C[i].first = s[C[i].second+1];
    });
    radixSortPair(C.begin(), n12, K);
    parallel_for((intT)0, n12, [&] (intT i) {
      C[i].first = s[C[i].second];
    });
    radixSortPair(C.begin(), n12, K);
  }
  
  // copy sorted results into sorted12
  parray<intT> sorted12(n12, [&] (intT i) {
    return C[i].second;
  });
  
  // generate names based on 3 chars
  parray<intT> name12(n12, [&] (intT i) {
    if (i == 0)
      return 1;
    else if (s[sorted12[i]]!=s[sorted12[i-1]]
        || s[sorted12[i]+1]!=s[sorted12[i-1]+1]
        || s[sorted12[i]+2]!=s[sorted12[i-1]+2])
      return 1;
    else return 0;
  });
  intT id = 0;
  dps::scan(name12.begin(), name12.end(), id, [&] (intT x, intT y) {
    return x + y;
  }, name12.begin(), forward_inclusive_scan);
  intT names = name12[n12-1];
  
  parray<intT> SA12;
  parray<intT> LCP12;
  // recurse if names are not yet unique
  if (names < n12) {
    parray<intT> s12(n12+3);
    s12[n12] = s12[n12+1] = s12[n12+2] = 0;
    
    // move mod 1 suffixes to bottom half and and mod 2 suffixes to top
    parallel_for((intT)0, n12, [&] (intT i) {
      if (sorted12[i]%3 == 1) s12[sorted12[i]/3] = name12[i];
      else s12[sorted12[i]/3+n1] = name12[i];
    });
    
    suffixArrayRec(s12.begin(), n12, names+1, findLCPs, SA12, LCP12);
    
    // restore proper indices into original array
    parallel_for((intT)0, n12, [&] (intT i) {
      intT l = SA12[i];
      SA12[i] = (l<n1) ? 3*l+1 : 3*(l-n1)+2;
    });
  } else {
    SA12.swap(sorted12); // suffix array is sorted array
    if (findLCPs) {
      LCP12.resize(n12+3, 0);
    }
  }
  
  // place ranks for the mod12 elements in full length array
  // mod0 locations of rank will contain garbage
  parray<intT> rank(n+2);
  rank[n]=1; rank[n+1] = 0;
  parallel_for((intT)0, n12, [&] (intT i) {
    rank[SA12[i]] = i+2;
  });
  
  // stably sort the mod 0 suffixes
  // uses the fact that we already have the tails sorted in SA12
  parray<intT> s0 = filter(SA12.cbegin(), SA12.cbegin()+n12, [&] (intT i) {
    return i%3 == 1;
  });
  intT x = (intT)s0.size();
  parray<pair<intT,intT>> D(n0);
  D[0].first = s[n-1]; D[0].second = n-1;
  parallel_for((intT)0, x, [&] (intT i) {
    D[i+n0-x].first = s[s0[i]-1];
    D[i+n0-x].second = s0[i]-1;
  });
  radixSortPair(D.begin(),n0, K);
//  intT* SA0  = s0.begin(); // reuse memory since not overlapping
  parray<intT> SA0(n0, [&] (intT i) {
    return D[i].second;
  });
  
  compS comp(s,rank.begin());
  intT o = (n%3 == 1) ? 1 : 0;
  SA.resize(n);
  auto SA0beg = SA0.begin()+o;
  auto SA12beg = SA12.begin()+1-o;
  merge(SA0beg, SA0beg+(n0-o), SA12beg, SA12beg+(n12+o-1), SA.begin(), comp);
  
  //get LCP from LCP12
  if(findLCPs){
    LCP.resize(n);
    LCP[n-1] = LCP[n-2] = 0;
    myRMQ RMQ(LCP12.begin(),n12+3); //simple rmq
    parallel_for((intT)0, n-2, [&] (intT i) {
      intT j=SA[i];
      intT k=SA[i+1];
      int CLEN = 16;
      intT ii;
      for (ii=0; ii < CLEN; ii++)
        if (s[j+ii] != s[k+ii]) break;
      if (ii != CLEN) LCP[i] = ii;
      else {
        if (j%3 != 0 && k%3 != 0)
          LCP[i] = computeLCP(LCP12.begin(), rank.begin(), RMQ, j, k, s, n);
        else if (j%3 != 2 && k%3 != 2)
          LCP[i] = 1 + computeLCP(LCP12.begin(), rank.begin(), RMQ, j+1, k+1, s, n);
        else
          LCP[i] = 2 + computeLCP(LCP12.begin(), rank.begin(), RMQ, j+2, k+2, s, n);
      }
    });
  }

}

void suffixArray(unsigned char* s, intT n, bool findLCPs,
                 parray<intT>& SA, parray<intT>& LCP) {
  parray<intT> ss(n+3);
  ss[n] = ss[n+1] = ss[n+2] = 0;
  parallel_for((intT)0, n, [&] (intT i) {
    ss[i] = s[i]+1;
  });
  intT k = 1 + reduce(ss.cbegin(), ss.cbegin()+n, ss[0], [&] (intT x, intT y) {
    return std::max(x, y);
  });
  
  suffixArrayRec(ss.begin(), n, k, findLCPs, SA, LCP);
}

parray<intT> suffixArray(unsigned char* s, intT n) {
  parray<intT> SA;
  parray<intT> LCP;
  suffixArray(s,n,false, SA, LCP);
  return SA;
}
    
} // end namespace
} // end namespace

#endif /*! _PCTL_PBBS_SUFFIXARRAY_H_ */
