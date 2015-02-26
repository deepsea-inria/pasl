/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file ploop.hpp
 * \brief Parallel loops
 *
 */

#include "granularity.hpp"

#ifndef _PARRAY_PCTL_PLOOP_H_
#define _PARRAY_PCTL_PLOOP_H_

namespace pasl {
namespace pctl {
  
/***********************************************************************/
  
/*---------------------------------------------------------------------*/

namespace par = sched::granularity;
using controller_type = par::control_by_prediction;
//using controller_type = par::control_by_force_parallel;

template <class T>
std::string string_of_template_arg() {
  return std::string(typeid(T).name());
}

template <class T>
std::string sota() {
  return string_of_template_arg<T>();
}
  
/*---------------------------------------------------------------------*/
/* Parallel-for loops */

namespace range {
  
template <
  class Iter,
  class Body,
  class Comp_rng,
  class Seq_body_rng
>
class parallel_for_controller_type {
public:
  static controller_type contr;
};

template <
  class Iter,
  class Body,
  class Comp_rng,
  class Seq_body_rng
>
controller_type parallel_for_controller_type<Iter,Body,Comp_rng,Seq_body_rng>::contr(                                                                                       "parallel_for"+sota<Iter>()+sota<Body>()+sota<Comp_rng>()+sota<Seq_body_rng>());

template <
  class Iter,
  class Body,
  class Comp_rng,
  class Seq_body_rng
>
void parallel_for(Iter lo,
                  Iter hi,
                  const Comp_rng& comp_rng,
                  const Body& body,
                  const Seq_body_rng& seq_body_rng) {
  using controller_type = parallel_for_controller_type<Iter, Body, Comp_rng, Seq_body_rng>;
  par::cstmt(controller_type::contr, [&] { return comp_rng(lo, hi); }, [&] {
    long n = hi - lo;
    if (n <= 1) {
      body(lo);
    } else {
      Iter mid = lo + n;
      par::fork2([&] {
        parallel_for(lo, mid, comp_rng, body, seq_body_rng);
      }, [&] {
        parallel_for(mid, hi, comp_rng, body, seq_body_rng);
      });
    }
  }, [&] {
    seq_body_rng(lo, hi);
  });
}

template <class Iter, class Body, class Comp_rng>
void parallel_for(Iter lo, Iter hi, const Comp_rng& comp_rng, const Body& body) {
  auto seq_body_rng = [&] (Iter lo, Iter hi) {
    for (Iter i = lo; i != hi; i++) {
      body(i);
    }
  };
  parallel_for(lo, hi, comp_rng, body, seq_body_rng);
}

} // end namespace

template <class Iter, class Body, class Comp>
void parallel_for(Iter lo, Iter hi, const Comp& comp, const Body& body);
  
template <class Iter, class Body>
void parallel_for(Iter lo, Iter hi, const Body& body) {
  auto comp_rng = [&] (Iter lo, Iter hi) {
    return (long)(hi - lo);
  };
  range::parallel_for(lo, hi, comp_rng, body);
}
  
/***********************************************************************/

} // end namespace
} // end namespace

#endif /*! _PARRAY_PCTL_PLOOP_H_ */