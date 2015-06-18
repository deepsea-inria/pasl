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
#include <type_traits>

#include "weights.hpp"
#include "atomic.hpp"
#include "pchunkedseqbase.hpp"

#ifndef _PCTL_DATAPAR_H_
#define _PCTL_DATAPAR_H_

namespace pasl {
namespace pctl {

/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/* Scan types */
  
using scan_type = enum {
  forward_inclusive_scan,
  forward_exclusive_scan,
  backward_inclusive_scan,
  backward_exclusive_scan
};
  
static inline bool is_backward_scan(scan_type st) {
  return (st == backward_inclusive_scan) || (st == backward_exclusive_scan);
}
  
/*---------------------------------------------------------------------*/
/* Level 4 reduction */
  
namespace level4 {

namespace {
  
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

template <
  class Input,
  class Output,
  class Result,
  class Convert_reduce_comp,
  class Convert_reduce,
  class Seq_convert_reduce
>
class reduce_contr {
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
controller_type reduce_contr<Input,Output,Result,Convert_reduce_comp,Convert_reduce,Seq_convert_reduce>::contr(
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
  using controller_type = reduce_contr<Input, Output, Result, Convert_reduce_comp, Convert_reduce, Seq_convert_reduce>;
  reduce_rec(in, out, id, dst, convert_reduce_comp, convert_reduce, seq_convert_reduce, controller_type::contr);
}

namespace {
  
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
  if (st == forward_exclusive_scan) {
    in_it = in_lo;
    out_it = out_lo;
    for (; in_it != in_hi; in_it++, out_it++) {
      Result tmp1; // required because input and output ranges can overlap
      out.copy(x, tmp1);
      Result tmp2;
      convert(*in_it, tmp2);
      out.merge(tmp2, x);
      out.copy(tmp1, *out_it);
    }
  } else if (st == forward_inclusive_scan) {
    in_it = in_lo;
    out_it = out_lo;
    for (; in_it != in_hi; in_it++, out_it++) {
      Result tmp;
      convert(*in_it, tmp);
      out.merge(tmp, x);
      out.copy(x, *out_it);
    }
  } else if (st == backward_exclusive_scan) {
    long n = in_hi - in_lo;
    long m = n - 1;
    in_it = in_lo + m;
    out_it = out_lo + m;
    for (; in_it >= in_lo; in_it--, out_it--) {
      Result tmp1; // required because input and output ranges can overlap
      out.copy(x, tmp1);
      Result tmp2;
      convert(*in_it, tmp2);
      out.merge(tmp2, x);
      out.copy(tmp1, *out_it);
    }
  } else if (st == backward_inclusive_scan) {
    long n = in_hi - in_lo;
    long m = n - 1;
    in_it = in_lo + m;
    out_it = out_lo + m;
    for (; in_it >= in_lo; in_it--, out_it--) {
      Result tmp;
      convert(*in_it, tmp);
      out.merge(tmp, x);
      out.copy(x, *out_it);
    }
  } else {
    util::atomic::die("Bogus scan type passed to scan");
  }
}
  
template <class In_iter, class Out_iter, class Output, class Result>
void scan_seq(In_iter in_lo,
              In_iter in_hi,
              Out_iter out_lo,
              const Output& out,
              const Result& id,
              scan_type st) {
  auto convert = [&] (const Result& src, Result& dst) {
    out.copy(src, dst);
  };
  scan_seq(in_lo, in_hi, out_lo, out, id, convert, st);
}

template <class Result, class Output>
void scan_seq(const parray<Result>& ins,
              typename parray<Result>::iterator outs_lo,
              const Output& out,
              const Result& id,
              scan_type st) {
  scan_seq(ins.cbegin(), ins.cend(), outs_lo, out, id, st);
}

template <class Result, class Output, class Merge_comp>
class scan_rec_contr {
public:
  static controller_type contr;
};

template <class Result, class Output, class Merge_comp>
controller_type scan_rec_contr<Result, Output, Merge_comp>::contr(
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
class scan_contr {
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
controller_type scan_contr<Input,Output,Result,Output_iter,Convert_reduce_comp,Convert_reduce,Convert_scan,Seq_convert_scan>::contr(
  "scan"+sota<Input>()+sota<Output>()+sota<Result>()+sota<Output_iter>()+sota<Convert_reduce_comp>()+
         sota<Convert_reduce>()+sota<Convert_scan>()+sota<Seq_convert_scan>());

#ifdef CONTROL_BY_FORCE_PARALLEL
const long Scan_branching_factor = 2;
#else
const long Scan_branching_factor = 1024;
#endif

static inline long get_nb_blocks(long k, long n) {
  return 1 + ((n - 1) / k);
} 
  
static inline std::pair<long,long> get_rng(long k, long n, long i) {
  long lo = i * k;
  long hi = std::min(lo + k, n);
  return std::make_pair(lo, hi);
}
  
template <class Result, class Output, class Merge_comp>
void scan_rec(const parray<Result>& ins,
              typename parray<Result>::iterator outs_lo,
              const Output& out,
              const Result& id,
              const Merge_comp& merge_comp,
              scan_type st) {
  using controller_type = scan_rec_contr<Result, Output, Merge_comp>;
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
      parray<Result> partials(m);
      parallel_for(0l, m, loop_comp, [&] (long i) {
        auto beg = ins.cbegin();
        long lo = get_rng(k, n, i).first;
        long hi = get_rng(k, n, i).second;
        out.merge(beg+lo, beg+hi, partials[i]);
      });
      parray<Result> scans(m);
      auto st2 = (is_backward_scan(st)) ? backward_exclusive_scan : forward_exclusive_scan;
      scan_rec(partials, scans.begin(), out, id, merge_comp, st2);
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
  
} // end namespace

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
  using controller_type = scan_contr<Input, Output, Result, Output_iter,
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
      convert_scan(id, in, outs_lo);
    } else {
      parray<Input> splits = in.split(m);
      parray<Result> partials(m);
      parallel_for(0l, m, loop_comp, [&] (long i) {
        long lo = get_rng(k, n, i).first;
        long hi = get_rng(k, n, i).second;
        Input in2 = in.slice(splits, lo, hi);
        convert_reduce(in2, partials[i]);
      });
      parray<Result> scans(m);
      auto st2 = (is_backward_scan(st)) ? backward_exclusive_scan : forward_exclusive_scan;
      scan_rec(partials, scans.begin(), out, id, merge_comp, st2);
      parallel_for(0l, m, loop_comp, [&] (long i) {
        long lo = get_rng(k, n, i).first;
        long hi = get_rng(k, n, i).second;
        Input in2 = in.slice(splits, lo, hi);
        scan(in2, out, scans[i], outs_lo+lo, merge_comp, convert_reduce_comp, convert_reduce, convert_scan, seq_convert_scan, st);
      });
    }
  }, [&] {
    seq_convert_scan(id, in, outs_lo);
  });
}
  
template <class Input_iter>
class random_access_iterator_input {
public:
  
