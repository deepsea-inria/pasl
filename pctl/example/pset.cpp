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
#include "pset.hpp"

/***********************************************************************/

namespace pasl {
  namespace pctl {
    
    void ex() {
      
      pset<int> s;
      s.insert(45);
      s.insert(3);
      s.insert(1);
      s.insert(3);
      //      s.check();
      std::cout << "s = " << s << std::endl;
      
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
