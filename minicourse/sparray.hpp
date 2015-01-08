/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file array.hpp
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

#include "hash.hpp"
#include "granularity.hpp"

#ifndef _MINICOURSE_SPARRAY_H_
#define _MINICOURSE_SPARRAY_H_

/***********************************************************************/

/*---------------------------------------------------------------------*/

#ifndef TARGET_MAC_OS
static int __ii =  mallopt(M_MMAP_MAX,0);
static int __jj =  mallopt(M_TRIM_THRESHOLD,-1);
#endif

namespace par = pasl::sched::granularity;

#if defined(CONTROL_BY_FORCE_SEQUENTIAL)
using controller_type = par::control_by_force_sequential;
#elif defined(CONTROL_BY_FORCE_PARALLEL)
using controller_type = par::control_by_force_parallel;
#else
using controller_type = par::control_by_prediction;
#endif
using loop_controller_type = par::loop_by_eager_binary_splitting<controller_type>;

#ifdef VALUE_32_BITS
using value_type = int;
#define VALUE_MIN INT_MIN
#define VALUE_MAX INT_MAX
#define VALUE_NB_BITS 32
#else
using value_type = long;
#define VALUE_MIN LONG_MIN
#define VALUE_MAX LONG_MAX
#define VALUE_NB_BITS 64
#endif

/*---------------------------------------------------------------------*/

void todo() {
  pasl::util::atomic::fatal([&] {
    std::cerr << "TODO" << std::endl;
  });
}

template <class T>
T* my_malloc(long n) {
  return (T*)malloc(n*sizeof(T));
}

/*---------------------------------------------------------------------*/
/* Primitive memory transfer */

namespace prim {
  
  using pointer_type = value_type*;
  using const_pointer_type = const value_type*;
  
  controller_type pfill_contr("pfill");
  
  void pfill(pointer_type first, pointer_type last, value_type val) {
    long nb = last-first;
    auto seq = [&] {
      std::fill(first, last, val);
    };
    par::cstmt(pfill_contr, [&] { return nb; }, [&] {
      if (nb <= 512) {
        seq();
      } else {
        long m = nb/2;
        par::fork2([&] {
          pfill(first, first+m, val);
        }, [&] {
          pfill(first+m, last, val);
        });
      }
    }, seq);
  }
  
  void pfill(pointer_type first, long nb, value_type val) {
    pfill(first, first+nb, val);
  }
  
  void copy(const_pointer_type src, pointer_type dst,
            long lo_src, long hi_src, long lo_dst) {
    if (hi_src-lo_src > 0)
      std::copy(&src[lo_src], &src[hi_src-1]+1, &dst[lo_dst]);
  }
  
  controller_type pcopy_contr("pcopy");
  
  void pcopy(const_pointer_type first, const_pointer_type last, pointer_type d_first) {
    long nb = last-first;
    auto seq = [&] {
      std::copy(first, last, d_first);
    };
    par::cstmt(pcopy_contr, [&] { return nb; }, [&] {
      if (nb <= 512) {
        seq();
      } else {
        long m = nb/2;
        par::fork2([&] {
          pcopy(first,   first+m, d_first);
        }, [&] {
          pcopy(first+m, last,    d_first+m);
        });
      }
    }, seq);
  }
  
  void pcopy(const_pointer_type src, pointer_type dst,
             long lo_src, long hi_src, long lo_dst) {
    pcopy(&src[lo_src], &src[hi_src], &dst[lo_dst]);
  }
  
}

/*---------------------------------------------------------------------*/
/* Array-based implementation of sequences */

class sparray {
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
    value_type* p = my_malloc<value_type>(sz);
    assert(p != nullptr);
    ptr.reset(p);
  }
  
  void check(long i) const {
    assert(ptr != nullptr);
    assert(i >= 0);
    assert(i < sz);
  }
  
public:
  
  sparray(long sz = 0)
  : sz(sz) {
    alloc();
  }
  
  sparray(std::initializer_list<value_type> xs)
  : sz(xs.size()) {
    alloc();
    long i = 0;
    for (auto it = xs.begin(); it != xs.end(); it++)
      ptr[i++] = *it;
  }
  
  // To disable copy semantics, we disable:
  sparray(const sparray& other);            //   1. copy constructor
  sparray& operator=(const sparray& other); //   2. assign-by-copy operator
  
  sparray& operator=(sparray&& other) {
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
  
  void swap(sparray& other) {
    ptr.swap(other.ptr);
    std::swap(sz, other.sz);
  }
  
};

