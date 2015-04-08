/*!
 * \file mergesort.cpp
 * \brief Benchmarking script for parallel sorting algorithms
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include "benchmark.hpp"
#include "psort.hpp"
#include "io.hpp"

/***********************************************************************/

namespace pasl {
namespace pctl {

void ex() {
  
  // parallel array
  {
    parray::parray<int> xs = { 3, 2, 100, 1, 0, -1, -3 };
    std::cout << xs << std::endl;
    sort::mergesort(xs);
    std::cout << xs << std::endl;
  }
  
  // parallel chunked sequence
  {
    pchunkedseq::pchunkedseq<int> xs = { 3, 2, 100, 1, 0, -1, -3 };
    std::cout << xs << std::endl;
    pchunkedseq::pchunkedseq<int> ys = sort::mergesort(xs);
    std::cout << ys << std::endl;
  }

}
  
}
}

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  
  auto init = [&] {

  };
  auto run = [&] (bool sequential) {
    pasl::pctl::ex();
  };
  auto output = [&] {
    
  };
  auto destroy = [&] {
    ;
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
  return 0;
}

/***********************************************************************/
