/*!
 * \file mergesort.cpp
 * \brief Benchmarking script for parallel sorting algorithms
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include "pasl.hpp"
#include "psort.hpp"
#include "io.hpp"

/***********************************************************************/

namespace pasl {
namespace pctl {

void ex() {
  
  // parallel array
  std::cout << "-- parray -- " << std::endl;
  {
    parray<int> xs = { 3, 2, 100, 1, 0, -1, -3 };
    std::cout << "xs\t\t\t=\t" << xs << std::endl;
    sort::mergesort(xs);
    std::cout << "mergesort(xs)\t=\t" << xs << std::endl;
  }
  std::cout << std::endl;
  
  // parallel chunked sequence
  std::cout << "-- pchunkedseq -- " << std::endl;
  {
    pchunkedseq<int> xs = { 3, 2, 100, 1, 0, -1, -3 };
    std::cout << "xs\t\t\t=\t" << xs << std::endl;
    std::cout << "mergesort(xs)\t=\t" << sort::mergesort(xs) << std::endl;
  }
  std::cout << std::endl;

}
  
}
}

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (bool sequential) {
    pasl::pctl::ex();
  });
  return 0;
}

/***********************************************************************/
