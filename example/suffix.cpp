#include <iostream>
#include <algorithm>
#include "randperm.hpp"
#include "sequenceio.hpp"
#include "samplesort.hpp"
#include "sequencedata.hpp"
#include "benchmark.hpp"
#include "blockradixsort.hpp"
#include "rangemin.hpp"
#include "merge.hpp"
#include "myint.hpp"
#include "sequence.hpp"
#include "io.hpp"

namespace pbbs {

  using namespace std;

  bool isSorted(intT *SA, intT *s, intT n);

// Radix sort a pair of integers based on first element
template <class intT>
void radixSortPair(pair<intT,intT> *A, intT n, intT m) {
  intSort::iSort(A,n,m,utils::firstF<intT,intT>());
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

struct mod3is1 { bool operator() (intT i) {return i%3 == 1;}};

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
pair<intT*,intT*> suffixArrayRec(intT* s, intT n, intT K, bool findLCPs) {
  n = n+1;
  intT n0=(n+2)/3, n1=(n+1)/3, n12=n-n0;
  pair<intT,intT> *C = (pair<intT,intT> *) malloc(n12*sizeof(pair<intT,intT>));

  intT bits = utils::logUp(K);
  // if 3 chars fit into an int then just do one radix sort
  if (bits < 11) {
    native::parallel_for((intT)0, n12, [&] (intT i) {
      intT j = 1+(i+i+i)/2;
      C[i].first = (s[j] << 2*bits) + (s[j+1] << bits) + s[j+2];
      C[i].second = j;});
    //radixTime.start();
    radixSortPair(C, n12, (intT) 1 << 3*bits);
    //radixTime.stop();

  // otherwise do 3 radix sorts, one per char
  } else {
    native::parallel_for((intT)0, n12, [&] (intT i) {
      intT j = 1+(i+i+i)/2;
      C[i].first = s[j+2]; 
      C[i].second = j;});
    //radixTime.start();
    // radix sort based on 3 chars
    radixSortPair(C, n12, K);
    native::parallel_for((intT)0, n12, [&] (intT i) {
        C[i].first = s[C[i].second+1];
      });
    radixSortPair(C, n12, K);
    native::parallel_for((intT)0, n12, [&] (intT i) {
        C[i].first = s[C[i].second];
      });
    radixSortPair(C, n12, K);
    //radixTime.stop();
  }

  // copy sorted results into sorted12
  intT* sorted12 = newA(intT,n12);
  native::parallel_for((intT)0, n12, [&] (intT i) {
      sorted12[i] = C[i].second;
    });
  free(C);

  // generate names based on 3 chars
  intT* name12 = newA(intT,n12);
  native::parallel_for((intT)1, n12, [&] (intT i) {
    if (s[sorted12[i]]!=s[sorted12[i-1]] 
	|| s[sorted12[i]+1]!=s[sorted12[i-1]+1] 
	|| s[sorted12[i]+2]!=s[sorted12[i-1]+2]) 
      name12[i] = 1;
    else name12[i] = 0;
  });
  name12[0] = 1;
  sequence::scanI(name12,name12,n12,utils::addF<intT>(),(intT)0);
  intT names = name12[n12-1];
  
  pair<intT*,intT*> SA12_LCP;
  intT* SA12;
  intT* LCP12 = NULL;
  // recurse if names are not yet unique
  if (names < n12) {
    intT* s12  = newA(intT,n12 + 3);  
    s12[n12] = s12[n12+1] = s12[n12+2] = 0;

    // move mod 1 suffixes to bottom half and and mod 2 suffixes to top
    native::parallel_for((intT)0, n12, [&] (intT i) {
      if (sorted12[i]%3 == 1) s12[sorted12[i]/3] = name12[i];
      else s12[sorted12[i]/3+n1] = name12[i];
      });
    free(name12);  free(sorted12);
    //for (int i=0; i < n12; i++) cout << s12[i] << " : ";
    //cout << endl;

    SA12_LCP = suffixArrayRec(s12, n12, names+1, findLCPs); 
    SA12 = SA12_LCP.first;
    LCP12 = SA12_LCP.second;
    free(s12);

    // restore proper indices into original array
    native::parallel_for((intT)0, n12, [&] (intT i) {
      intT l = SA12[i]; 
      SA12[i] = (l<n1) ? 3*l+1 : 3*(l-n1)+2;
      });
  } else {
    free(name12); // names not needed if we don't recurse
    SA12 = sorted12; // suffix array is sorted array
    if (findLCPs) {
      LCP12 = newA(intT,n12+3);
      native::parallel_for((intT)0, n12+3, [&] (intT i) {
	LCP12[i]=0; //LCP's are all 0 if not recursing
        });
    }
  }

  // place ranks for the mod12 elements in full length array
  // mod0 locations of rank will contain garbage
  intT* rank  = newA(intT,n + 2);  
  rank[n]=1; rank[n+1] = 0;
  native::parallel_for((intT)0, n12, [&] (intT i) {
      rank[SA12[i]] = i+2;
    });

  
  // stably sort the mod 0 suffixes 
  // uses the fact that we already have the tails sorted in SA12
  intT* s0  = newA(intT,n0);
  intT x = sequence::filter(SA12,s0,n12,mod3is1());
  pair<intT,intT> *D = (pair<intT,intT> *) malloc(n0*sizeof(pair<intT,intT>));
  D[0].first = s[n-1]; D[0].second = n-1;
  native::parallel_for((intT)0, x, [&] (intT i) {
    D[i+n0-x].first = s[s0[i]-1]; 
    D[i+n0-x].second = s0[i]-1;
    });
  //radixTime.start();
  radixSortPair(D,n0, K);
  //radixTime.stop();
  intT* SA0  = s0; // reuse memory since not overlapping
  native::parallel_for((intT)0, n0, [&] (intT i) {
      SA0[i] = D[i].second;
    });
  free(D);

  compS comp(s,rank);
  intT o = (n%3 == 1) ? 1 : 0;
  intT *SA = newA(intT,n); 
  //mergeTime.start();
  merge(SA0+o,n0-o,SA12+1-o,n12+o-1,SA,comp);
  //mergeTime.stop();
  free(SA0); free(SA12);
  intT* LCP = NULL;


  //get LCP from LCP12
  if(findLCPs){
    LCP = newA(intT,n);  
    LCP[n-1] = LCP[n-2] = 0; 
    //LCPtime.start();
    myRMQ RMQ(LCP12,n12+3); //simple rmq
    native::parallel_for1((intT)0, n-2, [&] (intT i) {
      intT j=SA[i];
      intT k=SA[i+1];
      int CLEN = 16;
      intT ii;
      for (ii=0; ii < CLEN; ii++) 
	if (s[j+ii] != s[k+ii]) break;
      if (ii != CLEN) LCP[i] = ii;
      else {
      	if (j%3 != 0 && k%3 != 0)  
	  LCP[i] = computeLCP(LCP12, rank, RMQ, j, k, s, n); 
	else if (j%3 != 2 && k%3 != 2)
	  LCP[i] = 1 + computeLCP(LCP12, rank, RMQ, j+1, k+1, s, n);
	else 
	  LCP[i] = 2 + computeLCP(LCP12, rank, RMQ, j+2, k+2, s, n);
	  }
      });
    //LCPtime.stop();
    free(LCP12);
  }
  free(rank);
  return make_pair(SA,LCP);
}

  
  pair<intT*,intT*> suffixArray(unsigned char* s, intT n, bool findLCPs) {
  // following line is used to fool icpc into starting the scheduler
  intT *ss = newA(intT,n+3); 
  ss[n] = ss[n+1] = ss[n+2] = 0;
  native::parallel_for((intT)0, n, [&] (intT i) {
      ss[i] = s[i]+1;
    });
  intT k = 1 + sequence::reduce(ss,n,utils::maxF<intT>());


  pair<intT*,intT*> SA_LCP = suffixArrayRec(ss, n, k, findLCPs);
  free(ss);
  return SA_LCP;
}

intT* suffixArray(unsigned char* s, intT n) { 
  return suffixArray(s,n,false).first;
}
  
void doit(int argc, char** argv) {
    intT* R;
  _seq<char> S;
  auto init = [&] {
    std::string infile = pasl::util::cmdline::parse_or_default_string("infile", "");
    char* s = (char*)infile.c_str();
    S = benchIO::readStringFromFile(s);
  };
  auto run = [&] (bool sequential) {
    R = suffixArray((unsigned char*)S.A, S.n);
  };
  auto output = [&] {
    
  };
  auto destroy = [&] {
    
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}
  
}

int main(int argc, char** argv) {
    pbbs::doit(argc, argv);
  return 0;
}