std::ostream& operator<<(std::ostream& out, const sparray& xs) {
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
/* Random-array generation */

const long rand_maxval = 1l<<20;

#ifdef SEQUENTIAL_RANDOM_NUMBER_GEN

sparray gen_random_sparray_seq(long n) {
  sparray tmp = sparray(n);
  for (long i = 0; i < n; i++)
    tmp[i] = std::abs(random()) % rand_maxval;
  return tmp;
}

#endif

loop_controller_type random_sparray_contr("random_sparray");

// returns a random array of size n using seed s
sparray gen_random_sparray_par(long s, long n, long maxval) {
  sparray tmp = sparray(n);
  //      par::parallel_for(random_sparray_contr, 0l, n, [&] (long i) {
  pasl::sched::native::parallel_for(0l, n, [&] (long i) {
    tmp[i] = std::abs(hash_signed(i+s)) % maxval;
  }); 
  return tmp;
}

sparray gen_random_sparray_par(long n) {
  return gen_random_sparray_par(23423, n, rand_maxval);
}

sparray gen_random_sparray(long n) {
#ifdef SEQUENTIAL_RANDOM_NUMBER_GEN
  return gen_random_sparray_seq(n);
#else
  return gen_random_sparray_par(n);
#endif
}

/*---------------------------------------------------------------------*/
/* Sample lambda expressions */

auto identity_fct = [] (value_type x) {
  return x;
};

auto plus_fct = [] (value_type x, value_type y) {
  return x+y;
};

auto max_fct = [] (value_type x, value_type y) {
  return std::max(x, y);
};

auto min_fct = [] (value_type x, value_type y) {
  return std::min(x, y);
};

auto and_fct = [] (value_type x, value_type y) {
  return x and y;
};

auto plus1_fct = [] (value_type x) {
  return plus_fct(x, 1);
};

auto is_even_fct = [] (value_type x) {
  return x % 2 == 0;
};

/*---------------------------------------------------------------------*/
/* Parallel array operations */

sparray fill(long n, value_type v) {
  sparray tmp = sparray(n);
  prim::pfill(&tmp[0], n, v);
  return tmp;
}

sparray empty() {
  return sparray(0);
}

sparray singleton(value_type v) {
  return fill(1, v);
}

sparray slice(const sparray& xs, long lo, long hi) {
  long n = hi-lo;
  assert(n <= xs.size());
  assert(n >= 0);
  sparray result = sparray(n);
  if (n > 0)
    prim::pcopy(&xs[0], &result[0], lo, hi, 0);
  return result;
}

sparray take(const sparray& xs, long n) {
  return slice(xs, 0, n);
}

sparray drop(const sparray& xs, long n) {
  long m = xs.size()-n;
  return slice(xs, n, n+m);
}

sparray copy(const sparray& xs) {
  return take(xs, xs.size());
}

loop_controller_type concat_contr("concat");

sparray concat(const sparray** xss, long k) {
  long n = 0;
  for (long i = 0; i < k; i++)
    n += xss[i]->size();
  sparray result = sparray(n);
  long offset = 0;
  for (long i = 0; i < k; i++) {
    const sparray& xs = *xss[i];
    long m = xs.size();
    if (m > 0l)
      prim::pcopy(&xs[0l], &result[0l], 0l, m, offset);
    offset += m;
  }
  return result;
}

sparray concat(const sparray& xs1, const sparray& xs2) {
  const sparray* xss[2] = { &xs1, &xs2 };
  return concat(xss, 2);
}

sparray concat(const sparray& xs1, const sparray& xs2, const sparray& xs3) {
  const sparray* xss[3] = { &xs1, &xs2, &xs3 };
  return concat(xss, 3);
}

template <class Func>
class tabulate_controller_type {
public:
  static loop_controller_type contr;
};
template <class Func>
loop_controller_type tabulate_controller_type<Func>::contr("tabulate"+
                                                           par::string_of_template_arg<Func>());

template <class Func>
sparray tabulate(const Func& f, long n) {
  sparray tmp = sparray(n);
  par::parallel_for(tabulate_controller_type<Func>::contr, 0l, n, [&] (long i) {
    tmp[i] = f(i);
  });
  return tmp;
}

template <class Func>
sparray map(const Func& f, const sparray& xs) {
  return tabulate([&] (long i) { return f(xs[i]); }, xs.size());
}

template <class Func>
sparray map_pair(const Func& f, const sparray& xs, const sparray& ys) {
  long n = std::min(xs.size(), ys.size());
  return tabulate([&] (long i) { return f(xs[i], ys[i]); }, n);
}

template <class Assoc_op, class Lift_func>
value_type reduce_seq(const Assoc_op& op, const Lift_func& lift, value_type id, const sparray& xs,
                      long lo, long hi) {
  value_type x = id;
  for (long i = lo; i < hi; i++)
    x = op(x, lift(xs[i]));
  return x;
}

template <class Assoc_op, class Lift_func>
class reduce_controller_type {
public:
  static controller_type contr;
};
template <class Assoc_op, class Lift_func>
controller_type reduce_controller_type<Assoc_op,Lift_func>::contr("reduce"+
                                                                  par::string_of_template_arg<Assoc_op>()+
                                                                  par::string_of_template_arg<Lift_func>());

template <class Assoc_op, class Lift_func>
value_type reduce_rec(const Assoc_op& op, const Lift_func& lift, value_type id, const sparray& xs,
                      long lo, long hi) {
  using contr_type = reduce_controller_type<Assoc_op,Lift_func>;
  value_type result = id;
  auto seq = [&] {
    result = reduce_seq(op, lift, id, xs, lo, hi);
  };
  par::cstmt(contr_type::contr, [&] { return hi-lo; }, [&] {
    if (hi <= lo + 2) {
      seq();
    } else {
      long m = (lo+hi)/2;
      value_type v1,v2;
      par::fork2([&] {
        v1 = reduce_rec(op, lift, id, xs, lo, m);
      }, [&] {
        v2 = reduce_rec(op, lift, id, xs, m, hi);
      });
      result = op(v1, v2);
    }
  }, seq);
  return result;
}

template <class Assoc_op, class Lift_func>
value_type reduce(const Assoc_op& op, const Lift_func& lift, value_type id, const sparray& xs) {
  return reduce_rec(op, lift, id, xs, 0, xs.size());
}

template <class Assoc_op>
value_type reduce(const Assoc_op& op, value_type id, const sparray& xs) {
  return reduce(op, identity_fct, id, xs);
}

value_type sum(value_type id, const sparray& xs) {
  return reduce(plus_fct, id, xs);
}

value_type sum(const sparray& xs) {
  return sum(0l, xs);
}

value_type max(const sparray& xs) {
  return reduce(max_fct, VALUE_MIN, xs);
}

value_type min(const sparray& xs) {
  return reduce(min_fct, VALUE_MAX, xs);
}

template <class Assoc_op, class Lift_func>
class scan_controller_type {
public:
  static controller_type body;
  static loop_controller_type lp1;
  static loop_controller_type lp2;
};
template <class Assoc_op, class Lift_func>
controller_type scan_controller_type<Assoc_op,Lift_func>::body("scan_body"+
                                                               par::string_of_template_arg<Assoc_op>()+
                                                               par::string_of_template_arg<Lift_func>());
template <class Assoc_op, class Lift_func>
loop_controller_type scan_controller_type<Assoc_op,Lift_func>::lp1("scan_lp1"+
                                                                   par::string_of_template_arg<Assoc_op>()+
                                                                   par::string_of_template_arg<Lift_func>());
template <class Assoc_op, class Lift_func>
loop_controller_type scan_controller_type<Assoc_op,Lift_func>::lp2("scan_lp2"+
                                                                   par::string_of_template_arg<Assoc_op>()+
                                                                   par::string_of_template_arg<Lift_func>());

class scan_excl_result {
public:
  sparray partials;
  value_type total;
  
  scan_excl_result() { }
  scan_excl_result(sparray&& _partials, value_type total) {
    this->partials = std::move(_partials);
    this->total = total;
  }
  
  // disable copying
  scan_excl_result(const scan_excl_result&);
  scan_excl_result& operator=(const scan_excl_result&);
  
  scan_excl_result(scan_excl_result&& other) {
    partials = std::move(other.partials);
    total = std::move(other.total);
  }
  
  scan_excl_result& operator=(scan_excl_result&& other) {
    partials = std::move(other.partials);
    total = std::move(other.total);
    return *this;
  }
  
};

template <class Assoc_op, class Lift_func>
value_type scan_seq(const Assoc_op& op, const Lift_func& lift, value_type id, const sparray& xs,
                    sparray& dest, long lo, long hi, const bool is_excl) {
  value_type x = id;
  if (is_excl) {
    for (long i = lo; i < hi; i++) {
      dest[i] = x;
      x = op(x, lift(xs[i]));
    }
  } else {
    for (long i = lo; i < hi; i++) {
      x = op(x, lift(xs[i]));
      dest[i] = x;
    }
  }
  return x;
}

template <class Assoc_op, class Lift_func>
value_type scan_seq(const Assoc_op& op, const Lift_func& lift, value_type id,
                    const sparray& xs, sparray& dest, const bool is_excl) {
  return scan_seq(op, lift, id, xs, dest, 0l, xs.size(), is_excl);
}

template <class Assoc_op, class Lift_func>
scan_excl_result scan_rec(const Assoc_op& op, const Lift_func& lift, value_type id, const sparray& xs, const bool is_excl) {
  const long k = 1024;
  using contr_type = scan_controller_type<Assoc_op,Lift_func>;
  long n = xs.size();
  scan_excl_result result;
  auto seq = [&] {
    result.partials = sparray(n);
    result.total = scan_seq(op, lift, id, xs, result.partials, is_excl);
  };
  long m = 1 + ((n - 1) / k);
  par::cstmt(contr_type::body, [&] { return m; }, [&] {
    if (n <= k) {
      seq();
    } else {
      sparray sums = sparray(m);
      par::parallel_for(contr_type::lp1, 0l, m, [&] (long i) {
        long lo = i * k;
        long hi = std::min(lo + k, n);
        sums[i] = reduce_seq(op, lift, id, xs, lo, hi);
      });
      scan_excl_result scans = scan_rec(op, identity_fct, id, sums, true);
      sums = {};
      result.partials = sparray(n);
      par::parallel_for(contr_type::lp2, 0l, m, [&] (long i) {
        long lo = i * k;
        long hi = std::min(lo + k, n);
        scan_seq(op, lift, scans.partials[i], xs, result.partials, lo, hi, is_excl);
      });
      result.total = scans.total;
    }
  }, seq);
  return result;
}

template <class Assoc_op, class Lift_func>
scan_excl_result scan_excl(const Assoc_op& op, const Lift_func& lift, value_type id, const sparray& xs) {
  return scan_rec(op, lift, id, xs, true);
}

template <class Assoc_op, class Lift_func>
sparray scan_incl(const Assoc_op& op, const Lift_func& lift, value_type id, const sparray& xs) {
  sparray result;
  scan_excl_result scan = scan_rec(op, lift, id, xs, false);
  result = std::move(scan.partials);
  return result;
}

template <class Assoc_op>
sparray scan_incl(const Assoc_op& op, value_type id, const sparray& xs) {
  return scan_incl(op, identity_fct, id, xs);
}

template <class Assoc_op>
scan_excl_result scan_excl(const Assoc_op& op, value_type id, const sparray& xs) {
  return scan_excl(op, identity_fct, id, xs);
}

scan_excl_result prefix_sums_excl(value_type id, const sparray& xs) {
  return scan_excl(plus_fct, identity_fct, id, xs);
}

scan_excl_result prefix_sums_excl(const sparray& xs) {
  return prefix_sums_excl(0l, xs);
}

sparray prefix_sums_incl(value_type id, const sparray& xs) {
  return scan_incl(plus_fct, identity_fct, id, xs);
}

sparray prefix_sums_incl(const sparray& xs) {
  return prefix_sums_incl(0l, xs);
}

loop_controller_type pack_contr("pack");

template <class Pred>
sparray pack_by_predicate(const Pred& p, const sparray& xs) {
  long n = xs.size();
  sparray result = {};
  if (n == 0)
    return result;
  scan_excl_result offsets = scan_excl(plus_fct, p, 0l, xs);
  value_type m = offsets.total;
  result = sparray(m);
  par::parallel_for(pack_contr, 0l, n, [&] (long i) {
    value_type cur  = offsets.partials[i];
    value_type next = (i + 1 == n) ? offsets.total : offsets.partials[i+1];
    if ((cur+1) == next)
      result[offsets.partials[i]] = xs[i];
  });
  return result;
}

sparray pack(const sparray& flags, const sparray& xs) {
  return pack_by_predicate([&] (long i) { return flags[i]; }, xs);
}

template <class Pred>
sparray filter(const Pred& p, const sparray& xs) {
  /*
  return pack(map(p, xs), xs);
   */
  return pack_by_predicate(p, xs);
}

/***********************************************************************/

#endif /*! _MINICOURSE_SPARRAY_H_ */
