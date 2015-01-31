/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file parray.hpp
 * \brief Array-based implementation of sequences
 *
 */

#include <iostream>
#include <limits.h>
#include <vector>
#include <memory>
#include <utility>
#ifndef TARGET_MAC_OS
#include <malloc.h>
#endif
#include <cmath>

#include "granularity.hpp"

#ifndef _PARRAY_DATA_PASL_H_
#define _PARRAY_DATA_PASL_H_

/***********************************************************************/

namespace pasl {
namespace data {
namespace parray {
  
namespace par = sched::granularity;
using controller_type = par::control_by_prediction;
//using controller_type = par::control_by_force_parallel;
using loop_controller_type = par::loop_by_eager_binary_splitting<controller_type>;
  
template <class T>
std::string string_of_template_arg() {
  return std::string(typeid(T).name());
}

template <class T>
std::string sota() {
  return string_of_template_arg<T>();
}

/*---------------------------------------------------------------------*/
/* Primitive memory operations */

namespace prim {
  
template <class T>
T* alloc_array(long n) {
  return (T*)malloc(n*sizeof(T));
}
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Parallel array */
  
template <class Item>
class parray {
public:
  
  using value_type = Item;
  
private:
  
  class Deleter {
  public:
    void operator()(value_type* ptr) {
      free(ptr);
    }
  };
  
  std::unique_ptr<value_type[], Deleter> ptr;
  long sz = -1l;
  
  void alloc() {
    assert(sz >= 0);
    value_type* p = prim::alloc_array<value_type>(sz);
    assert(p != nullptr);
    ptr.reset(p);
  }
  
  void check(long i) const {
    assert(ptr != nullptr);
    assert(i >= 0);
    assert(i < sz);
  }
  
public:
  
  parray(long sz = 0)
  : sz(sz) {
    alloc();
  }
  
  parray(std::initializer_list<value_type> xs)
  : sz(xs.size()) {
    alloc();
    long i = 0;
    for (auto it = xs.begin(); it != xs.end(); it++)
      ptr[i++] = *it;
  }
  
  // To disable copy semantics, we disable:
  parray(const parray& other);            //   1. copy constructor
  parray& operator=(const parray& other); //   2. assign-by-copy operator
  
  parray& operator=(parray&& other) {
    ptr = std::move(other.ptr);
    sz = std::move(other.sz);
    other.sz = 0l; // redundant?
    return *this;
  }
  
  value_type& operator[](long i) {
    check(i);
    return ptr[i];
  }
  
  const value_type& operator[](long i) const {
    check(i);
    return ptr[i];
  }
  
  long size() const {
    return sz;
  }
  
  void swap(parray& other) {
    ptr.swap(other.ptr);
    std::swap(sz, other.sz);
  }
  
};
  
template <class Item>
std::ostream& operator<<(std::ostream& out, const parray<Item>& xs) {
  out << "{ ";
  size_t sz = xs.size();
  for (long i = 0; i < sz; i++) {
    out << xs[i];
    if (i+1 < sz)
      out << ", ";
  }
  out << " }";
  return out;
}
  
/*---------------------------------------------------------------------*/
/* Data parallel operations */

template <class PArray>
class parray_slice {
public:

  const PArray* array;
  long lo;
  long hi;
  
  parray_slice()
  : lo(0), hi(0), array(nullptr) {}
  
  parray_slice(const PArray* array)
  : lo(0), hi(0), array(array) {
    lo = 0;
    hi = array->size();
  }
  
  void initialize(const parray_slice& other) {
    *this = other;
  }
  
  bool can_split() const {
    return size() <= 1;
  }
  
  long size() const {
    return hi - lo;
  }
  
  long nb_blocks() const {
    return 1 + ((size() - 1) / block_size());
  }
  
  long block_size() const {
    return (long)std::pow(size(), 0.5);
  }
  
  void split(parray_slice& destination) {
    destination = *this;
    long mid = (lo + hi) / 2;
    hi = mid;
    destination.lo = mid;
  }
  
  parray_slice slice(long lo2, long hi2) {
    assert(lo2 >= lo2);
    assert(hi2 <= hi);
    parray_slice tmp = *this;
    tmp.lo = lo2;
    tmp.hi = hi2;
    return tmp;
  }
  
};

template <class PArray>
parray_slice<PArray> create_parray_slice(const PArray& xs) {
  parray_slice<PArray> slice(&xs);
  return slice;
}
  
template <class Item, class Merge_fct>
class cell {
public:
  
  Item item;
  Merge_fct merge_fct;
  
  cell(cell& other)
  : item(other.item), merge_fct(other.merge_fct) { }
  
  cell(Item item, const Merge_fct& merge_fct)
  : item(item), merge_fct(merge_fct) { }
  
  void initialize(cell& other) {
    
  }
  
