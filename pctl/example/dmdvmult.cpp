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
    
    double ddotprod(long n, const double* row, const double* vec) {
      return level1::reducei(vec, vec+n, 0.0, [&] (double x, double y) {
        return x + y;
      }, [&] (long i, const double&) {
        return row[i] * vec[i];
      });
    }
    
    parray<double> dmdvmult1(const parray<double>& mtx, const parray<double>& vec) {
      long n = vec.size();
      parray<double> result(n);
      auto comp = [&] (long i) {
        return n;
      };
      parallel_for(0L, n, comp, [&] (long i) {
        result[i] = ddotprod(n, mtx.cbegin()+(i*n), vec.begin());
      });
      return result;
    }
    
    parray<double> dmdvmult2(const parray<double>& mtx, const parray<double>& vec) {
      long n = vec.size();
      parray<double> result(n);
      auto comp_rng = [&] (long lo, long hi) {
        return (hi - lo) * n;
      };
      range::parallel_for(0L, n, comp_rng, [&] (long i) {
        result[i] = ddotprod(n, mtx.cbegin()+(i*n), vec.begin());
      });
      return result;
    }
    
    parray<double> dmdvmult3(const parray<double>& mtx, const parray<double>& vec) {
      long n = vec.size();
      parray<double> result(n);
      auto comp_rng = [&] (long lo, long hi) {
        return (hi - lo) * n;
      };
      range::parallel_for(0L, n, comp_rng, [&] (long i) {
        result[i] = ddotprod(n, mtx.cbegin()+(i*n), vec.begin());
      }, [&] (long lo, long hi) {
        for (long i = lo; i < hi; i++) {
          double dotp = 0.0;
          for (long j = 0; j < n; j++) {
            dotp += mtx[i*n+j] * vec[j];
          }
          result[i] = dotp;
        }
      });
      return result;
    }
    
    void ex() {
      
      parray<double> mtx = { 1.1, 2.1, 0.3, 5.8,
                             8.1, 9.3, 3.1, 3.2,
                             5.3, 3.5, 7.9, 2.3,
                             4.5, 5.5, 3.4, 4.5 };
      
      parray<double> vec = { 4.3, 0.3, 2.1, 3.3 };
      
      {
        parray<double> result = dmdvmult1(mtx, vec);
        std::cout << "result = " << result << std::endl;
      }
      
      {
        parray<double> result = dmdvmult2(mtx, vec);
        std::cout << "result = " << result << std::endl;
      }
      
      {
        parray<double> result = dmdvmult3(mtx, vec);
        std::cout << "result = " << result << std::endl;
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
