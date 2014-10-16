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

#include "granularity.hpp"

#ifndef _MINICOURSE_ARRAY_H_
#define _MINICOURSE_ARRAY_H_

/***********************************************************************/

namespace par = pasl::sched::granularity;

using controller_type = par::control_by_prediction;
using loop_controller_type = par::loop_by_eager_binary_splitting<controller_type>;

using value_type = long;

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
    value_type* p = (value_type*)malloc(sizeof(value_type) * sz);
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
  
  array(const array& other);
  array& operator=(const array& other);
  
  array& operator=(array&& other) {
    // todo: make sure that the following does not create a memory leak
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
  out << "[";
  size_t sz = xs.size();
  for (long i = 0; i < sz; i++) {
    out << xs[i];
    if (i+1 < sz)
      out << ", ";
  }
  out << "]";
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

array concat(const_array_ref xs1, const_array_ref xs2) {
  long n1 = xs1.size();
  long n2 = xs2.size();
  long n = n1+n2;
  array result = array(n);
  par::parallel_for(concat_contr, 0l, n1, [&] (long i) {
    result[i] = xs1[i];
  });
  par::parallel_for(concat_contr, 0l, n2, [&] (long i) {
    result[i+n1] = xs2[i];
  });
  return result;
}

loop_controller_type tabulate_contr("tabulate");

template <class Func>
array tabulate(const Func& f, long n) {
  array tmp = array(n);
  par::parallel_for(tabulate_contr, 0l, n, [&] (long i) {
    tmp[i] = f(i);
  });
  return tmp;
}

template <class Func>
array map(const Func& f, const_array_ref xs) {
  return tabulate([&] (long i) { return f(xs[i]); }, xs.size());
}

template <class Func>
array map2(const Func& f, const_array_ref xs, const_array_ref ys) {
  long n = std::min(xs.size(), ys.size());
  return tabulate([&] (long i) { return f(xs[i], ys[i]); }, n);
}

controller_type reduce_contr("reduce");

template <class Assoc_op, class Lift_func>
value_type reduce_rec(const Assoc_op& op, const Lift_func& lift, value_type v, const_array_ref xs,
                      long lo, long hi) {
  value_type result = v;
  auto seq = [&] {
    value_type x = v;
    for (long i = 0; i < xs.size(); i++)
      x = op(x, lift(xs[i]));
    result = x;
  };
  long n = hi - lo;
  par::cstmt(reduce_contr, [&] { return n; }, [&] {
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

controller_type scan_contr("scan");
loop_controller_type scan_lp1_contr("scan_lp1");
loop_controller_type scan_lp2_contr("scan_lp2");
loop_controller_type scan_lp3_contr("scan_lp3");

template <class Assoc_op, class Lift_func>
array scan(const Assoc_op& op, const Lift_func& lift, value_type id, const_array_ref xs) {
  long n = xs.size();
  array tmp = array(n);
  auto seq = [&] {
    value_type x = id;
    for (long i = 0; i < n; i++) {
      tmp[i] = x;
      x = op(x, lift(xs[i]));
    }
  };
  par::cstmt(scan_contr, [&] { return n; }, [&] {
    if (n < 2) {
      seq();
    } else {
      long m = n/2;
      array sums = array(m);
      par::parallel_for(scan_lp1_contr, 0l, m, [&] (long i) {
        sums[i] = op(lift(xs[i*2]), lift(xs[i*2+1]));
      });
      array scans = scan(op, lift, id, sums);
      par::parallel_for(scan_lp2_contr, 0l, m, [&] (long i) {
        tmp[2*i] = scans[i];
      });
      par::parallel_for(scan_lp3_contr, 0l, m, [&] (long i) {
        tmp[2*i+1] = op(lift(xs[2*i]), tmp[2*i]);
      });
      if (n == 2*m + 1) {
        long last = n-1;
        tmp[last] = op(lift(xs[last-1]), tmp[last-1]);
      }
    }
  }, seq);
  return tmp;
}

template <class Assoc_op>
array scan(const Assoc_op& op, value_type id, const_array_ref xs) {
  return scan(op, identity_fct, id, xs);
}

array partial_sums(value_type id, const_array_ref xs) {
  return scan(plus_fct, identity_fct, id, xs);
}

array partial_sums(const_array_ref xs) {
  return scan(plus_fct, identity_fct, 0, xs);
}

loop_controller_type pack_contr("pack");

array pack(const_array_ref flags, const_array_ref xs) {
  array offsets = partial_sums(flags);
  long n = xs.size();
  long last = n-1;
  value_type m = offsets[last] + flags[last];
  array result = array(m);
  par::parallel_for(pack_contr, 0l, n, [&] (long i) {
    if (flags[i] == 1)
      result[offsets[i]] = xs[i];
  });
  return result;
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