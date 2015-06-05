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
#include "dpsdatapar.hpp"

/***********************************************************************/

namespace pasl {
  namespace pctl {
    
   
    void ex() {
     
      auto combine = [&] (int x, int y) {
        return x + y;
      };
      
      parray<int> xs = { 1, 3, 9, 0, 33, 1, 1 };
      std::cout << "xs\t= " << xs << std::endl;
      
      { // forward-inclusive scan
        parray<int> fe = scan(xs.cbegin(), xs.cend(), 0, combine, forward_exclusive_scan);
        std::cout << "fe\t= " << fe << std::endl;
      }
      
      { // backward-exclusive scan
        parray<int> be = scan(xs.cbegin(), xs.cend(), 0, combine, backward_exclusive_scan);
        std::cout << "be\t= " << be << std::endl;
      }
      
      { // forward-inclusive scan
        parray<int> fi = scan(xs.cbegin(), xs.cend(), 0, combine, forward_inclusive_scan);
        std::cout << "fi\t= " << fi << std::endl;
      }
      
      { // backward-inclusive scan
        parray<int> bi = scan(xs.cbegin(), xs.cend(), 0, combine, backward_inclusive_scan);
        std::cout << "bi\t= " << bi << std::endl;
      }
      
      { // in-place version of forward-exclusive scan
        dps::scan(xs.begin(), xs.end(), 0, combine, xs.begin(), forward_exclusive_scan);
        std::cout << "xs\t= " << xs << std::endl;
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
