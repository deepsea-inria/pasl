/*!
 * \file suffixarray.cpp
 * \brief Benchmarking script for parallel sorting algorithms
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include <math.h>
#include "benchmark.hpp"
#include "suffixarray.hpp"

/***********************************************************************/

using namespace pasl::pctl;

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (pasl::sched::experiment exp) {
    std::cout << "todo" << std::endl;
  });
  return 0;
}

/***********************************************************************/
