/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file datapar.hpp
 * \brief Data parallel operations
 *
 */

#include <limits.h>
#include <memory>
#include <utility>
#ifndef TARGET_MAC_OS
#include <malloc.h>
#endif

#include "weights.hpp"

#ifndef _PARRAY_PCTL_DATAPAR_H_
#define _PARRAY_PCTL_DATAPAR_H_

namespace pasl {
namespace pctl {
namespace datapar {

/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/* Level 4 reduction */
  
namespace level4 {
  
template <
  class Input,
  class Output,
  class Convert_comp,
  class Convert,
  class Seq_convert,
  class Granularity_controller
>
void reduce_rec(Input& in,
                Output& out,
                const Convert_comp& convert_comp,
                const Convert& convert,
                const Seq_convert& seq_convert,
                Granularity_controller& contr) {
  par::cstmt(contr, [&] { return convert_comp(in); }, [&] {
    if (! in.can_split()) {
      convert(in, out);
    } else {
      Input in2(in);
      Output out2(out);
      in.split(in2);
      par::fork2([&] {
        reduce_rec(in,  out,  convert_comp, convert, seq_convert, contr);
      }, [&] {
        reduce_rec(in2, out2, convert_comp, convert, seq_convert, contr);
      });
      out2.merge(out);
    }
  }, [&] {
    seq_convert(in, out);
  });
}

template <
  class Input,
  class Output,
  class Convert_comp,
  class Convert,
  class Seq_convert
>
class reduce_controller_type {
public:
  static controller_type contr;
};

template <
  class Input,
  class Output,
  class Convert_comp,
  class Convert,
  class Seq_convert
>
controller_type reduce_controller_type<Input,Output,Convert_comp,Convert,Seq_convert>::contr(
  "reduce"+sota<Input>()+sota<Output>()+sota<Convert_comp>()+sota<Convert>()+sota<Seq_convert>());

template <
  class Input,
  class Output,
  class Convert_comp,
  class Convert,
  class Seq_convert
>
void reduce(Input& in,
            Output& out,
            const Convert_comp& convert_comp,
            const Convert& convert,
            const Seq_convert& seq_convert) {
  using controller_type = reduce_controller_type<Input, Output, Convert_comp, Convert, Seq_convert>;
  reduce_rec(in, out, convert_comp, convert, seq_convert, controller_type::contr);
}
  
template <class Iter>
class random_access_iterator_input {
public:
  
  Iter lo;
  Iter hi;
  
  random_access_iterator_input(Iter lo, Iter hi)
  : lo(lo), hi(hi) { }
  
  bool can_split() const {
    return hi - lo >= 2;
  }
  
  void split(random_access_iterator_input& dst) {
    dst = *this;
    long n = hi - lo;
    assert(n >= 2);
    Iter mid = lo + (n / 2);
    hi = mid;
    dst.lo = mid;
  }
  
};

} // end namespace
  
/*---------------------------------------------------------------------*/
/* Level 3 reduction */

namespace level3 {
  
template <class Result, class Combine>
class cell {
public:
  
  Result result;
  Combine combine;
  
  cell(Result result, Combine combine)
  : result(result), combine(combine) { }
  
  cell(const cell& other)
  : combine(other.combine) { }
  
  void merge(cell& dst) {
    dst.result = combine(dst.result, result);
    Result empty;
    result = empty;
  }
  
};
  
} // end namespace
  
/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace


#endif /*! _PARRAY_PCTL_DATAPAR_H_ */