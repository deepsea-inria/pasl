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
  
using scan_type = enum { inclusive_scan, exclusive_scan };
  
/*---------------------------------------------------------------------*/
/* Level 4 reduction */
  
namespace level4 {

template <
  class Input,
  class Output,
  class Result,
  class Convert_reduce_comp,
  class Convert_reduce,
  class Seq_convert_reduce,
  class Granularity_controller
>
void reduce_rec(Input& in,
                const Output& out,
                Result& id,
                Result& dst,
                const Convert_reduce_comp& convert_reduce_comp,
                const Convert_reduce& convert_reduce,
                const Seq_convert_reduce& seq_convert_reduce,
                Granularity_controller& contr) {
  par::cstmt(contr, [&] { return convert_reduce_comp(in); }, [&] {
    if (! in.can_split()) {
      convert_reduce(in, dst);
    } else {
      Input in2(in);
      in.split(in2);
      Result dst2;
      out.init(dst2);
      par::fork2([&] {
        reduce_rec(in,  out, id, dst,  convert_reduce_comp, convert_reduce, seq_convert_reduce, contr);
      }, [&] {
        reduce_rec(in2, out, id, dst2, convert_reduce_comp, convert_reduce, seq_convert_reduce, contr);
      });
      out.merge(dst2, dst);
    }
  }, [&] {
    seq_convert_reduce(in, dst);
  });
}

namespace contr {

template <
  class Input,
  class Output,
  class Result,
  class Convert_reduce_comp,
  class Convert_reduce,
  class Seq_convert_reduce
>
class reduce {
public:
  static controller_type contr;
};

template <
  class Input,
  class Output,
  class Result,
  class Convert_reduce_comp,
  class Convert_reduce,
  class Seq_convert_reduce
>
controller_type reduce<Input,Output,Result,Convert_reduce_comp,Convert_reduce,Seq_convert_reduce>::contr(
  "reduce"+sota<Input>()+sota<Output>()+sota<Result>()+sota<Convert_reduce_comp>()+
           sota<Convert_reduce>()+sota<Seq_convert_reduce>());

} // end namespace
  
template <
  class Input,
  class Output,
  class Result,
  class Convert_reduce_comp,
  class Convert_reduce,
  class Seq_convert_reduce
>
void reduce(Input& in,
            Output& out,
            Result& id,
            Result& dst,
            const Convert_reduce_comp& convert_reduce_comp,
            const Convert_reduce& convert_reduce,
            const Seq_convert_reduce& seq_convert_reduce) {
  using controller_type = contr::reduce<Input, Output, Result, Convert_reduce_comp, Convert_reduce, Seq_convert_reduce>;
  reduce_rec(in, out, id, dst, convert_reduce_comp, convert_reduce, seq_convert_reduce, controller_type::contr);
}

template <
  class In_iter,
  class Out_iter,
  class Output,
  class Result,
  class Convert
>
void scan_seq(In_iter in_lo,
              In_iter in_hi,
              Out_iter out_lo,
              const Output& out,
              const Result& id,
              const Convert& convert,
              scan_type st) {
  Result x;
  out.copy(id, x);
  In_iter in_it = in_lo;
  Out_iter out_it = out_lo;
  if (st == exclusive_scan) {
    for (; in_it != in_hi; in_it++, out_it++) {
      Result tmp1; // required because input and output ranges can overlap
      out.copy(x, tmp1);
      Result tmp2;
      convert(in_it, tmp2);
      out.merge(tmp2, x);
      out.copy(tmp1, *out_it);
    }
  } else { // (st == inclusive_scan)
    for (; in_it != in_hi; in_it++, out_it++) {
      Result tmp;
      convert(in_it, tmp);
      out.merge(tmp, x);
      out.copy(x, *out_it);
    }
  }
}
  
template <class In_iter, class Out_iter, class Output, class Result>
void scan_seq(In_iter in_lo,
              In_iter in_hi,
              Out_iter out_lo,
              const Output& out,
              const Result& id,
              scan_type st) {
  auto convert = [&] (const Result* src, Result& dst) {
    out.copy(*src, dst);
  };
  scan_seq(in_lo, in_hi, out_lo, out, id, convert, st);
}

template <class Result, class Output>
void scan_seq(const parray::parray<Result>& ins,
              typename parray::parray<Result>::iterator outs_lo,
              const Output& out,
              const Result& id,
              scan_type st) {
  scan_seq(ins.cbegin(), ins.cend(), outs_lo, out, id, st);
}

namespace contr {
  
template <class Result, class Output, class Merge_comp>
class scan_rec {
public:
  static controller_type contr;
};

template <class Result, class Output, class Merge_comp>
controller_type scan_rec<Result, Output, Merge_comp>::contr(
  "scan_rec"+sota<Result>()+sota<Output>()+sota<Merge_comp>());

template <
  class Input,
  class Output,
  class Result,
  class Output_iter,
  class Convert_reduce_comp,
  class Convert_reduce,
  class Convert_scan,
  class Seq_convert_scan
>
class scan {
public:
  static controller_type contr;
};

template <
  class Input,
  class Output,
  class Result,
  class Output_iter,
  class Convert_reduce_comp,
  class Convert_reduce,
  class Convert_scan,
  class Seq_convert_scan
>
controller_type scan<Input,Output,Result,Output_iter,Convert_reduce_comp,Convert_reduce,Convert_scan,Seq_convert_scan>::contr(
  "scan"+sota<Input>()+sota<Output>()+sota<Result>()+sota<Output_iter>()+sota<Convert_reduce_comp>()+
         sota<Convert_reduce>()+sota<Convert_scan>()+sota<Seq_convert_scan>());
  
} // end namespace

const long Scan_branching_factor = 1024;

long get_nb_blocks(long k, long n) {
  return 1 + ((n - 1) / k);
} 
  
std::pair<long,long> get_rng(long k, long n, long i) {
  long lo = i * k;
  long hi = std::min(lo + k, n);
  return std::make_pair(lo, hi);
}
  
template <class Result, class Output, class Merge_comp>
void scan_rec(const parray::parray<Result>& ins,
              typename parray::parray<Result>::iterator outs_lo,
              const Output& out,
              const Result& id,
              const Merge_comp& merge_comp,
              scan_type st) {
  using controller_type = contr::scan_rec<Result, Output, Merge_comp>;
  const long k = Scan_branching_factor;
  long n = ins.size();
  long m = get_nb_blocks(k, n);
  auto loop_comp = [&] (long i) {
    auto beg = ins.cbegin();
    long lo = get_rng(k, n, i).first;
    long hi = get_rng(k, n, i).second;
    return merge_comp(beg+lo, beg+hi);
  };
  par::cstmt(controller_type::contr, [&] { return merge_comp(ins.cbegin(), ins.cend()); }, [&] {
    if (n <= k) {
      scan_seq(ins, outs_lo, out, id, st);
    } else {
      parray::parray<Result> partials(m);
      parallel_for(0l, m, loop_comp, [&] (long i) {
        auto beg = ins.cbegin();
        long lo = get_rng(k, n, i).first;
        long hi = get_rng(k, n, i).second;
        out.merge(beg+lo, beg+hi, partials[i]);
      });
      parray::parray<Result> scans(m);
      scan_rec(partials, scans.begin(), out, id, merge_comp, exclusive_scan);
      parallel_for(0l, m, loop_comp, [&] (long i) {
        auto ins_beg = ins.cbegin();
        long lo = get_rng(k, n, i).first;
        long hi = get_rng(k, n, i).second;
        scan_seq(ins_beg+lo, ins_beg+hi, outs_lo+lo, out, scans[i], st);
      });
    }
  }, [&] {
    scan_seq(ins, outs_lo, out, id, st);
  });
}

template <
  class Input,
  class Output,
  class Result,
  class Output_iter,
  class Merge_comp,
  class Convert_reduce_comp,
  class Convert_reduce,
  class Convert_scan,
  class Seq_convert_scan
>
void scan(Input& in,
          const Output& out,
          Result& id,
          Output_iter outs_lo,
          const Merge_comp& merge_comp,
          const Convert_reduce_comp& convert_reduce_comp,
          const Convert_reduce& convert_reduce,
          const Convert_scan& convert_scan,
          const Seq_convert_scan& seq_convert_scan,
          scan_type st) {
  using controller_type = contr::scan<Input, Output, Result, Output_iter,
                                      Convert_reduce_comp, Convert_reduce,
                                      Convert_scan, Seq_convert_scan>;
  const long k = Scan_branching_factor;
  long n = in.size();
  long m = get_nb_blocks(k, n);
  auto loop_comp = [&] (long i) {
    long lo = get_rng(k, n, i).first;
    long hi = get_rng(k, n, i).second;
    return convert_reduce_comp(lo, hi);
  };
  par::cstmt(controller_type::contr, [&] { return convert_reduce_comp(0l, n); }, [&] {
    if (n <= k) {
      convert_scan(in, outs_lo);
    } else {
      parray::parray<Input> splits = in.split(m);
      parray::parray<Result> partials(m);
      parallel_for(0l, m, loop_comp, [&] (long i) {
        long lo = get_rng(k, n, i).first;
        long hi = get_rng(k, n, i).second;
        Input in2 = in.slice(splits, lo, hi);
        convert_reduce(in2, partials[i]);
      });
      parray::parray<Result> scans(m);
      scan_rec(partials, scans.begin(), out, id, merge_comp, exclusive_scan);
      parallel_for(0l, m, loop_comp, [&] (long i) {
        long lo = get_rng(k, n, i).first;
        long hi = get_rng(k, n, i).second;
        Input in2 = in.slice(splits, lo, hi);
        scan(in2, out, scans[i], outs_lo+lo, merge_comp, convert_reduce_comp, convert_reduce, convert_scan, seq_convert_scan, st);
      });
    }
  }, [&] {
    seq_convert_scan(in, outs_lo);
  });
}
  
template <class Input_iter>
class random_access_iterator_input {
public:
  
