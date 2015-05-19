/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file dpsdatapar.hpp
 * \brief Destination-passing-style data-parallel operations
 *
 */

#include <limits.h>
#include <memory>
#include <utility>
#ifndef TARGET_MAC_OS
#include <malloc.h>
#endif

#include "datapar.hpp"

#ifndef _PCTL_DPS_DATAPAR_H_
#define _PCTL_DPS_DATAPAR_H_

namespace pasl {
namespace pctl {

/***********************************************************************/
  
namespace dps {
  
/*---------------------------------------------------------------------*/
/* Reduction level 2 */
  
namespace level2 {

template <
  class Input_iter,
  class Result,
  class Output_iter,
  class Combine,
  class Lift_comp_rng,
  class Lift_idx,
  class Seq_scan_rng_dst
  >
void scan(Input_iter lo,
          Input_iter hi,
          const Combine& combine,
          Result& id,
          Output_iter outs_lo,
          const Lift_comp_rng& lift_comp_rng,
          const Lift_idx& lift_idx,
          const Seq_scan_rng_dst& seq_scan_rng_dst,
          scan_type st) {
  using output_type = level3::cell_output<Result, Combine>;
  output_type out(id, combine);
  auto lift_idx_dst = [&] (long pos, Input_iter it, Result& dst) {
    dst = lift_idx(pos, it);
  };
  level3::scan(lo, hi, out, id, outs_lo, lift_comp_rng, lift_idx_dst, seq_scan_rng_dst, st);
}
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Reduction level 1 */
  
namespace level1 {
  
template <
  class Input_iter,
  class Result,
  class Output_iter,
  class Combine,
  class Lift_comp_idx,
  class Lift_idx
>
void scani(Input_iter lo,
           Input_iter hi,
           const Combine& combine,
           Result& id,
           Output_iter outs_lo,
           const Lift_comp_idx& lift_comp_idx,
           const Lift_idx& lift_idx,
           scan_type st) {
  parray<long> w = weights(hi-lo, [&] (long pos) {
    return lift_comp_idx(pos, lo+pos);
  });
  auto lift_comp_rng = [&] (Input_iter _lo, Input_iter _hi) {
    long l = _lo - lo;
    long h = _hi - lo;
    long wrng = w[l] - w[h];
    return (long)(log(wrng) * wrng);
  };
  using output_type = level3::cell_output<Result, Combine>;
  output_type out(id, combine);
  using iterator = typename parray<Result>::iterator;
  auto seq_scan_rng_dst = [&] (Result _id, Input_iter _lo, Input_iter _hi, iterator outs_lo) {
    level4::scan_seq(_lo, _hi, outs_lo, out, _id, [&] (Input_iter src, Result& dst) {
      dst = *src;
    }, st);
  };
  level2::scan(lo, hi, combine, id, outs_lo, lift_comp_rng, lift_idx, seq_scan_rng_dst, st);
}
  
template <
  class Input_iter,
  class Result,
  class Output_iter,
  class Combine,
  class Lift_idx
>
void scani(Input_iter lo,
           Input_iter hi,
           const Combine& combine,
           Result& id,
           Output_iter outs_lo,
           const Lift_idx& lift_idx,
           scan_type st) {
  auto lift_comp_rng = [&] (Input_iter lo, Input_iter hi) {
    return hi - lo;
  };
  using output_type = level3::cell_output<Result, Combine>;
  output_type out(id, combine);
  using iterator = typename parray<Result>::iterator;
  auto seq_scan_rng_dst = [&] (Result id, Input_iter lo, Input_iter hi, iterator outs_lo) {
    level4::scan_seq(lo, hi, outs_lo, out, id, [&] (Input_iter src, Result& dst) {
      dst = *src;
    }, st);
  };
  level2::scan(lo, hi, combine, id, outs_lo, lift_comp_rng, lift_idx, seq_scan_rng_dst, st);
}

} // end namespace
  
/*---------------------------------------------------------------------*/
/* Reduction level 0 */

template <
  class Iter,
  class Item,
  class Combine,
  class Weight
>
void scan(Iter lo,
          Iter hi,
          const Combine& combine,
          Item& id,
          Iter outs_lo,
          const Weight& weight,
          scan_type st) {
  auto lift_idx = [&] (long, Iter it) {
    return *it;
  };
  auto lift_comp_idx = [&] (long, Iter it) {
    return weight(*it);
  };
  level1::scani(lo, hi, combine, id, outs_lo, lift_comp_idx, lift_idx, st);
}
  
template <
  class Iter,
  class Item,
  class Combine
>
void scan(Iter lo,
          Iter hi,
          const Combine& combine,
          Item& id,
          Iter outs_lo,
          scan_type st) {
  auto lift_idx = [&] (long, Iter it) {
    return *it;
  };
  level1::scani(lo, hi, combine, id, outs_lo, lift_idx, st);
}
  
/*---------------------------------------------------------------------*/
/* Pack and filter */
  
template <
  class Input_iter,
  class Output_iter
>
long pack(parray<bool>& flags, Input_iter lo, Input_iter hi, Output_iter dst_lo) {
  return __priv::pack(flags, lo, hi, *lo, [&] (long) {
    return dst_lo;
  });
}

template <
  class Input_iter,
  class Output_iter,
  class Pred
>
long filter(Input_iter lo, Input_iter hi, Output_iter dst_lo, const Pred& p) {
  long n = hi - lo;
  parray<bool> flags(n, [&] (long i) {
    return p(*(lo+i));
  });
  return pack(flags, lo, hi, dst_lo);
}
  
} // end namespace
  
/***********************************************************************/

} // end namespace
} // end namespace

#endif /*! _PCTL_DPS_DATAPAR_H_ */