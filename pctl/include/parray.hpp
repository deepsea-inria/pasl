/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file parray.hpp
 * \brief Array-based implementation of sequences
 *
 */

#include <cmath>

#include "datapar.hpp"

#ifndef _PARRAY_PCTL_PARRAY_H_
#define _PARRAY_PCTL_PARRAY_H_

namespace pasl {
namespace pctl {
namespace parray {
  
/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/* Reduction level 3 */

namespace level3 {

template <
  class Iter,
  class Output,
  class Lift_comp_rng,
  class Lift_idx_dst,
  class Seq_lift_dst
>
void reduce(Iter lo,
            Iter hi,
            Output& out,
            Lift_comp_rng lift_comp_rng,
            Lift_idx_dst lift_idx_dst,
            Seq_lift_dst seq_lift_dst) {
  using input_type = datapar::level4::random_access_iterator_input<Iter>;
  input_type in(lo, hi);
  auto lift_comp = [&] (input_type& in) {
    return lift_comp_rng(in.lo, in.hi);
  };
  auto convert = [&] (input_type& in, Output& out) {
    long i = 0;
    for (Iter it = in.lo; it != in.hi; it++, i++) {
      lift_idx_dst(i, it, out);
    }
  };
  auto seq_convert = [&] (input_type& in, Output& out) {
    seq_lift_dst(in.lo, in.hi, out);
  };
  datapar::level4::reduce(in, out, lift_comp, convert, seq_convert);
}

} // end namespace
  
/*---------------------------------------------------------------------*/
/* Reduction level 2 */

namespace level2 {
  
template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp_rng,
  class Lift_idx,
  class Seq_lift
>
Result reduce(Iter lo,
              Iter hi,
              Result id,
              const Combine& combine,
              const Lift_comp_rng& lift_comp_rng,
              const Lift_idx& lift_idx,
              const Seq_lift& seq_lift) {
  using output_type = datapar::level3::cell<Result, Combine>;
  output_type out(id, combine);
  auto lift_idx_dst = [&] (long pos, Iter it, output_type& out) {
    out.result = lift_idx(pos, it);
  };
  auto seq_lift_dst = [&] (Iter lo, Iter hi, output_type& out) {
    out.result = seq_lift(lo, hi);
  };
  level3::reduce(lo, hi, out, lift_comp_rng, lift_idx_dst, seq_lift_dst);
  return out.result;
}
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Reduction level 1 */

namespace level1 {
  
template <
  class Iter,
  class Result,
  class Combine,
  class Lift_idx
>
Result reducei(Iter lo,
               Iter hi,
               Result id,
               Combine combine,
               Lift_idx lift_idx) {
  auto lift_comp_rng = [&] (Iter lo, Iter hi) {
    return hi - lo;
  };
  auto seq_lift = [&] (Iter lo, Iter hi) {
    Result r = id;
    long i = 0;
    for (Iter it = lo; it != hi; it++, i++) {
      r = combine(r, lift_idx(i, it));
    }
    return r;
  };
  return level2::reduce(lo, hi, id, combine, lift_comp_rng, lift_idx, seq_lift);
}
  
template <
  class Iter,
  class Result,
  class Combine,
  class Lift
>
Result reduce(Iter lo,
              Iter hi,
              Result id,
              Combine combine,
              Lift lift) {
  auto lift_idx = [&] (long pos, Iter it) {
    return lift(it);
  };
  return reducei(lo, hi, id, combine, lift_idx);
}
  
template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp_idx,
  class Lift_idx
>
Result reducei(Iter lo,
               Iter hi,
               Result id,
               Combine combine,
               Lift_comp_idx lift_comp_idx,
               Lift_idx lift_idx) {
  parray<long> w = weights(hi-lo, [&] (long pos) {
    return lift_comp_idx(pos, lo+pos);
  });
  auto lift_comp_rng = [&] (Iter _lo, Iter _hi) {
    long l = _lo - lo;
    long h = _hi - lo;
    long wrng = w[l] - w[h];
    return (long)(log(wrng) * wrng);
  };
  auto seq_lift = [&] (Iter lo, Iter hi) {
    Result r = id;
    long i = 0;
    for (Iter it = lo; it != hi; it++, i++) {
      r = combine(r, lift_idx(i, it));
    }
    return r;
  };
  return level2::reduce(lo, hi, id, combine, lift_comp_rng, lift_idx, seq_lift);
}

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp,
  class Lift
>
Result reduce(Iter lo,
              Iter hi,
              Result id,
              Combine combine,
              Lift_comp lift_comp,
              Lift lift) {
  auto lift_comp_idx = [&] (long pos, Iter it) {
    return lift_comp(it);
  };
  auto lift_idx = [&] (long pos, Iter it) {
    return lift(it);
  };
  return reducei(lo, hi, id, combine, lift_comp_idx, lift_idx);
}
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Reduction level 0 */

template <class Iter, class Item, class Combine>
Item reduce(Iter lo, Iter hi, Item id, Combine combine) {
  auto lift = [&] (Iter it) {
    return *it;
  };
  return level1::reduce(lo, hi, id, combine, lift);
}

template <
  class Iter,
  class Item,
  class Weight,
  class Combine
>
Item reduce(Iter lo,
            Iter hi,
            Item id,
            Weight weight,
            Combine combine) {
  auto lift = [&] (Iter it) {
    return *it;
  };
  auto lift_comp = [&] (Iter it) {
    return weight(*it);
  };
  return level1::reduce(lo, hi, id, combine, lift_comp, lift);
}
  
/***********************************************************************/
  
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PARRAY_PCTL_PARRAY_H_ */
