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

#ifndef _PCTL_DATAPAR_H_
#define _PCTL_DATAPAR_H_

namespace pasl {
namespace pctl {

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

namespace contr {

template <
  class Input,
  class Output,
  class Convert_comp,
  class Convert,
  class Seq_convert
>
class reduce {
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
controller_type reduce<Input,Output,Convert_comp,Convert,Seq_convert>::contr(
  "reduce"+sota<Input>()+sota<Output>()+sota<Convert_comp>()+sota<Convert>()+sota<Seq_convert>());

} // end namespace
  
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
  using controller_type = contr::reduce<Input, Output, Convert_comp, Convert, Seq_convert>;
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

template <class Body>
class tabulate_input {
public:
  
  long lo;
  long hi;
  Body body;
  
  tabulate_input(long lo, long hi, const Body& body)
  : lo(lo), hi(hi), body(body) { }
  
  tabulate_input(const tabulate_input& other)
  : lo(other.lo), hi(other.hi), body(other.body) { }
  
  bool can_split() const {
    return hi - lo >= 2;
  }
  
  void split(tabulate_input& dst) {
    dst.lo = lo;
    dst.hi = hi;
    long n = hi - lo;
    assert(n >= 2);
    long mid = lo + (n / 2);
    hi = mid;
    dst.lo = mid;
  }
  
};
  
template <class Chunked_sequence>
class chunked_sequence_input {
public:
  
  Chunked_sequence seq;
  
  chunked_sequence_input(Chunked_sequence& _seq) {
    _seq.swap(seq);
  }
  
  chunked_sequence_input(const chunked_sequence_input& other) {
    
  }
  
  bool can_split() const {
    return seq.size() >= 2;
  }
  
  void split(chunked_sequence_input& dst) {
    long n = seq.size() / 2;
    seq.split(seq.begin() + n, dst.seq);
  }
  
};
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Level 3 reduction */

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
  using input_type = level4::random_access_iterator_input<Iter>;
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
  level4::reduce(in, out, lift_comp, convert, seq_convert);
}

class trivial_output {
public:
  
  void merge(trivial_output&) {
    
  }
  
};
  
template <class Result, class Combine>
class cell_output {
public:
  
  Result result;
  Combine combine;
  
  cell_output(Result result, Combine combine)
  : result(result), combine(combine) { }
  
  cell_output(const cell_output& other)
  : combine(other.combine) { }
  
  void merge(cell_output& dst) {
    dst.result = combine(dst.result, result);
    Result empty;
    result = empty;
  }
  
};
  
template <class Chunked_sequence>
class chunkedseq_output {
public:
  
  using seq_type = Chunked_sequence;
  
  seq_type seq;
  
  chunkedseq_output() { }
  
  chunkedseq_output(const chunkedseq_output& other) { }
  
  void merge(chunkedseq_output& dst) {
    dst.seq.concat(seq);
  }
  
};

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
  using output_type = level3::cell_output<Result, Combine>;
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
  parray::parray<long> w = weights(hi-lo, [&] (long pos) {
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

#endif /*! _PCTL_DATAPAR_H_ */
