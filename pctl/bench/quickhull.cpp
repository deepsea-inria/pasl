/*!
 * \file quickhull.cpp
 * \brief Benchmarking script for parallel sorting algorithms
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include <math.h>
#include "benchmark.hpp"
#include "hull.hpp"

/***********************************************************************/

using namespace pasl::pctl;

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (pasl::sched::experiment exp) {
    parray<point2d> points = pasl::pctl::load_points2d<intT>();
    parray<intT> hull_idxs;
    
    // run the experiment
    exp([&] {
      hull_idxs = hull(points);
    });
    
    // output the results
    std::cout << "|points| = " << points.size() << std::endl;
    std::cout << "|hull| = " << hull_idxs.size() << std::endl;
  });
  return 0;
}

/***********************************************************************/
