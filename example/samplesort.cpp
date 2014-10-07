// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2010 Guy Blelloch and the PBBS team
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
#include "randperm.hpp"
#include "sequenceio.hpp"
#include "samplesort.hpp"
#include "sequencedata.hpp"
#include "benchmark.hpp"

namespace pbbs {
using namespace std;
using namespace pbbs::benchIO;

struct strCmp {
  bool operator() (char* s1c, char* s2c) {
    char* s1 = s1c, *s2 = s2c;
    while (*s1 && *s1==*s2) {s1++; s2++;};
    return (*s1 < *s2);
  }
};

template <class intT>
void doit(int argc, char** argv) {
  intT* seq;
  intT n = 0;
  intT r;
  auto init = [&] {
    n = (intT)pasl::util::cmdline::parse_or_default_int64("n", 100000);
    r = (intT)pasl::util::cmdline::parse_or_default_int64("r", 100000);
    seq = dataGen::randIntRange<intT>(0,n,r);
  };
  auto run = [&] (bool sequential) {
    pbbs::sampleSort(seq, n, less<intT>());
  };
  auto output = [&] {
    
  };
  auto destroy = [&] {
    free(seq);
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}
}

int main(int argc, char** argv) {
  pbbs::doit<int>(argc, argv);
  return 0;
}
