/*!
 * \file psort.cpp
 * \brief Benchmarking script for parallel sorting algorithms
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include <math.h>
#include "benchmark.hpp"
#include "sequencedata.hpp"
#include "psort.hpp"

/***********************************************************************/

namespace pasl {
namespace pctl {

template <class Number>
parray::parray<Number> random_parray(Number s, Number e, Number m) {
  Number n = e - s;
  parray::parray<Number> result((long)n);
  parallel_for(Number(0), n, [&] (Number i) {
    result[i] = pbbs::dataGen::hash<Number>(i+s)%m;
  });
  return result;
}

template <class Number>
pchunkedseq::pchunkedseq<Number> random_pchunkedseq(Number s, Number e, Number m) {
  parray::parray<Number> tmp = random_parray(s, e, m);
  pchunkedseq::pchunkedseq<Number> result(tmp.size(), [&] (long i) {
    return tmp[i];
  });
  return result;
}

}
}

/*---------------------------------------------------------------------*/

template <class Item>
using pchunkedseq = pasl::pctl::pchunkedseq::pchunkedseq<Item>;
template <class Item>
using parray = pasl::pctl::parray::parray<Item>;

int main(int argc, char** argv) {
  pchunkedseq<int>* xsp = nullptr;
  parray<int>* pap = nullptr;
  long n;
  
  auto init = [&] {
    n = pasl::util::cmdline::parse_or_default_long("n", 100000);
    long m = pasl::util::cmdline::parse_or_default_long("m", 200*n);
    std::string datastruct = pasl::util::cmdline::parse_or_default_string("datastruct", "pchunkedseq");
    if (datastruct == "pchunkedseq") {
      xsp = new pchunkedseq<int>();
      *xsp = pasl::pctl::random_pchunkedseq((int)0, (int)n, (int)m);
    } else if (datastruct == "parray") {
      pap = new parray<int>();
      *pap = pasl::pctl::random_parray((int)0, (int)n, (int)m);
    } else {
      pasl::util::atomic::die("bogus datastruct");
    }
  };
  auto run = [&] (bool sequential) {
    if (xsp != nullptr) {
      *xsp = pasl::pctl::sort::mergesort(*xsp);
    } else if (pap != nullptr) {
      pasl::pctl::sort::mergesort(*pap);
    }
  };
  auto output = [&] {
    int x;
    long sz;
    if (xsp != nullptr) {
      x = xsp->seq[0];
      sz = xsp->seq.size();
    } else if (pap != nullptr) {
      x = (*pap)[0];
      sz = pap->size();
    }
    std::cout << "result\t" << x << std::endl;
    assert(sz == n);
  };
  auto destroy = [&] {
    ;
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
  return 0;
}

/***********************************************************************/
