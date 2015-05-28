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
          Result& id,
          const Combine& combine,
          Output_iter outs_lo,
          const Lift_comp_rng& lift_comp_rng,
          const Lift_idx& lift_idx,
          const Seq_scan_rng_dst& seq_scan_rng_dst,
          scan_type st) {
  using output_type = level3::cell_output<Result, Combine>;
  output_type out(id, combine);
  auto lift_idx_dst = [&] (long pos, reference_of<Input_iter> x, Result& dst) {
    dst = lift_idx(pos, x);
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
Result scani(Input_iter lo,
             Input_iter hi,
             Result& id,
             const Combine& combine,
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
    level4::scan_seq(_lo, _hi, outs_lo, out, _id, [&] (reference_of<Input_iter> src, Result& dst) {
      dst = src;
    }, st);
  };
  level2::scan(lo, hi, id, combine, outs_lo, lift_comp_rng, lift_idx, seq_scan_rng_dst, st);
  return total_of_exclusive_scan(lo, outs_lo, hi-lo, id, combine, lift_idx);
}
  
template <
  class Input_iter,
  class Result,
  class Output_iter,
  class Combine,
  class Lift_idx
>
Result scani(Input_iter lo,
             Input_iter hi,
             Result& id,
             const Combine& combine,
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
    level4::scan_seq(lo, hi, outs_lo, out, id, [&] (reference_of<Input_iter> src, Result& dst) {
      dst = src;
    }, st);
  };
  level2::scan(lo, hi, id, combine, outs_lo, lift_comp_rng, lift_idx, seq_scan_rng_dst, st);

  return pasl::pctl::level1::total_from_exclusive_scani(lo, hi, outs_lo, id, combine, lift_idx);
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
Item scan(Iter lo,
          Iter hi,
          Item id,
          const Combine& combine,
          Iter outs_lo,
          const Weight& weight,
          scan_type st) {
  auto lift_idx = [&] (long, reference_of<Iter> x) {
    return x;
  };
  auto lift_comp_idx = [&] (long, reference_of<Iter>x ) {
    return weight(x);
  };
  return level1::scani(lo, hi, id, combine, outs_lo, lift_comp_idx, lift_idx, st);
}
  
template <
  class Iter,
  class Item,
  class Combine
>
Item scan(Iter lo,
          Iter hi,
          Item id,
          const Combine& combine,
          Iter outs_lo,
          scan_type st) {
  auto lift_idx = [&] (long, reference_of<Iter> x) {
    return x;
  };
  return level1::scani(lo, hi, id, combine, outs_lo, lift_idx, st);
}
  
/*---------------------------------------------------------------------*/
/* Pack and filter */
  
template <
  class Flags_iter,
  class Input_iter,
  class Output_iter
>
long pack(Flags_iter flags_lo, Input_iter lo, Input_iter hi, Output_iter dst_lo) {
  return __priv::pack(flags_lo, lo, hi, *lo, [&] (long) {
    return dst_lo;
  }, [&] (long, reference_of<Input_iter> x) {
    return x;
  });
}

template <
  class Input_iter,
  class Output_iter,
  class Pred_idx
>
long filteri(Input_iter lo, Input_iter hi, Output_iter dst_lo, const Pred_idx& pred_idx) {
  long n = hi - lo;
  parray<bool> flags(n, [&] (long i) {
    return pred_idx(i, *(lo+i));
  });
  return pack(flags.cbegin(), lo, hi, dst_lo);
}
  
template <
  class Input_iter,
  class Output_iter,
  class Pred
>
long filter(Input_iter lo, Input_iter hi, Output_iter dst_lo, const Pred& pred) {
  auto pred_idx = [&] (long, reference_of<Input_iter> x) {
    return pred(x);
  };
  return filteri(lo, hi, dst_lo, pred_idx);
}
  
} // end namespace
  
/***********************************************************************/

} // end namespace
} // end namespace

#endif /*! _PCTL_DPS_DATAPAR_H_ */