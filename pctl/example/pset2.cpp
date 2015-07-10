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
      
      {
        pset<int> s;
        s.insert(5);
        s.insert(432);
        s.insert(5);
        s.insert(89);
        s.erase(5);
        std::cout << "s = " << s << std::endl;
      }
      
      {
        pset<int> s1 = { 3, 1, 5, 8, 12 };
        pset<int> s2 = { 3, 54, 8, 9 };
        s1.merge(s2);
        std::cout << "s1 = " << s1 << std::endl;
        std::cout << "s2 = " << s2 << std::endl;
      }
      
      {
        pset<int> s1 = { 3, 1, 5, 8, 12 };
        pset<int> s2 = { 3, 54, 8, 9 };
        s1.intersect(s2);
        std::cout << "s1 = " << s1 << std::endl;
        std::cout << "s2 = " << s2 << std::endl;
      }
      
      {
        pset<int> s1 = { 3, 1, 5, 8, 12 };
        pset<int> s2 = { 3, 54, 8, 9 };
        s1.diff(s2);
        std::cout << "s1 = " << s1 << std::endl;
        std::cout << "s2 = " << s2 << std::endl;
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
