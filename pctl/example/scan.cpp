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

/***********************************************************************/

namespace pasl {
  namespace pctl {
    
    template <
      class Input_iter,
      class Combine,
      class Result,
      class Output_iter
    >
    void in_place_scan(Input_iter lo,
                        Input_iter hi,
                        const Combine& combine,
                        Result& id,
                        Output_iter outs_lo,
                        scan_type st) {
      using output_type = level3::cell_output<Result, Combine>;
      output_type out(id, combine);
      auto lift_idx_dst = [&] (long pos, Input_iter it, Result& dst) {
        dst = *it;
      };
      auto lift_comp_rng = [&] (Input_iter lo, Input_iter hi) {
        return hi - lo;
      };
      using iterator = typename parray<Result>::iterator;
      auto seq_scan_rng_dst = [&] (Result _id, Input_iter _lo, Input_iter _hi, iterator outs_lo) {
        level4::scan_seq(_lo, _hi, outs_lo, out, _id, [&] (Input_iter src, Result& dst) {
          dst = *src;
        }, st);
      };
      level3::scan(lo, hi, out, id, outs_lo, lift_comp_rng, lift_idx_dst, seq_scan_rng_dst, st);
    }
    
    void ex() {
      
      {
        parray<long> xs = { 1, 3, 9, 0, 33, 1, 1 };
        std::cout << "xs\t= " << xs << std::endl;
        auto combine = [&] (long x, long y) {
          return x + y;
        };
        
        parray<long> ys1 = scan(xs.cbegin(), xs.cend(), 0L, combine, forward_exclusive_scan);
        std::cout << "ys1\t= " << ys1 << std::endl;
        
        parray<long> ys2 = scan(xs.cbegin(), xs.cend(), 0L, combine, backward_exclusive_scan);
        std::cout << "ys2\t= " << ys2 << std::endl;
        
        parray<long> ys3 = scan(xs.cbegin(), xs.cend(), 0L, combine, forward_inclusive_scan);
        std::cout << "ys3\t= " << ys3 << std::endl;
        
        parray<long> ys4 = scan(xs.cbegin(), xs.cend(), 0L, combine, backward_inclusive_scan);
        std::cout << "ys4\t= " << ys4 << std::endl;
        
        // in-place scan
        
        long id = 0;
        in_place_scan(xs.begin(), xs.end(), combine, id, xs.begin(), forward_exclusive_scan);
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
