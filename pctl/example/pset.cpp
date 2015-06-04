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

      s.erase(45);
      
      s.insert(45);
      s.insert(78);
      s.erase(1);
      
      pset<int> s2 = { 4, 45, 100, 303 };
      s.merge(s2);
      
      std::cout << "s = " << s << std::endl;
      
      pset<int> s3 = { 4, 100 };
      s.intersect(s3);
      
      std::cout << "s = " << s << std::endl;
      
      pset<int> s4 = { 4, 45, 100, 303 };
      pset<int> s5 = { 0, 1, 100, 303, 555 };

      s4.diff(s5);
      
      std::cout << s4 << std::endl;
      
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
