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

int main(int argc, char** argv) {
  pchunkedseq<int>* xsp;
  
  long n;
  auto init = [&] {
    n = pasl::util::cmdline::parse_or_default_long("n", 100000);
    long m = pasl::util::cmdline::parse_or_default_long("m", 200*n);
    xsp = new pchunkedseq<int>();
    *xsp = pasl::pctl::random_pchunkedseq((int)0, (int)n, (int)m);
  };
  auto run = [&] (bool sequential) {
    *xsp = pasl::pctl::sort::mergesort(*xsp);
  };
  auto output = [&] {
    std::cout << "result\t" << xsp->seq[0] << std::endl;
    assert(xsp->seq.size() == n);
  };
  auto destroy = [&] {
    ;
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
  return 0;
}

/***********************************************************************/
