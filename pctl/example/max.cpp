/*!
 * \file max.cpp
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
  
using namespace max_ex;

void ex() {
  
  {
    parray<int> xs = { 1, 3, 9, 0, 33, 1, 1 };
    std::cout << "xs\t\t= " << xs << std::endl;
    std::cout << "max(xs)\t= " << max_ex::max(xs) << std::endl;
  }
  
  parray<parray<int>> xss = { {23, 1, 3}, { 19, 3, 3 }, { 100 } };
  
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
  
  {
    std::cout << "xss\t\t= " << xss << std::endl;
    std::cout << "max3(xss)\t= " << max3(xss) << std::endl;
  }
  
}

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (bool sequential) {
    pasl::pctl::ex();
  });
  return 0;
}

/***********************************************************************/