  using self_type = random_access_iterator_input;
  using array_type = parray::parray<self_type>;
  
  Input_iter lo;
  Input_iter hi;
  
  random_access_iterator_input() { }
  
  random_access_iterator_input(Input_iter lo, Input_iter hi)
  : lo(lo), hi(hi) { }
  
  bool can_split() const {
    return size() >= 2;
  }
  
  long size() const {
    return hi - lo;
  }
  
  void split(random_access_iterator_input& dst) {
    dst = *this;
    long n = size();
    assert(n >= 2);
    Input_iter mid = lo + (n / 2);
    hi = mid;
    dst.lo = mid;
  }
  
  array_type split(long) {
    array_type tmp;
    return tmp;
  }
  
  self_type slice(const array_type&, long _lo, long _hi) {
    self_type tmp(lo + _lo, lo + _hi);
    return tmp;
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
  
  chunked_sequence_input(const chunked_sequence_input& other) { }
  
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
  class Input_iter,
  class Output,
  class Result,
  class Lift_comp_rng,
  class Lift_idx_dst,
  class Seq_lift_dst
>
void reduce(Input_iter lo,
            Input_iter hi,
            const Output& out,
            Result& id,
            Result& dst,
            const Lift_comp_rng& lift_comp_rng,
            const Lift_idx_dst& lift_idx_dst,
            const Seq_lift_dst& seq_lift_dst) {
  using input_type = level4::random_access_iterator_input<Input_iter>;
  input_type in(lo, hi);
  auto convert_reduce_comp = [&] (input_type& in) {
    return lift_comp_rng(in.lo, in.hi);
  };
  auto convert_reduce = [&] (input_type& in, Result& dst) {
    long i = 0;
    for (Input_iter it = in.lo; it != in.hi; it++, i++) {
      lift_idx_dst(i, it, dst);
    }
  };
  auto seq_convert_reduce = [&] (input_type& in, Result& dst) {
    seq_lift_dst(in.lo, in.hi, dst);
  };
  level4::reduce(in, out, id, dst, convert_reduce_comp, convert_reduce, seq_convert_reduce);
}
  
template <
  class Input_iter,
  class Output,
  class Result,
  class Output_iter,
  class Lift_comp_rng,
  class Lift_idx_dst,
  class Seq_lift_dst
>
void scan(Input_iter lo,
          Input_iter hi,
          const Output& out,
          Result& id,
          Output_iter outs_lo,
          Lift_comp_rng lift_comp_rng,
          Lift_idx_dst lift_idx_dst,
          Seq_lift_dst seq_lift_dst,
          scan_type st) {
  using input_type = level4::random_access_iterator_input<Input_iter>;
  input_type in(lo, hi);
  auto convert_reduce_comp = [&] (long lo, long hi) {
    return lift_comp_rng(in.lo+lo, in.lo+hi);
  };
  auto convert_reduce = [&] (input_type& in, Result& dst) {
    long i = 0;
    for (Input_iter it = in.lo; it != in.hi; it++, i++) {
      lift_idx_dst(i, it, dst);
    }
  };
  auto convert_scan = [&] (input_type& in, Output_iter outs_lo) {
    long i = 0;
    level4::scan_seq(in.lo, in.hi, outs_lo, out, id, [&] (Input_iter src, Result& dst) {
      lift_idx_dst(i, src, dst);
    }, st);
  };
  auto seq_convert_scan = [&] (input_type& in, Output_iter outs_lo) {
    seq_lift_dst(in.lo, in.hi, outs_lo);
  };
  auto merge_comp = [&] (const Result* lo, const Result* hi) {
    return hi-lo; // later: generalize
  };
  level4::scan(in, out, id, outs_lo, merge_comp, convert_reduce_comp, convert_reduce, convert_scan, seq_convert_scan, st);
}
  
template <class T>
class trivial_output {
public:
  
  using result_type = T;
  
  void init(T&) const {
    
  }
  
  void merge(T&, T&) const {
    
  }
  
};
  
template <class Result, class Combine>
class cell_output {
public:
  
  using result_type = Result;
  using array_type = parray::parray<result_type>;
  using const_iterator = typename array_type::const_iterator;
  
  result_type id;
  Combine combine;
  
  cell_output(result_type id, Combine combine)
  : id(id), combine(combine) { }
  
  cell_output(const cell_output& other)
  : id(other.id), combine(other.combine) { }
  
  void init(result_type& dst) const {
    dst = id;
  }
  
  void copy(const result_type& src, result_type& dst) const {
    dst = src;
  }
  
  void merge(const result_type& src, result_type& dst) const {
    dst = combine(dst, src);
  }
  
  void merge(const_iterator lo, const_iterator hi, result_type& dst) const {
    dst = id;
    for (const_iterator it = lo; it != hi; it++) {
      dst = combine(*it, dst);
    }
  }
  
};
  
template <class Chunked_sequence>
class chunkedseq_output {
public:
  
  using result_type = Chunked_sequence;
  
  result_type id;
  
  chunkedseq_output() { }
  
  chunkedseq_output(const chunkedseq_output& other) { }
  
  void init(result_type& dst) const {
    
  }
  
  void merge(result_type& src, result_type& dst) const {
    dst.concat(src);
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
  Result result;
  auto lift_idx_dst = [&] (long pos, Iter it, Result& dst) {
    dst = lift_idx(pos, it);
  };
  auto seq_lift_dst = [&] (Iter lo, Iter hi, Result& dst) {
    dst = seq_lift(lo, hi);
  };
  level3::reduce(lo, hi, out, id, result, lift_comp_rng, lift_idx_dst, seq_lift_dst);
  return result;
}
  
template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp_rng,
  class Lift_idx,
  class Seq_lift
>
parray::parray<Result> scan(Iter lo,
                            Iter hi,
                            Result id,
                            const Combine& combine,
                            const Lift_comp_rng& lift_comp_rng,
                            const Lift_idx& lift_idx,
                            const Seq_lift& seq_lift,
                            scan_type st) {
  using output_type = level3::cell_output<Result, Combine>;
  output_type out(id, combine);
  parray::parray<Result> results(hi-lo);
  auto outs_lo = results.begin();
  auto lift_idx_dst = [&] (long pos, Iter it, Result& dst) {
    dst = lift_idx(pos, it);
  };
  auto seq_lift_dst = [&] (Iter lo, Iter hi, typename parray::parray<Result>::iterator outs_lo) {
    seq_lift(lo, hi, outs_lo);
  };
  level3::scan(lo, hi, out, id, outs_lo, lift_comp_rng, lift_idx_dst, seq_lift_dst, st);
  return results;
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
               const Combine& combine,
               const Lift_idx& lift_idx) {
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
              const Combine& combine,
              const Lift& lift) {
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
               const Combine& combine,
               const Lift_comp_idx& lift_comp_idx,
               const Lift_idx& lift_idx) {
  parray::parray<long> w = weights(hi - lo, [&] (long pos) {
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
              const Combine& combine,
              const Lift_comp& lift_comp,
              const Lift& lift) {
  auto lift_comp_idx = [&] (long pos, Iter it) {
    return lift_comp(it);
  };
  auto lift_idx = [&] (long pos, Iter it) {
    return lift(it);
  };
  return reducei(lo, hi, id, combine, lift_comp_idx, lift_idx);
}

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_idx
>
parray::parray<Result> scani(Iter lo,
                             Iter hi,
                             Result id,
                             const Combine& combine,
                             const Lift_idx& lift_idx,
                             scan_type st) {
  using output_type = level3::cell_output<Result, Combine>;
  output_type out(id, combine);
  auto lift_comp_rng = [&] (Iter lo, Iter hi) {
    return hi - lo;
  };
  auto seq_lift = [&] (Iter lo, Iter hi, typename parray::parray<Result>::iterator outs_lo) {
    long i = 0;
    level4::scan_seq(lo, hi, outs_lo, out, id, [&] (Iter src, Result& dst) {
      dst = lift_idx(i, src);
    }, st);
  };
  return level2::scan(lo, hi, id, combine, lift_comp_rng, lift_idx, seq_lift, st);
}

template <
  class Iter,
  class Result,
  class Combine,
  class Lift
>
parray::parray<Result> scan(Iter lo,
                            Iter hi,
                            Result id,
                            const Combine& combine,
                            const Lift& lift,
                            scan_type st) {
  auto lift_idx = [&] (long pos, Iter it) {
    return lift(it);
  };
  return scani(lo, hi, id, combine, lift_idx, st);
}

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp_idx,
  class Lift_idx
>
parray::parray<Result> scani(Iter lo,
                             Iter hi,
                             Result id,
                             const Combine& combine,
                             const Lift_comp_idx& lift_comp_idx,
                             const Lift_idx& lift_idx,
                             scan_type st) {
  using output_type = level3::cell_output<Result, Combine>;
  output_type out(id, combine);
  parray::parray<long> w = weights(hi-lo, [&] (long pos) {
    return lift_comp_idx(pos, lo+pos);
  });
  auto lift_comp_rng = [&] (Iter _lo, Iter _hi) {
    long l = _lo - lo;
    long h = _hi - lo;
    long wrng = w[l] - w[h];
    return (long)(log(wrng) * wrng);
  };
  auto seq_lift = [&] (Iter lo, Iter hi, typename parray::parray<Result>::iterator outs_lo) {
    long i = 0;
    level4::scan_seq(lo, hi, outs_lo, out, id, [&] (Iter src, Result& dst) {
      dst = lift_idx(i, src);
    }, st);
  };
  return level2::scan(lo, hi, id, combine, lift_comp_rng, lift_idx, seq_lift, st);
}

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp,
  class Lift
>
parray::parray<Result> scan(Iter lo,
                            Iter hi,
                            Result id,
                            const Combine& combine,
                            const Lift_comp& lift_comp,
                            const Lift& lift,
                            scan_type st) {
  auto lift_comp_idx = [&] (long pos, Iter it) {
    return lift_comp(it);
  };
  auto lift_idx = [&] (long pos, Iter it) {
    return lift(it);
  };
  return scan(lo, hi, id, combine, lift_comp_idx, lift_idx, st);
}
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Reduction level 0 */

template <class Iter, class Item, class Combine>
Item reduce(Iter lo, Iter hi, Item id, const Combine& combine) {
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
            const Weight& weight,
            const Combine& combine) {
  auto lift = [&] (Iter it) {
    return *it;
  };
  auto lift_comp = [&] (Iter it) {
    return weight(*it);
  };
  return level1::reduce(lo, hi, id, combine, lift_comp, lift);
}
  
template <
  class Iter,
  class Item,
  class Combine
>
parray::parray<Item> scan(Iter lo,
                          Iter hi,
                          Item id,
                          const Combine& combine, scan_type st) {
  auto lift = [&] (Iter it) {
    return *it;
  };
  return level1::scan(lo, hi, id, combine, lift, st);
}

template <
  class Iter,
  class Item,
  class Weight,
  class Combine
>
parray::parray<Item> scan(Iter lo,
                          Iter hi,
                          Item id,
                          const Weight& weight,
                          const Combine& combine,
                          scan_type st) {
  auto lift = [&] (Iter it) {
    return *it;
  };
  auto lift_comp = [&] (Iter it) {
    return weight(*it);
  };
  return level1::scan(lo, hi, id, combine, lift_comp, lift, st);
}
  
/***********************************************************************/

} // end namespace
} // end namespace

#endif /*! _PCTL_DATAPAR_H_ */
