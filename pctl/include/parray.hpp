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

namespace level3 {

  template <
    class Item,
    class Output,
    class Base,
    class Base_compl
  >
  void reduce(const parray<Item>& xs,
              Output& out,
              const Base& base,
              const Base_compl& base_compl) {
    using input_type = datapar::parray_reduce_binary_input<Item>;
    input_type in = datapar::create_parray_reduce_binary_input(xs);
    datapar::reduce_binary(in, out, base, base_compl);
  }
  
} // end namespace
  
namespace level2 {
  
  template <
    class Item,
    class Result,
    class Assoc_oper,
    class Liftn,
    class Lift_compl
  >
  Result reduce(const parray<Item>& xs,
                Result id,
                const Assoc_oper& assoc_oper,
                const Liftn liftn,
                const Lift_compl& lift_compl) {
    using input_type = datapar::parray_reduce_binary_input<Item>;
    auto merge = [&] (const Result& src, Result& dst) {
      dst = assoc_oper(dst, src);
    };
    using output_type = datapar::cell_reduce_output<Result, typeof(merge)>;
    auto base = [&] (input_type& in, output_type& out) {
      out.item = liftn(in.slice.lo, in.slice.hi);
    };
    auto base_compl = [&] (const input_type& in) {
      return lift_compl(in.slice.lo, in.slice.hi);
    };
    output_type out(id, merge);
    level3::reduce(xs, out, base, base_compl);
    return out.item;
  }
  
} // end namespace

namespace level1 {
  
  template <
    class Item,
    class Result,
    class Assoc_oper,
    class Lift,
    class Lift_compl
  >
  Result reduce(const parray<Item>& xs,
                Result id,
                const Assoc_oper& assoc_oper,
                const Lift lift,
                const Lift_compl& lift_compl) {
    auto liftn = [&] (long lo, long hi) {
      Item x = id;
      const Item* first = &xs[0];
      for (long i = lo; i < hi; i++) {
        x = assoc_oper(x, first[i]);
      }
      return x;
    };
    return level2::reduce(xs, id, assoc_oper, liftn, lift_compl);
  }
  
} // end namespace

template <
  class Item,
  class Assoc_oper,
  class Assoc_oper_compl
>
Item reduce(const parray<Item>& xs,
            Item id,
            const Assoc_oper& assoc_oper,
            const Assoc_oper_compl& assoc_oper_compl) {
  auto lift = [&] (const Item& x) {
    return x;
  };
  auto lift_compl = [&] (long lo, long hi) {
    return assoc_oper_compl(lo, hi);
  };
  return level1::reduce(xs, id, assoc_oper, lift, lift_compl);
}

template <
  class Item,
  class Assoc_oper
>
Item reduce(const parray<Item>& xs, Item id, const Assoc_oper& assoc_oper) {
  auto assoc_oper_compl = [&] (long lo, long hi) {
    return hi - lo;
  };
  return reduce(xs, id, assoc_oper, assoc_oper_compl);
}
  
/***********************************************************************/
  
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PARRAY_PCTL_PARRAY_H_ */
