/*!
 * \file filter.cpp
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
    parray<long> xs = { 444, 1, 3, 9, 6, 33, 2, 1, 234, 99 };
    std::cout << "xs\t\t= " << xs << std::endl;
    parray<long> dst(xs.size(), -1);
    long nb_evens = level1::filter(xs.cbegin(), xs.cend(), dst.begin(), [&] (long x) { return x%2==0; });
    std::cout << "evens(xs) ++ { -1 ... } =" << dst << std::endl;
    std::cout << "nb_evens(xs) = " << nb_evens << std::endl;
    std::cout << "odds(xs)\t\t= " << filter(xs, [&] (long x) { return x%2==1; }) << std::endl;
    return;
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
