
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

#ifndef _myRMQ_hpp_
#define _myRMQ_hpp_

#include <iostream>
#include <math.h>
//#include "merge.hpp"
#define BSIZE 16

namespace pasl {
namespace pctl {
  
using intT = int;
    
using namespace std;

class myRMQ{
protected:
  intT* a;
  intT n;
  intT m;
  intT** table;
  intT depth;
  
public:
  myRMQ(intT* _A, intT _n);
  void precomputeQueries();
  intT query(intT,intT);
  ~myRMQ();
};

myRMQ::myRMQ(intT* _a, intT _n){
  a = _a;
  n = _n;
  m = 1 + (n-1)/BSIZE;
  precomputeQueries();
}

void myRMQ::precomputeQueries(){
  depth = log2(m) + 1;
  table = new intT*[depth];
  parallel_for((intT)0, depth, [&] (intT k) {
    table[k] = new intT[n];
  });
  
  parallel_for((intT)0, m, [&] (intT i) {
    intT start = i*BSIZE;
    intT end = min(start+BSIZE,n);
    intT k = i*BSIZE;
    for (intT j = start+1; j < end; j++)
      if (a[j] < a[k]) k = j;
    table[0][i] = k;
  });
  intT dist = 1;
  for(intT j=1;j<depth;j++) {
    parallel_for((intT)0, m-dist, [&] (intT i) {
      if (a[table[j-1][i]] <= a[table[j-1][i+dist]])
        table[j][i] = table[j-1][i];
      else table[j][i] = table[j-1][i+dist];
    });
    parallel_for((intT)(m-dist), m, [&] (intT l) {
      table[j][l] = table[j-1][l];
    });
    dist*=2;
  }
  
}

intT myRMQ::query(intT i, intT j){
  //same block
  if (j-i < BSIZE) {
    intT r = i;
    for (intT k = i+1; k <= j; k++)
      if (a[k] < a[r]) r = k;
    return r;
  }
  intT block_i = i/BSIZE;
  intT block_j = j/BSIZE;
  intT min = i;
  for(intT k=i+1;k<(block_i+1)*BSIZE;k++){
    if(a[k] < a[min]) min = k;
  }
  for(intT k=j; k>=(block_j)*BSIZE;k--){
    if(a[k] < a[min]) min = k;
  }
  if(block_j == block_i + 1) return min;
  intT outOfBlockMin;
  //not same or adjacent blocks
  if(block_j > block_i + 1){
    block_i++;
    block_j--;
    if(block_j == block_i) outOfBlockMin = table[0][block_i];
    else if(block_j == block_i + 1) outOfBlockMin = table[1][block_i];
    else {
      intT k = log2(block_j - block_i);
      intT p = 1<<k; //2^k
      outOfBlockMin = a[table[k][block_i]] <= a[table[k][block_j+1-p]]
      ? table[k][block_i] : table[k][block_j+1-p];
    }
  }
  
  return a[min] < a[outOfBlockMin] ? min : outOfBlockMin;
  
}

myRMQ::~myRMQ(){
  
  parallel_for((intT)0, depth, [&] (intT i) {
    delete[] table[i];
  });
  delete[] table;
}
  
} // end namespace
} // end namespace

#endif