  void merge(const cell& other) {
    merge_fct(other.item, item);
  }
  
};

template <
  class Input,
  class Sequential_fct,
  class Complexity_fct,
  class Granularity_control,
  class Output
  >
void reduce_binary(Input in,
                   const Sequential_fct& sequential_fct,
                   const Complexity_fct& complexity_fct,
                   Granularity_control* contr,
                   Output& out) {
  par::cstmt(*contr, [&] { return complexity_fct(in); }, [&] {
    if (! in.can_split()) {
      sequential_fct(in, out);
    } else {
      Input in2(in);
      Output out2(out);
      in.initialize(in2);
      out.initialize(out2);
      in.split(in2);
      par::fork2([&] {
        reduce_binary(in, sequential_fct, complexity_fct, contr, out);
      }, [&] {
        reduce_binary(in2, sequential_fct, complexity_fct, contr, out2);
      });
      out.merge(out2);
    }
  }, [&] {
    sequential_fct(in, out);
  });
}
  

template <
  class Input,
  class Sequential_fct,
  class Complexity_fct,
//  class Compute_weights_fct,
//  class Weighted_complexity_fct,
  class Granularity_control,
  class Output
>
void reduce_nary(Input in,
                 const Sequential_fct& sequential_fct,
                 const Complexity_fct& complexity_fct,
//                 const Compute_weights_fct& compute_weights_fct,
//                 const Weighted_complexity_fct& weighted_complexity_fct,
                 Granularity_control contr,
                 Output& out) {
  /*
  par::cstmt(*contr.nary, [&] { return complexity_fct(in); }, [&] {
    if (! in.can_split()) {
      sequential_fct(in, out);
    } else {
      long n = in.size();
      long b = in.nb_blocks();
      long k = in.block_size();
      parray<Output> outs;
//      parray<long> weights;
//      compute_weights_fct(b, weights);
      par::parallel_for(*contr.loop,
//                        [&] (long lo, long hi) { return weighted_complexity_fct(weights, lo, hi); },
                        0l, b,
                        [&] (long i) {
                          long lo = i * k;
                          long hi = std::min(lo + k, n);
                          reduce_binary(in.slice(lo, hi), sequential_fct, complexity_fct, contr.binary, outs[i]);
                        });
      parray_slice<parray<Output>> slice = create_parray_slice(outs);
      auto reduce_seq = [&] (parray_slice<parray<Output>>& in, Output& out) {
        for (long i = in.lo; i < in.hi; i++) {
          out.merge_fct(outs[i].item, out.item);
        }
      };
      auto recur_complexity_fct = [&] (typeof(slice) slice) {
        return slice.size();
      };
      auto compl_fct2 = [&] (Input) {
        return 0;
      };
      //      reduce_nary(slice, reduce_seq, recur_complexity_fct, contr, out);
    }
  }, [&] {
    sequential_fct(in, out);
  });
*/
        auto sequential_fct2 = [&] (Input, Output) {
      };
      reduce_nary(in, sequential_fct, complexity_fct, contr, out);

}

template <class Item, class Lift_fct, class Output>
class reduce_controller_type {
public:
  static controller_type binary;
  static controller_type nary;
  static loop_controller_type loop;
};

template <class Item, class Lift_fct, class Output>
controller_type reduce_controller_type<Item,Lift_fct,Output>::binary("reduce_binary_"
                                                                           + sota<Item>()+sota<Lift_fct>()+sota<Output>());
template <class Item, class Lift_fct, class Output>
controller_type reduce_controller_type<Item,Lift_fct,Output>::nary("reduce_nary_"
                                                                         + sota<Item>()+sota<Lift_fct>()+sota<Output>());
template <class Item, class Lift_fct, class Output>
loop_controller_type reduce_controller_type<Item,Lift_fct,Output>::loop("reduce_loop_"
                                                                              + sota<Item>()+sota<Lift_fct>()+sota<Output>());
  
class reduce_controller_alias_type {
public:

  reduce_controller_alias_type(controller_type* binary, controller_type* nary, loop_controller_type* loop)
    : binary(binary), nary(nary), loop(loop) { }

  controller_type* binary;
  controller_type* nary;
  loop_controller_type* loop;
  
};
  
template <class Item, class Liftn_fct, class Output>
void reduce_skel(const parray<Item>& xs, const Liftn_fct& liftn_fct, Output& out) {
  using slice_type = parray_slice<parray<Item>>;
  slice_type slice = create_parray_slice(xs);
  auto sequential_fct = [&] (slice_type in, Output& out) {
    Item x = liftn_fct(*in.array, in.lo, in.hi);
    out.merge_fct(x, out.item);
  };
  auto complexity_fct = [&] (slice_type in) {
    return in.size();
  };
  /*
  auto compute_weights_fct = [&] (long b, const parray<long>& weights) {
    
  };
  auto weighted_complexity_fct = [&] (const parray<long>& weights, long lo, long hi) {
    return hi - lo;
  }; */
  using rct = reduce_controller_type<Item, Liftn_fct, Output>;
  reduce_controller_alias_type gc2(&rct::binary, &rct::nary, &rct::loop);
  reduce_nary(slice, sequential_fct, complexity_fct, gc2, out);
  //  reduce_binary(slice, sequential_fct, complexity_fct, gc2.binary, out);
}

template <class Item, class Associative_operation>
Item reduce(const parray<Item>& xs, Item id, const Associative_operation& op) {
  auto merge_fct = [&] (Item src, Item& dst) {
    dst = op(src, dst);
  };
  auto liftn_fct = [&] (const parray<Item>& xs, long lo, long hi) {
    Item x = id;
    for (long i = lo; i < hi; i++) {
      x = op(x, xs[i]);
    }
    return x;
  };
  using output_type = cell<Item, typeof(merge_fct)>;
  output_type out(0, merge_fct);
  reduce_skel(xs, liftn_fct, out);
  return out.item;
}
  
template <class Number>
Number sum(const parray<Number>& xs) {
  auto add_fct = [&] (Number x, Number y) {
    return x + y;
  };
  return reduce(xs, Number(0), add_fct);
}

} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PARRAY_DATA_PASL_H_ */
