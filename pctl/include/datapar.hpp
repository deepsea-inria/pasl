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
/* Level 7 reduction */

namespace level7 {

template <
  class Block,
  class Block_weight,
  class Block_convert,
  class Seq_block_convert,
  class Granularity_control
>
void reduce(Block in,
            Block out,
            Block_weight block_weight,
            Block_convert block_convert,
            Seq_block_convert seq_block_convert,
            Granularity_control contr) {
  par::cstmt(contr.nary_rec, [&] { return block_weight(in); }, [&] {
    if (! in.can_split()) {
      block_convert(in, out);
    } else {
      Block tmp;
      block_convert(in, tmp);
      reduce(tmp, out, block_weight, block_convert, seq_block_convert, contr);
    }
  }, seq_block_convert);
}

} // end namespace
  
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