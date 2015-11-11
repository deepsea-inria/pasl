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

template <class E, class BinPred>
void comparisonSort(E* A, long n, BinPred f) {
#ifdef SEQUENTIAL_ELISION
  std::sort(A,A+n,f);
#else
  sampleSort(A, n, f);
#endif
}

template <class intT>
void doit(int argc, char** argv) {
  intT* seq = nullptr;
  seqData doubles;
  seqData strings;
  intT n = 0;
  intT r;
  auto init = [&] {
    std::string infile = pasl::util::cmdline::parse_or_default_string("infile", "");
    std::string ftype = pasl::util::cmdline::parse_or_default_string("type", "");
    char* s = (char*)infile.c_str();
    if (ftype == "doubles") {
      new (&doubles) seqData(readSequenceFromFile<long>(s));
      assert(doubles.A != nullptr);
    } else if (ftype == "strings") {
      new (&strings) seqData(readSequenceFromFile<long>(s));
    } else {
      n = (intT)pasl::util::cmdline::parse_or_default_int64("n", 100000);
      r = (intT)pasl::util::cmdline::parse_or_default_int64("r", 100000);
      seq = dataGen::randIntRange<intT>(0,n,r);
    }
  };
  auto run = [&] (bool sequential) {
    if (seq != nullptr) {
      comparisonSort(seq, (long)n, less<intT>());
    } else if (doubles.A != nullptr) {
      comparisonSort((double*)doubles.A, (long)doubles.n, less<double>());
    } else if (strings.A != nullptr) {
      comparisonSort((char**) strings.A, (long)strings.n, strCmp());
    }
  };
  auto output = [&] {
    
  };
  auto destroy = [&] {
    if (seq != nullptr) {
      free(seq);
    }
    //    doubles.del();
    //    strings.del();
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}
}

int main(int argc, char** argv) {
    pbbs::doit<int>(argc, argv);
  return 0;
}
