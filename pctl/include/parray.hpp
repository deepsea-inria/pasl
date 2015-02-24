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
  
namespace level5 {

template <class Item>
class block_input {
public:
  
  using slice_type = slice<Item>;
  
  slice_type slice;
  
  block_input(const slice_type slice)
  : slice(slice) { }
  
  block_input(const parray<Item>& xs)
  : slice(xs) { }
  
  bool can_split() const {
    return size() > block_size();
  }
  
  long size() const {
    return slice.hi - slice.lo;
  }
  
  long nb_blocks() const {
    return (long)std::pow(size(), 0.5);
  }
  
  long block_size() const {
    return 1 + ((size() - 1) / nb_blocks());
  }
  
};

} // end namespace
  
/*---------------------------------------------------------------------*/

template <class Item, class Weight>
parray<long> weights(const parray<Item>& xs, const Weight& weight) {
  assert(false);
  parray<long> w(xs.size() + 1);
  return w;
}

/*---------------------------------------------------------------------*/
/* Reduction level 4 */

namespace level4 {

template <class Item>
class slice_input {
public:
  
  using slice_type = slice<Item>;
  
  slice_type slice;
  
  slice_input(const parray<Item>& array)
  : slice(&array) { }
  
  slice_input(const slice_input& other)
  : slice(other.slice) { }
  
  bool can_split() const {
    return slice.hi - slice.lo > 1;
  }
  
  void split(slice_input& dst) {
    dst.slice = slice;
    long mid = (slice.lo + slice.hi) / 2;
    slice.hi = mid;
    dst.slice.lo = mid;
  }
  
};

} // end namespace
  
/*---------------------------------------------------------------------*/
/* Reduction level 3 */

namespace level3 {

template <
  class Item,
  class Output,
  class Lift_comp_rng,
  class Lift_idx_dst,
  class Seq_lift_dst
>
void reduce(const parray<Item>& xs,
            Output& out,
            Lift_comp_rng lift_comp_rng,
            Lift_idx_dst lift_idx_dst,
            Seq_lift_dst seq_lift_dst) {
  using input_type = level4::slice_input<Item>;
  input_type in(xs);
  auto lift_comp = [&] (input_type& in) {
    return lift_comp_rng(in.slice.lo, in.slice.hi);
  };
  auto convert = [&] (input_type& in, Output& out) {
    const parray<Item>& xs = *in.slice.pointer;
    for (long i = in.slice.lo; i < in.slice.hi; i++) {
      lift_idx_dst(i, xs[i], out);
    }
  };
  auto seq_convert = [&] (input_type& in, Output& out) {
    seq_lift_dst(in.slice.lo, in.slice.hi, out);
  };
  datapar::level4::reduce(in, out, lift_comp, convert, seq_convert);
}

} // end namespace
  
/*---------------------------------------------------------------------*/
/* Reduction level 2 */

namespace level2 {
  
template <
  class Item,
  class Result,
  class Combine,
  class Lift_comp_rng,
  class Lift_idx,
  class Seq_lift
>
Result reduce(const parray<Item>& xs,
              Result id,
              const Combine& combine,
              const Lift_comp_rng& lift_comp_rng,
              const Lift_idx& lift_idx,
              const Seq_lift& seq_lift) {
  using output_type = datapar::level3::cell<Result, Combine>;
  output_type out(id, combine);
  auto lift_idx_dst = [&] (long pos, const Item& x, output_type& out) {
    out.result = lift_idx(pos, x);
  };
  auto seq_lift_dst = [&] (long lo, long hi, output_type& out) {
    out.result = seq_lift(lo, hi);
  };
  level3::reduce(xs, out, lift_comp_rng, lift_idx_dst, seq_lift_dst);
  return out.result;
}
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Reduction level 1 */

namespace level1 {
  
template <
  class Item,
  class Result,
  class Combine,
  class Lift_idx
>
Result reducei(const parray<Item>& xs,
               Result id,
               Combine combine,
               Lift_idx lift_idx) {
  auto lift_comp_rng = [&] (long lo, long hi) {
    return hi - lo;
  };
  auto seq_lift = [&] (long lo, long hi) {
    Result r = id;
    for (long i = lo; i < hi; i++) {
      r = combine(r, lift_idx(i, xs[i]));
    }
    return r;
  };
  return level2::reduce(xs, id, combine, lift_comp_rng, lift_idx, seq_lift);
}
  
template <
  class Item,
  class Result,
  class Combine,
  class Lift
>
Result reduce(const parray<Item>& xs,
              Result id,
              Combine combine,
              Lift lift) {
  auto lift_idx = [&] (long pos, const Item& x) {
    return lift(x);
  };
  return reducei(xs, id, combine, lift_idx);
}
  
template <
  class Item,
  class Result,
  class Combine,
  class Lift_comp_idx,
  class Lift_idx
>
Result reducei(const parray<Item>& xs,
               Result id,
               Combine combine,
               Lift_comp_idx lift_comp_idx,
               Lift_idx lift_idx) {
  parray<long> w = weights(xs, [&] (long pos) {
    return lift_comp_idx(pos, xs[pos]);
  });
  auto lift_comp_rng = [&] (long lo, long hi) {
    long wrng = w[lo] - w[hi];
    return (long)(log(wrng) * wrng);
  };
  auto seq_lift = [&] (long lo, long hi) {
    Result r = id;
    for (long i = lo; i < hi; i++) {
      r = combine(r, lift_idx(i, xs[i]));
    }
    return r;
  };
  return level2::reduce(xs, id, combine, lift_comp_rng, lift_idx, seq_lift);
}

template <
  class Item,
  class Result,
  class Combine,
  class Lift_comp,
  class Lift
>
Result reduce(const parray<Item>& xs,
              Result id,
              Combine combine,
              Lift_comp lift_comp,
              Lift lift) {
  auto lift_comp_idx = [&] (long pos, const Item& x) {
    return lift_comp(x);
  };
  auto lift_idx = [&] (long pos, const Item& x) {
    return lift(x);
  };
  return reducei(xs, id, combine, lift_comp_idx, lift_idx);
}
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Reduction level 0 */

template <class Item, class Combine>
Item reduce(const parray<Item>& xs, Item id, Combine combine) {
  auto lift = [&] (const Item& x) {
    return x;
  };
  return level1::reduce(xs, id, combine, lift);
}

template <
  class Item,
  class Weight,
  class Combine
>
Item reduce(const parray<Item>& xs,
            Item id,
            Weight weight,
            Combine combine) {
  auto lift = [&] (const Item& x) {
    return x;
  };
  auto lift_comp = [&] (const Item& x) {
    return weight(x);
  };
  return level1::reduce(xs, id, combine, lift_comp, lift);
}
  
/***********************************************************************/
  
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PARRAY_PCTL_PARRAY_H_ */
