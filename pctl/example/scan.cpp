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
#include "io.hpp"

/***********************************************************************/

namespace pasl {
  namespace pctl {
    
    void ex() {
      
      {
        parray<long> xs = { 1, 3, 9, 0, 33, 1, 1 };
        std::cout << "xs\t\t= " << xs << std::endl;
        auto combine = [&] (long x, long y) {
          return x + y;
        };
        parray<long> ys = scan(xs.cbegin(), xs.cend(), 0L, combine, backward_exclusive_scan);
        std::cout << "ys = " << ys << std::endl;
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
