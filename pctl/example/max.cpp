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
#include "max.hpp"
#include "io.hpp"

/***********************************************************************/

namespace pasl {
namespace pctl {

void ex() {
  
  {
    parray::parray<long> xs = { 1, 3, 9, 0, 33, 1, 1 };
    std::cout << "xs\t\t= " << xs << std::endl;
    std::cout << "max(xs)\t= " << max(xs) << std::endl;
  }
  
  parray::parray<parray::parray<long>> xss = { {23, 1, 3}, { 19, 3, 3 }, { 100 } };
  
  {
    std::cout << "xss\t\t= " << xss << std::endl;
    std::cout << "max0(xss)\t= " << max0(xss) << std::endl;
  }
  
  {
    std::cout << "xss\t\t= " << xss << std::endl;
    std::cout << "max1(xss)\t= " << max1(xss) << std::endl;
  }
  
  {
    std::cout << "xss\t\t= " << xss << std::endl;
    std::cout << "max2(xss)\t= " << max2(xss) << std::endl;
  }
  
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
