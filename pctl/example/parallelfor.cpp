/*!
 * \file parallelfor.cpp
 * \brief Benchmarking script for parallel sorting algorithms
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include "pasl.hpp"
#include "io.hpp"
#include "datapar.hpp"

/***********************************************************************/

namespace pasl {
  namespace pctl {
    
    
    void ex() {
      
      { // assign each position i in xs to the value i+1
        parray<long> xs = { 0, 0, 0, 0 };
        parallel_for((long)0, xs.size(), [&] (long i) {
          xs[i] = i+1;
        });
        
        std::cout << "xs = " << xs << std::endl;
      }
      
      { // increment each value in a parallel array
        parray<long> xs = { 0, 1, 2, 3 };
        parallel_for(xs.begin(), xs.end(), [&] (long* p) {
          long& xs_i = *p;
          xs_i = xs_i + 1;
        });
        
        std::cout << "xs = " << xs << std::endl;
      }
      
      { // increment each value in a parallel chunked sequence
        pchunkedseq<long> xs = { 0, 1, 2, 3 };
        parallel_for(xs.begin(), xs.end(), [&] (typename pchunkedseq<long>::iterator p) {
          long& xs_i = *p;
          xs_i = xs_i + 1;
        });
        
        std::cout << "xs = " << xs << std::endl;
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
