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
#include "pstring.hpp"

/***********************************************************************/

namespace pasl {
  namespace pctl {
    
    namespace weighted {
      
      template <class Item>
      class by_size {
      public:
        
        pasl::pctl::parray<long> ws;
        
        by_size() { }
        
        void resize(const Item* lo, const Item* hi) {
          long n = hi - lo;
          ws = weights(n, [&] (long i) {
            const Item& x = *(lo + i);
            return x.size();
          });
        }
        
        const long* begin() const {
          return ws.cbegin();
        }
        
        const long* end() const {
          return ws.cend();
        }
        
        void swap(by_size& other) {
          ws.swap(other.ws);
        }
        
      };
      
    }

    void ex() {

      {
        auto w = weighted::unary<int>();
        weighted::parray<int> xs(w);
        xs.tabulate(5, [&] (int i) {
          return i;
        });
        
        auto lo = xs.begin();
        auto hi = xs.end();
        
        std::cout << "xs = " << xs << std::endl;
        std::cout << "weight_of(xs.begin(),xs.end()) = " << weighted::weight_of(lo, hi) << std::endl;
      }

      {
        auto w = weighted::by_size<std::string>();
        weighted::parray<std::string, decltype(w)> xs(w);
        xs.tabulate(5, [&] (int i) {
          return std::to_string(i)+"x";
        });
        
        auto lo = xs.begin();
        auto hi = xs.end();
        
        std::cout << "xs = " << xs << std::endl;
        std::cout << "weight_of(xs.begin(),xs.end()) = " << weighted::weight_of(lo+2, hi) << std::endl;
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
