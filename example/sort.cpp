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
  intT* seq = nullptr;
  seqData doubles;
  seqData strings;
  intT n = 0;
  intT r;
  auto init = [&] {
    std::string doublesstr = pasl::util::cmdline::parse_or_default_string("doubles", "");
    std::string stringsstr = pasl::util::cmdline::parse_or_default_string("strings", "");    
    if (doublesstr != "") {
      char* s = (char*)doublesstr.c_str();
      new (&doubles) seqData(readSequenceFromFile<long>(s));
      assert(doubles.A != nullptr);
    } else if (stringsstr != "") {
      char* s = (char*)stringsstr.c_str();
      new (&strings) seqData(readSequenceFromFile<long>(s));
    } else {
      n = (intT)pasl::util::cmdline::parse_or_default_int64("n", 100000);
      r = (intT)pasl::util::cmdline::parse_or_default_int64("r", 100000);
      seq = dataGen::randIntRange<intT>(0,n,r);
    }
  };
  auto run = [&] (bool sequential) {
    if (seq != nullptr) {
      pbbs::sampleSort(seq, n, less<intT>());
    } else if (doubles.A != nullptr) {
      pbbs::sampleSort((double*)doubles.A, doubles.n, less<double>());
    } else if (strings.A != nullptr) {
      pbbs::sampleSort((char**) strings.A, strings.n, strCmp());
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
