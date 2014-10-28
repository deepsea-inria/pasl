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

#include "granularity.hpp"

#ifndef _MINICOURSE_ARRAY_H_
#define _MINICOURSE_ARRAY_H_

/***********************************************************************/

/*---------------------------------------------------------------------*/

namespace par = pasl::sched::granularity;

using controller_type = par::control_by_prediction;
using loop_controller_type = par::loop_by_eager_binary_splitting<controller_type>;

using value_type = long;

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

class array {
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
  
  array(long sz = 0)
  : sz(sz) {
    alloc();
  }
  
  array(std::initializer_list<value_type> xs)
  : sz(xs.size()) {
    alloc();
    long i = 0;
    for (auto it = xs.begin(); it != xs.end(); it++)
      ptr[i++] = *it;
  }
  
  // To disable copy semantics, we disable:
  array(const array& other);            //   1. copy constructor
  array& operator=(const array& other); //   2. assign-by-copy operator
  
  array& operator=(array&& other) {
    // todo: make sure that the following does not create a memory leak
    // in particular, the move instruction below should be freeing the contents of this->ptr
    ptr = std::move(other.ptr);
    sz = other.sz;
    other.sz = 0l;
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
  
  void swap(array& other) {
    ptr.swap(other.ptr);
    std::swap(sz, other.sz);
  }
  
};

using array_ref = array&;
using const_array_ref = const array&;
using array_ptr = array*;
using const_array_ptr = const array*;

std::ostream& operator<<(std::ostream& out, const_array_ref xs) {
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

array gen_random_array(long n) {
  array tmp = array(n);
  for (long i = 0; i < n; i++)
    tmp[i] = random() % 1024;
  return tmp;
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

array fill(long n, value_type v) {
  array tmp = array(n);
  prim::pfill(&tmp[0], n, v);
  return tmp;
}

array empty() {
  return array(0);
}

array singleton(value_type v) {
  return fill(1, v);
}

array take(const_array_ref xs, long n) {
  assert(n <= xs.size());
  assert(n >= 0);
  array tmp = array(n);
  if (n > 0)
    prim::pcopy(&xs[0], &tmp[0], 0, n, 0);
  return tmp;
}

array drop(const_array_ref xs, long n) {
  long sz = xs.size();
  assert(n <= sz);
  assert(n >= 0);
  long m = sz-n;
  array tmp = array(m);
  if (m > 0)
    prim::pcopy(&xs[0], &tmp[0], n, n+m, 0);
  return tmp;
}

array copy(const_array_ref xs) {
  return take(xs, xs.size());
}

loop_controller_type concat_contr("concat");

array concat(const_array_ptr* xss, long k) {
  long n = 0;
  for (long i = 0; i < k; i++)
    n += xss[i]->size();
  array result = array(n);
  long offset = 0;
  for (long i = 0; i < k; i++) {
    const_array_ref xs = *xss[i];
    long m = xs.size();
    par::parallel_for(concat_contr, 0l, m, [&] (long j) {
      result[offset+j] = xs[j];
    });
    offset += m;
  }
  return result;
}

array concat(const_array_ref xs1, const_array_ref xs2) {
  const_array_ptr xss[2] = { &xs1, &xs2 };
  return concat(xss, 2);
}

array concat(const_array_ref xs1, const_array_ref xs2, const_array_ref xs3) {
  const_array_ptr xss[3] = { &xs1, &xs2, &xs3 };
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
array tabulate(const Func& f, long n) {
  array tmp = array(n);
  par::parallel_for(tabulate_controller_type<Func>::contr, 0l, n, [&] (long i) {
    tmp[i] = f(i);
  });
  return tmp;
}

template <class Func>
array map(const Func& f, const_array_ref xs) {
  return tabulate([&] (long i) { return f(xs[i]); }, xs.size());
}

template <class Func>
array map_pair(const Func& f, const_array_ref xs, const_array_ref ys) {
  long n = std::min(xs.size(), ys.size());
  return tabulate([&] (long i) { return f(xs[i], ys[i]); }, n);
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
value_type reduce_rec(const Assoc_op& op, const Lift_func& lift, value_type v, const_array_ref xs,
                      long lo, long hi) {
  using contr_type = reduce_controller_type<Assoc_op,Lift_func>;
  value_type result = v;
  auto seq = [&] {
    value_type x = v;
    for (long i = 0; i < xs.size(); i++)
      x = op(x, lift(xs[i]));
    result = x;
  };
  long n = hi-lo;
  par::cstmt(contr_type::contr, [&] { return n; }, [&] {
    if (n < 2) {
      seq();
    } else {
      long m = (lo+hi)/2;
      value_type v1,v2;
      par::fork2([&] {
        v1 = reduce_rec(op, lift, v, xs, lo, m);
      }, [&] {
        v2 = reduce_rec(op, lift, v, xs, m, hi);
      });
      result = op(v1, v2);
    }
  }, seq);
  return result;
}

template <class Assoc_op, class Lift_func>
value_type reduce(const Assoc_op& op, const Lift_func& lift, value_type id, const_array_ref xs) {
  return reduce_rec(op, lift, id, xs, 0, xs.size());
}

template <class Assoc_op>
value_type reduce(const Assoc_op& op, value_type id, const_array_ref xs) {
  return reduce(op, identity_fct, id, xs);
}

value_type sum(value_type id, const_array_ref xs) {
  return reduce(plus_fct, id, xs);
}

value_type sum(const_array_ref xs) {
  return reduce(plus_fct, 0, xs);
}

value_type max(const_array_ref xs) {
  return reduce(max_fct, LONG_MIN, xs);
}

value_type min(const_array_ref xs) {
  return reduce(min_fct, LONG_MAX, xs);
}

template <class Assoc_op, class Lift_func>
class scan_controller_type {
public:
  static controller_type contr;
  static loop_controller_type lp1_contr;
  static loop_controller_type lp2_contr;
  static loop_controller_type lp3_contr;
};
template <class Assoc_op, class Lift_func>
controller_type scan_controller_type<Assoc_op,Lift_func>::contr("scan"+
                                                                par::string_of_template_arg<Assoc_op>()+
                                                                par::string_of_template_arg<Lift_func>());
template <class Assoc_op, class Lift_func>
loop_controller_type scan_controller_type<Assoc_op,Lift_func>::lp1_contr("scan_lp1"+
                                                                         par::string_of_template_arg<Assoc_op>()+
                                                                         par::string_of_template_arg<Lift_func>());
template <class Assoc_op, class Lift_func>
loop_controller_type scan_controller_type<Assoc_op,Lift_func>::lp2_contr("scan_lp2"+
                                                                         par::string_of_template_arg<Assoc_op>()+
                                                                         par::string_of_template_arg<Lift_func>());
template <class Assoc_op, class Lift_func>
loop_controller_type scan_controller_type<Assoc_op,Lift_func>::lp3_contr("scan_lp3"+
                                                                         par::string_of_template_arg<Assoc_op>()+
                                                                         par::string_of_template_arg<Lift_func>());

class scan_result {
public:
  array prefix;
  value_type last;
  
  scan_result() { }
  scan_result(array&& prefix, value_type last) {
    this->prefix = std::move(prefix);
    this->last = last;
  }
  
  // disable copying
  scan_result(const scan_result&);
  scan_result& operator=(const scan_result&);
  
  scan_result(scan_result&& other) {
    prefix = std::move(other.prefix);
    last = std::move(other.last);
  }
  
  scan_result& operator=(scan_result&& other) {
    prefix = std::move(prefix);
    last = std::move(last);
    return *this;
  }
  
};

template <class Assoc_op, class Lift_func>
scan_result scan(const Assoc_op& op, const Lift_func& lift, value_type id, const_array_ref xs, bool inclusive) {
  using contr_type = scan_controller_type<Assoc_op,Lift_func>;
  long n = xs.size();
  array result = array(n);
  value_type x = id;
  auto seq = [&] {
    if (inclusive) {
      for (long i = 0; i < n; i++) {
        x = op(x, lift(xs[i]));
        result[i] = x;
      }
    } else {
      for (long i = 0; i < n; i++) {
        result[i] = x;
        x = op(x, lift(xs[i]));
      }
    }
  };
  par::cstmt(contr_type::contr, [&] { return n; }, [&] {
    if (n < 2) {
      seq();
    } else {
      long m = n/2;
      array sums = array(m);
      par::parallel_for(contr_type::lp1_contr, 0l, m, [&] (long i) {
        sums[i] = op(lift(xs[i*2]), lift(xs[i*2+1]));
      });
      scan_result scans = scan(op, lift, id, sums, inclusive);
      par::parallel_for(contr_type::lp2_contr, 0l, m, [&] (long i) {
        result[2*i] = scans.prefix[i];
      });
      par::parallel_for(contr_type::lp3_contr, 0l, m, [&] (long i) {
        result[2*i+1] = op(lift(xs[2*i]), result[2*i]);
      });
      long last = n-1;
      if (n == 2*m + 1) {
        result[last] = op(lift(xs[last-1]), result[last-1]);
      }
      x = op(result[last], lift(xs[last]));
    }
  }, seq);
  return scan_result(std::move(result), x);
}

template <class Assoc_op>
scan_result scan(const Assoc_op& op, value_type id, const_array_ref xs) {
  return scan(op, identity_fct, id, xs, false);
}

scan_result partial_sums(value_type id, const_array_ref xs) {
  return scan(plus_fct, identity_fct, id, xs, false);
}

scan_result partial_sums(const_array_ref xs) {
  return partial_sums(0l, xs);
}

array partial_sums_inclusive(value_type id, const_array_ref xs) {
  scan_result r = scan(plus_fct, identity_fct, id, xs, true);
  array tmp;
  tmp = std::move(r.prefix);
  return tmp;
}

array partial_sums_inclusive(const_array_ref xs) {
  return partial_sums_inclusive(0l, xs);
}

loop_controller_type pack_contr("pack");

array pack_nonempty(const_array_ref flags, const_array_ref xs) {
  assert(xs.size() == flags.size());
  assert(xs.size() > 0l);
  long n = xs.size();
  scan_result offsets = partial_sums(flags);
  value_type m = offsets.last;
  array result = array(m);
  par::parallel_for(pack_contr, 0l, n, [&] (long i) {
    if (flags[i] == 1)
      result[offsets.prefix[i]] = xs[i];
  });
  return result;
}

array pack(const_array_ref flags, const_array_ref xs) {
  assert(flags.size() == xs.size());
  return (xs.size() > 0) ? pack_nonempty(flags, xs) : empty();
}

template <class Pred>
array filter(const Pred& p, const_array_ref xs) {
  return pack(map(p, xs), xs);
}

array just_evens(const_array_ref xs) {
  return filter(is_even_fct, xs);
}

/***********************************************************************/

#endif /*! _MINICOURSE_ARRAY_H_ */