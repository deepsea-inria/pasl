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

#include "parraybase.hpp"

#ifndef _PARRAY_PCTL_DATAPAR_H_
#define _PARRAY_PCTL_DATAPAR_H_

namespace pasl {
namespace pctl {
namespace datapar {

/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/* Reduction input */

namespace pa = parray;
  
template <class Item>
class parray_reduce_binary_input {
public:
  
  using value_type = Item;
  using parray_type = parray::parray<Item>;
  using slice_type = parray::slice<const parray_type*>;
  slice_type slice;
  
  parray_reduce_binary_input() { }
  
  parray_reduce_binary_input(slice_type slice)
  : slice(slice) { }
  
  void init(const parray_reduce_binary_input& other) {
    slice = other.slice;
  }
  
  bool can_split() const {
    return size() <= 1;
  }
  
  long size() const {
    return slice.hi - slice.lo;
  }
  
  void split(parray_reduce_binary_input& dest) {
    dest.slice = slice;
    long mid = (slice.lo + slice.hi) / 2;
    slice.hi = mid;
    dest.slice.lo = mid;
  }
  
};

template <class Item>
parray_reduce_binary_input<Item> create_parray_reduce_binary_input(pa::parray<Item>* pointer) {
  auto slice = pa::slice<pa::parray<Item>>(pointer);
  return parray_reduce_binary_input<Item>(slice);
}

template <class Item>
parray_reduce_binary_input<Item> create_parray_reduce_binary_input(pa::parray<Item>& array) {
  return create_parray_reduce_binary_input(&array);
}

template <class Item>
parray_reduce_binary_input<Item> create_parray_reduce_binary_input(const pa::parray<Item>* pointer) {
  auto slice = pa::slice<const pa::parray<Item>*>(pointer);
  return parray_reduce_binary_input<Item>(slice);
}

template <class Item>
parray_reduce_binary_input<Item> create_parray_reduce_binary_input(const pa::parray<Item>& array) {
  return create_parray_reduce_binary_input(&array);
}
  
template <class Item>
class parray_reduce_nary_input {
public:
  
  using value_type = Item;
  using parray_type = parray::parray<Item>;
  using slice_type = parray::slice<parray_type>;
  slice_type slice;
  
  parray_reduce_nary_input(slice_type slice)
  : slice(slice) { }
  
  void init(const parray_reduce_nary_input& other) {
    slice = other.slice;
  }
  
  bool can_split() const {
    return size() <= nb_blocks();
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
  
  void split(parray_reduce_nary_input& dest) {
    dest.slice = slice;
    long mid = (slice.lo + slice.hi) / 2;
    slice.hi = mid;
    dest.slice.lo = mid;
  }
  
  parray_reduce_nary_input range(long lo2, long hi2) const {
    parray_reduce_nary_input dest;
    assert(lo2 >= lo2);
    assert(hi2 <= slice.hi);
    dest.slice = slice;
    dest.slice.lo = lo2;
    dest.slice.hi = hi2;
    return dest;
  }
  
  void range(parray::parray<value_type>&, long lo2, long hi2) const {
    return range(lo2, hi2);
  }
  
  parray_reduce_binary_input<Item> binary() const {
    return parray_reduce_binary_input<Item>(slice);
  }
  
};

template <class Item>
parray_reduce_nary_input<Item> create_parray_reduce_nary_input(pa::parray<Item>* pointer) {
  return parray_reduce_nary_input<Item>(pa::slice<pa::parray<Item>>(pointer));
}

template <class Item>
parray_reduce_nary_input<Item> create_parray_reduce_nary_input(pa::parray<Item>& array) {
  return create_parray_reduce_nary_input(&array);
}

/*---------------------------------------------------------------------*/
/* Reduction output */
  
template <class Item, class Merge>
class cell_reduce_output {
public:
  
  Item item;
  Merge _merge;
  
  cell_reduce_output(cell_reduce_output& other)
  : item(other.item), _merge(other._merge) { }
  
  cell_reduce_output(Item item, const Merge& _merge)
  : item(item), _merge(_merge) { }
  
  void init(cell_reduce_output& other) {
    
  }
  