  using self_type = random_access_iterator_input;
  using array_type = parray<self_type>;
  
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
  
using tabulate_input = random_access_iterator_input<long>;
  
template <class Chunkedseq>
class chunkedseq_input {
public:
  
  using self_type = chunkedseq_input<Chunkedseq>;
  using array_type = parray<self_type>;
  
  Chunkedseq seq;
  
  chunkedseq_input(Chunkedseq& _seq) {
    _seq.swap(seq);
  }
  
  chunkedseq_input(const chunkedseq_input& other) { }
  
  bool can_split() const {
    return seq.size() >= 2;
  }
  
  void split(chunkedseq_input& dst) {
    long n = seq.size() / 2;
    seq.split(seq.begin() + n, dst.seq);
  }
  
  array_type split(long) {
    array_type tmp;
    assert(false);
    return tmp;
  }
  
  self_type slice(const array_type&, long _lo, long _hi) {
    self_type tmp;
    assert(false);
    return tmp;
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
  class Seq_reduce_rng_dst
>
void reduce(Input_iter lo,
            Input_iter hi,
            const Output& out,
            Result& id,
            Result& dst,
            const Lift_comp_rng& lift_comp_rng,
            const Lift_idx_dst& lift_idx_dst,
            const Seq_reduce_rng_dst& seq_reduce_rng_dst) {
  using input_type = level4::random_access_iterator_input<Input_iter>;
  input_type in(lo, hi);
  auto convert_reduce_comp = [&] (input_type& in) {
    return lift_comp_rng(in.lo, in.hi);
  };
  auto convert_reduce = [&] (input_type& in, Result& dst) {
    long i = in.lo - lo;
    dst = id;
    for (Input_iter it = in.lo; it != in.hi; it++, i++) {
      Result tmp;
      lift_idx_dst(i, *it, tmp);
      out.merge(tmp, dst);
    }
  };
  auto seq_convert_reduce = [&] (input_type& in, Result& dst) {
    seq_reduce_rng_dst(in.lo, in.hi, dst);
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
  class Seq_scan_rng_dst
>
void scan(Input_iter lo,
          Input_iter hi,
          const Output& out,
          Result& id,
          Output_iter outs_lo,
          const Lift_comp_rng& lift_comp_rng,
          const Lift_idx_dst& lift_idx_dst,
          const Seq_scan_rng_dst& seq_scan_rng_dst,
          scan_type st) {
  using input_type = level4::random_access_iterator_input<Input_iter>;
  input_type in(lo, hi);
  auto convert_reduce_comp = [&] (long lo, long hi) {
    return lift_comp_rng(in.lo+lo, in.lo+hi);
  };
  auto convert_reduce = [&] (input_type& in, Result& dst) {
    long i = in.lo - lo;
    dst = id;
    for (Input_iter it = in.lo; it != in.hi; it++, i++) {
      Result tmp;
      lift_idx_dst(i, *it, tmp);
      out.merge(tmp, dst);
    }
  };
  auto convert_scan = [&] (Result _id, input_type& in, Output_iter outs_lo) {
    long pos = in.lo - lo;
    level4::scan_seq(in.lo, in.hi, outs_lo, out, _id, [&] (reference_of<Input_iter> src, Result& dst) {
      lift_idx_dst(pos++, src, dst);
    }, st);
  };
  auto seq_convert_scan = [&] (Result _id, input_type& in, Output_iter outs_lo) {
    seq_scan_rng_dst(_id, in.lo, in.hi, outs_lo);
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
  using array_type = parray<result_type>;
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
  using array_type = parray<result_type>;
  using const_iterator = typename array_type::const_iterator;
  
  result_type id;
  
  chunkedseq_output() { }
  
  void init(result_type& dst) const {
    
  }
  
  void copy(const result_type& src, result_type& dst) const {
    dst = src;
  }
  
  void merge(result_type& src, result_type& dst) const {
    dst.concat(src);
  }
  
  void merge(const_iterator lo, const_iterator hi, result_type& dst) const {
    dst = id;
    for (const_iterator it = lo; it != hi; it++) {
      merge(*it, dst);
    }
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
  class Seq_reduce_rng
>
Result reduce(Iter lo,
              Iter hi,
              Result id,
              const Combine& combine,
              const Lift_comp_rng& lift_comp_rng,
              const Lift_idx& lift_idx,
              const Seq_reduce_rng& seq_reduce_rng) {
  using output_type = level3::cell_output<Result, Combine>;
  output_type out(id, combine);
  Result result;
  auto lift_idx_dst = [&] (long pos, reference_of<Iter> x, Result& dst) {
    dst = lift_idx(pos, x);
  };
  auto seq_reduce_rng_dst = [&] (Iter lo, Iter hi, Result& dst) {
    dst = seq_reduce_rng(lo, hi);
  };
  level3::reduce(lo, hi, out, id, result, lift_comp_rng, lift_idx_dst, seq_reduce_rng_dst);
  return result;
}
  
template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp_rng,
  class Lift_idx,
  class Seq_scan_rng_dst
>
parray<Result> scan(Iter lo,
                    Iter hi,
                    Result id,
                    const Combine& combine,
                    const Lift_comp_rng& lift_comp_rng,
                    const Lift_idx& lift_idx,
                    const Seq_scan_rng_dst& seq_scan_rng_dst,
                    scan_type st) {
  using output_type = level3::cell_output<Result, Combine>;
  output_type out(id, combine);
  parray<Result> results(hi-lo);
  auto outs_lo = results.begin();
  auto lift_idx_dst = [&] (long pos, reference_of<Iter> x, Result& dst) {
    dst = lift_idx(pos, x);
  };
  level3::scan(lo, hi, out, id, outs_lo, lift_comp_rng, lift_idx_dst, seq_scan_rng_dst, st);
  return results;
}
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Reduction level 1 */
  
namespace level1 {
  
  
namespace {
  
template <class Iter, class Item>
class seq_reduce_rng_spec {
public:
  
  template <
    class Result,
    class Combine,
    class Lift_idx
  >
  Result f(long i, Iter lo, Iter hi, Result id, const Combine& combine, const Lift_idx& lift_idx) {
    Result r = id;
    for (auto it = lo; it != hi; it++, i++) {
      r = combine(r, lift_idx(i, *it));
    }
    return r;
  }

};

template <class Item>
class seq_reduce_rng_spec<typename pchunkedseq<Item>::iterator, Item> {
public:
  
  using iterator = typename pchunkedseq<Item>::iterator;
  using value_type = Item;
  using reference = reference_of<iterator>;
  
  template <
  class Result,
  class Combine,
  class Lift_idx
  >
  Result f(long i, iterator lo, iterator hi, Result id, const Combine& combine, const Lift_idx& lift_idx) {
    Result r = id;
    data::chunkedseq::extras::for_each(lo, hi, [&] (reference x) {
      r = combine(r, lift_idx(i++, x));
    });
    return r;
  }

};
  
} // end namespace
  
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
  auto seq_reduce_rng = [&] (Iter _lo, Iter _hi) {
    seq_reduce_rng_spec<Iter, value_type_of<Iter>> f;
    return f.f(_lo - lo, _lo, _hi, id, combine, lift_idx);
  };
  return level2::reduce(lo, hi, id, combine, lift_comp_rng, lift_idx, seq_reduce_rng);
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
  auto lift_idx = [&] (long pos, reference_of<Iter> x) {
    return lift(x);
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
  parray<long> w = weights(hi - lo, [&] (long pos) {
    return lift_comp_idx(pos, *(lo+pos));
  });
  auto lift_comp_rng = [&] (Iter _lo, Iter _hi) {
    long l = _lo - lo;
    long h = _hi - lo;
    long wrng = w[h] - w[l];
    return (long)(log(wrng) * wrng);
  };
  auto seq_reduce_rng = [&] (Iter _lo, Iter _hi) {
    seq_reduce_rng_spec<Iter, value_type_of<Iter>> f;
    return f.f(_lo - lo, _lo, _hi, id, combine, lift_idx);
  };
  return level2::reduce(lo, hi, id, combine, lift_comp_rng, lift_idx, seq_reduce_rng);
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
  auto lift_comp_idx = [&] (long pos, reference_of<Iter> x) {
    return lift_comp(x);
  };
  auto lift_idx = [&] (long pos, reference_of<Iter> x) {
    return lift(x);
  };
  return reducei(lo, hi, id, combine, lift_comp_idx, lift_idx);
}

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_idx
>
parray<Result> scani(Iter lo,
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
  using iterator = typename parray<Result>::iterator;
  auto seq_scan_rng_dst = [&] (Result _id, Iter _lo, Iter _hi, iterator outs_lo) {
    long pos = _lo - lo;
    level4::scan_seq(_lo, _hi, outs_lo, out, _id, [&] (reference_of<Iter> src, Result& dst) {
      dst = lift_idx(pos++, src);
    }, st);
  };
  return level2::scan(lo, hi, id, combine, lift_comp_rng, lift_idx, seq_scan_rng_dst, st);
}

template <
  class Iter,
  class Result,
  class Combine,
  class Lift
>
parray<Result> scan(Iter lo,
                    Iter hi,
                    Result id,
                    const Combine& combine,
                    const Lift& lift,
                    scan_type st) {
  auto lift_idx = [&] (long pos, reference_of<Iter> x) {
    return lift(x);
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
parray<Result> scani(Iter lo,
                     Iter hi,
                     Result id,
                     const Combine& combine,
                     const Lift_comp_idx& lift_comp_idx,
                     const Lift_idx& lift_idx,
                     scan_type st) {
  using output_type = level3::cell_output<Result, Combine>;
  output_type out(id, combine);
  parray<long> w = weights(hi-lo, [&] (long pos) {
    return lift_comp_idx(pos, *(lo+pos));
  });
  auto lift_comp_rng = [&] (Iter _lo, Iter _hi) {
    long l = _lo - lo;
    long h = _hi - lo;
    long wrng = w[l] - w[h];
    return (long)(log(wrng) * wrng);
  };
  using iterator = typename parray<Result>::iterator;
  auto seq_scan_rng_dst = [&] (Result _id, Iter _lo, Iter _hi, iterator outs_lo) {
    long pos = _lo - lo;
    level4::scan_seq(_lo, _hi, outs_lo, out, _id, [&] (reference_of<Iter> src, Result& dst) {
      dst = lift_idx(pos++, src);
    }, st);
  };
  return level2::scan(lo, hi, id, combine, lift_comp_rng, lift_idx, seq_scan_rng_dst, st);
}

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp,
  class Lift
>
parray<Result> scan(Iter lo,
                    Iter hi,
                    Result id,
                    const Combine& combine,
                    const Lift_comp& lift_comp,
                    const Lift& lift,
                    scan_type st) {
  auto lift_comp_idx = [&] (long pos, reference_of<Iter> x) {
    return lift_comp(x);
  };
  auto lift_idx = [&] (long pos, reference_of<Iter> x) {
    return lift(x);
  };
  return scan(lo, hi, id, combine, lift_comp_idx, lift_idx, st);
}
  
template <
  class Input_iter,
  class Result_iter,
  class Result,
  class Combine,
  class Lift_idx
>
Result total_from_exclusive_scani(Input_iter in_lo,
                                  Input_iter in_hi,
                                  Result_iter scan_lo,
                                  Result id,
                                  const Combine& combine,
                                  const Lift_idx& lift_idx) {
  long n = in_hi - in_lo;
  if (n < 1) {
    return id;
  } else {
    long last = n - 1;
    return combine(*(scan_lo+last), lift_idx(last, *(in_lo+last)));
  }
}
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Reduction level 0 */

template <class Iter, class Item, class Combine>
Item reduce(Iter lo, Iter hi, Item id, const Combine& combine) {
  auto lift = [&] (reference_of<Iter> x) {
    return x;
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
  auto lift = [&] (reference_of<Iter> x) {
    return x;
  };
  auto lift_comp = [&] (reference_of<Iter> x) {
    return weight(x);
  };
  return level1::reduce(lo, hi, id, combine, lift_comp, lift);
}
  
template <
  class Iter,
  class Item,
  class Combine
>
parray<Item> scan(Iter lo,
                          Iter hi,
                          Item id,
                          const Combine& combine, scan_type st) {
  auto lift = [&] (reference_of<Iter> x) {
    return x;
  };
  return level1::scan(lo, hi, id, combine, lift, st);
}

template <
  class Iter,
  class Item,
  class Weight,
  class Combine
>
parray<Item> scan(Iter lo,
                  Iter hi,
                  Item id,
                  const Weight& weight,
                  const Combine& combine,
                  scan_type st) {
  auto lift = [&] (reference_of<Iter> x) {
    return x;
  };
  auto lift_comp = [&] (reference_of<Iter> x) {
    return weight(x);
  };
  return level1::scan(lo, hi, id, combine, lift_comp, lift, st);
}
  
/*---------------------------------------------------------------------*/
/* Max index */
  
template <
  class Item,
  class Comp,
  class Get
>
long max_index(long n, const Item& id, const Comp& comp, const Get& get) {
  if (n < 1) {
    return -1L;
  }
  using result_type = std::pair<long, Item>;
  result_type res(0, id);
  using input_type = level4::tabulate_input;
  input_type in(0, n);
  auto combine = [&] (result_type x, result_type y) {
    if (comp(x.second, y.second)) { // x > y
      return x;
    } else {
      return y;
    }
  };
  using output_type = level3::cell_output<result_type, decltype(combine)>;
  output_type out(res, combine);
  auto convert_reduce_comp = [&] (input_type& in) {
    return in.size();
  };
  auto convert_reduce = [&] (input_type& in, result_type& out) {
    for (long i = in.lo; i < in.hi; i++) {
      const Item& x = get(i);
      if (comp(x, out.second)) {
        out = result_type(i, x);
      }
    }
  };
  auto seq_convert_reduce = convert_reduce;
  level4::reduce(in, out, res, res, convert_reduce_comp, convert_reduce, seq_convert_reduce);
  return res.first;
}
  
template <
  class Iter,
  class Item,
  class Comp,
  class Lift
>
long max_index(Iter lo, Iter hi, const Item& id, const Comp& comp, const Lift& lift) {
  if (hi-lo < 1) {
    return -1L;
  }
  using result_type = std::pair<long, Item>;
  result_type id2(0, id);
  auto combine = [&] (result_type x, result_type y) {
    if (comp(x.second, y.second)) { // x > y
      return x;
    } else {
      return y;
    }
  };
  auto lift_comp_rng = [&] (Iter lo, Iter hi) {
    return hi - lo;
  };
  auto lift_idx = [&] (long i, reference_of<Iter> x) {
    return result_type(i, lift(i, x));
  };
  auto seq_reduce_rng = [&] (Iter _lo, Iter _hi) {
    long i = _lo - lo;
    result_type res(0, id);
    for (Iter it = _lo; it != _hi; it++, i++) {
      const Item& x = *it;
      if (comp(x, res.second)) {
        res = result_type(i, x);
      }
    }
    return res;
  };
  return level2::reduce(lo, hi, id2, combine, lift_comp_rng, lift_idx, seq_reduce_rng).first;
}
  
template <
  class Iter,
  class Item,
  class Comp
>
long max_index(Iter lo, Iter hi, const Item& id, const Comp& comp) {
  return max_index(lo, hi, id, comp, [&] (long, const Item& x) {
    return x;
  });
}
  
/*---------------------------------------------------------------------*/
/* Pack and filter */
  
namespace __priv {
    
template <
  class Flags_iter,
  class Iter,
  class Item,
  class Output,
  class F
>
long pack(Flags_iter flags_lo, Iter lo, Iter hi, Item&, const Output& out, const F f) {
  long n = hi - lo;
  if (n < 1) {
    return 0;
  }
  auto combine = [&] (long x, long y) {
    return x + y;
  };
  auto lift = [&] (reference_of<Flags_iter> x) {
    return (long)x;
  };
  parray<long> offsets = level1::scan(flags_lo, flags_lo+n, 0L, combine, lift, forward_exclusive_scan);
  auto lift_idx = [&] (long, reference_of<Flags_iter> b) {
    return lift(b);
  };
  long m = level1::total_from_exclusive_scani(flags_lo, flags_lo+n, offsets.begin(), 0L, combine, lift_idx);
  auto dst_lo = out(m);
  parallel_for(0L, n, [&] (long i) {
    if (flags_lo[i]) {
      long offset = offsets[i];
      *(dst_lo+offset) = f(i, *(lo+i));
    }
  });
  return m;
}

} // end namespace
  
template <class Item_iter, class Flags_iter>
parray<value_type_of<Item_iter>> pack(Item_iter lo, Item_iter hi, Flags_iter flags_lo) {
  parray<value_type_of<Item_iter>> result;
  value_type_of<Item_iter> tmp;
  __priv::pack(flags_lo, lo, hi, tmp, [&] (long m) {
    result.resize(m);
    return result.begin();
  }, [&] (long, reference_of<Item_iter> x) {
    return x;
  });
  return result;
}
  
template <class Flags_iter>
parray<long> pack_index(Flags_iter lo, Flags_iter hi) {
  parray<long> result;
  long dummy;
  __priv::pack(lo, lo, hi, dummy, [&] (long m) {
    result.resize(m);
    return result.begin();
  }, [&] (long offset, reference_of<Flags_iter>) {
    return offset;
  });
  return result;
}
  
template <class Iter, class Pred_idx>
parray<value_type_of<Iter>> filteri(Iter lo, Iter hi, const Pred_idx& pred_idx) {
  long n = hi - lo;
  parray<bool> flags(n, [&] (long i) {
    return pred_idx(i, *(lo+i));
  });
  value_type_of<Iter> dummy;
  parray<value_type_of<Iter>> dst;
  __priv::pack(flags.cbegin(), lo, hi, dummy, [&] (long m) {
    dst.resize(m);
    return dst.begin();
  }, [&] (long, reference_of<Iter> x) {
    return x;
  });
  return dst;
}
  
template <class Iter, class Pred>
parray<value_type_of<Iter>> filter(Iter lo, Iter hi, const Pred& pred) {
  auto pred_idx = [&] (long, reference_of<Iter> x) {
    return pred(x);
  };
  return filteri(lo, hi, pred_idx);
}
  
/*---------------------------------------------------------------------*/
/* Array-sum and max */
  
template <class Iter>
value_type_of<Iter> sum(Iter lo, Iter hi) {
  using number = value_type_of<Iter>;
  return reduce(lo, hi, (number)0, [&] (number x, number y) {
    return x + y;
  });
}

template <class Iter>
value_type_of<Iter> max(Iter lo, Iter hi) {
  using number = value_type_of<Iter>;
  number id = std::numeric_limits<number>::lowest();
  return reduce(lo, hi, id, [&] (number x, number y) {
    return std::max(x, y);
  });
}
  
template <class Iter>
value_type_of<Iter> min(Iter lo, Iter hi) {
  using number = value_type_of<Iter>;
  number id = std::numeric_limits<number>::max();
  return reduce(lo, hi, id, [&] (number x, number y) {
    return std::min(x, y);
  });
}
  
/***********************************************************************/

} // end namespace
} // end namespace

#endif /*! _PCTL_DATAPAR_H_ */
