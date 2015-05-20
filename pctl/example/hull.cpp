/*!
 * \file hull.cpp
 * \brief Benchmarking script for parallel sorting algorithms
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include "pasl.hpp"
#include "io.hpp"
#include "hull.hpp"
#include "seqhull.hpp"
#include "geometryio.hpp"

/***********************************************************************/

namespace pasl {
  namespace pctl {
    
    void ex() {
      
      parray<point2d> points = {
        point2d(0.676688, -0.323272),
        point2d(-0.17929, 0.644188),
        point2d(0.537581, -0.885689),
        point2d(0.241092, 0.807589),
        point2d(-0.118645, 0.785797)
      };
      
      std::cout << "points = " << points << std::endl;
      std::cout << "parallel hull:\n" << hull(points) << std::endl;
      std::cout << "sequential hull:\n" << sequential::hull(points) << std::endl;

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