  void merge(const cell_reduce_output& other) {
    _merge(other.item, item);
  }
  
};
  
/*---------------------------------------------------------------------*/
/* Binary reduction */

template <
  class Input,
  class Output,
  class Input_base,
  class Input_compl,
  class Granularity_control
>
void reduce_binary_rec(Input& in,
                       Output& out,
                       const Input_base& input_base,
                       const Input_compl& input_compl,
                       Granularity_control& contr) {
  par::cstmt(contr, [&] { return input_compl(in); }, [&] {
    if (! in.can_split()) {
      input_base(in, out);
    } else {
      Input in2;
      Output out2(out);
      in2.init(in);
      out2.init(out);
      in.split(in2);
      par::fork2([&] {
        reduce_binary_rec(in,  out,  input_base, input_compl, contr);
      }, [&] {
        reduce_binary_rec(in2, out2, input_base, input_compl, contr);
      });
      out.merge(out2);
    }
  }, [&] {
    input_base(in, out);
  });
}
  
template <
  class Input,
  class Output,
  class Input_base,
  class Input_compl
>
class reduce_binary_controller_type {
public:
  static controller_type contr;
};
  
template <
  class Input,
  class Output,
  class Input_base,
  class Input_compl
>
controller_type reduce_binary_controller_type<Input,Output,Input_base,Input_compl>::contr(
"reduce_binary"+sota<Input>()+sota<Output>()+sota<Input_base>()+sota<Input_compl>());

template <
  class Input,
  class Output,
  class Input_base,
  class Input_compl
>
void reduce_binary(Input& in,
                   Output& out,
                   const Input_base& input_base,
                   const Input_compl& input_compl) {
  using controller_type = reduce_binary_controller_type<Input, Output, Input_base, Input_compl>;
  reduce_binary_rec(in, out, input_base, input_compl, controller_type::contr);
}
  
/*---------------------------------------------------------------------*/
/* Specified-arity reduction */
  
template <
  class Input,
  class Output,
  class Prepare_input,
  class Output_base,
  class Output_compl,
  class Weighted_output_compl,
  class Compute_output_weights,
  class Granularity_control
>
void reduce_nary_rec(Input in,
                     Output& out,
                     const Prepare_input& prepare_input,
                     const Output_base& output_base,
                     const Output_compl& output_compl,
                     const Weighted_output_compl& weighted_output_compl,
                     const Compute_output_weights& compute_output_weights,
                     Granularity_control contr) {
  par::cstmt(contr->nary_rec, [&] { return output_compl(in); }, [&] {
    if (! in.can_split()) {
      output_base(in, out);
    } else {
      long n = in.size();
      long b = in.nb_blocks();
      long k = in.block_size();
      parray::parray<Input> ins;
      prepare_input(n, b, k, ins);
      parray::parray<Output> outs(n, out);
      parray::parray<long> weights;
      compute_output_weights(in, weights);
      auto loop_compl = [&] (long lo, long hi) {
        return weighted_output_compl(weights, lo, hi);
      };
      par::parallel_for(contr->loop_rec, loop_compl, 0l, b, [&] (long i) {
        long lo = i * k;
        long hi = std::min(lo + k, n);
        auto slice = in.binary(in.range(ins, lo, hi));
        reduce_binary_rec(slice, outs[i], output_base, output_compl, contr->binary);
      });
      auto slice = create_parray_reduce_nary_input(outs);
      reduce_nary_rec(slice, out, prepare_input, output_base, output_compl,
                      weighted_output_compl, compute_output_weights, contr);
    }
  }, [&] {
    output_base(in, out);
  });
}
  
template <
  class Input,
  class Output,
  class Input_base,
  class Prepare_input,
  class Input_compl,
  class Weighted_input_compl,
  class Compute_input_weights,
  class Output_base,
  class Output_compl,
  class Weighted_output_compl,
  class Compute_output_weights
>
class reduce_nary_controller_type {
public:
  static controller_type binary;
  static controller_type nary_rec;
  static loop_controller_type loop_rec;
  static controller_type nary;
  static loop_controller_type loop;
};
  
template <
  class Input,
  class Output,
  class Input_base,
  class Prepare_input,
  class Input_compl,
  class Weighted_input_compl,
  class Compute_input_weights,
  class Output_base,
  class Output_compl,
  class Weighted_output_compl,
  class Compute_output_weights
>
void reduce_nary(Input& in,
                 Output& out,
                 const Input_base& input_base,
                 const Prepare_input& prepare_input,
                 const Input_compl& input_compl,
                 const Weighted_input_compl& weighted_input_compl,
                 const Compute_input_weights& compute_input_weights,
                 const Output_base& output_base,
                 const Weighted_output_compl& weighted_output_compl,
                 const Output_compl& output_compl,
                 const Compute_output_weights& compute_output_weights) {
  using controller_type = reduce_nary_controller_type<Input, Output,
  Input_base, Prepare_input, Input_compl, Weighted_input_compl, Compute_input_weights,
  Output_base, Output_compl, Weighted_output_compl, Compute_output_weights>;
  controller_type contr;
  par::cstmt(controller_type::nary, [&] { return input_compl(in); }, [&] {
    if (! in.can_split()) {
      input_base(in, out);
    } else {
      long n = in.size();
      long b = in.nb_blocks();
      long k = in.block_size();
      parray::parray<Input> ins;
      prepare_input(n, b, k, ins);
      parray::parray<Output> outs(n, out);
      parray::parray<long> weights;
      compute_input_weights(n, b, k, weights);
      auto loop_compl = [&] (long lo, long hi) {
        return weighted_input_compl(weights, lo, hi);
      };
      par::parallel_for(controller_type::loop, loop_compl, 0l, b, [&] (long i) {
        long lo = i * k;
        long hi = std::min(lo + k, n);
        auto slice = in.range(ins, lo, hi);
        reduce_nary_rec(slice, outs[i], prepare_input, output_base, output_compl,
                        weighted_output_compl, compute_output_weights, contr);
      });
      auto slice = create_parray_reduce_nary_input(outs);
      auto prepare_output = [&] (long,long,long,parray::parray<Output>&) {};
      reduce_nary_rec(slice, out, prepare_output, output_base, output_compl,
                      weighted_output_compl, compute_output_weights, &contr);
    }
  }, [&] {
    input_base(in, out);
  });
}

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace


#endif /*! _PARRAY_PCTL_DATAPAR_H_ */