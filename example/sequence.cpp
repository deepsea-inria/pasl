#include <iostream>
#include <limits.h>
#include <vector>
#include <memory>

#include "benchmark.hpp"

/*---------------------------------------------------------------------*/

using value_type = long;

namespace prim {
  
  using pointer_type = value_type*;
  using const_pointer_type = const value_type*;
  
  void pcopy(const_pointer_type first, const_pointer_type last, pointer_type d_first) {
    const size_t cutoff = 10000;
    size_t nb = last-first;
    if (nb <= cutoff) {
      std::copy(first, last, d_first);
    } else {
      size_t m = nb/2;
      pcopy(first,   first+m, d_first);
      pcopy(first+m, last,    d_first+m);
    }
  }
  
  void pcopy(const_pointer_type src, pointer_type dst,
             long lo_src, long hi_src, long lo_dst) {
    pcopy(&src[lo_src], &src[hi_src], &dst[lo_dst]);
  }
  
}

/*---------------------------------------------------------------------*/

class array {
private:
  
  class Deleter {
  public:
    void operator()(value_type* ptr) {
      free(ptr);
    }
  };
  
  std::unique_ptr<value_type[], Deleter> ptr;
  const long sz = -1l;
  
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
  
  array(long sz)
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
  
};

using array_ref = array&;
using const_array_ref = const array&;

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

/*---------------------------------------------------------------------*/

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

array take(const_array_ref xs, long n) {
  assert(n <= xs.size());
  assert(n >= 0);
  array tmp = array(n);
  if (n > 0)
    prim::pcopy(&xs[0], &xs[n-1]+1, &tmp[0]);
  return tmp;
}

array drop(const_array_ref xs, long n) {
  long sz = xs.size();
  assert(n <= sz);
  assert(n >= 0);
  long m = sz-n;
  array tmp = array(m);
  if (m > 0)
    prim::pcopy(&xs[n], &xs[n+m-1]+1, &tmp[0]);
  return tmp;
}

array copy(const_array_ref xs) {
  return take(xs, xs.size());
}

template <class Func>
void map_rec(const Func& f, array_ref dst, const_array_ref xs, long lo, long hi) {
  long n = hi - lo;
  auto seq = [&] {
    for (long i = lo; i < hi; i++)
      dst[i] = f(xs[i]);
  };
  if (n < 2) {
    seq();
  } else {
    long m = (lo+hi)/2;
    map_rec(f, dst, xs, lo, m);
    map_rec(f, dst, xs, m, hi);
  }
}

template <class Func>
array map(const Func& f, const_array_ref xs) {
  long n = xs.size();
  array tmp = array(n);
  map_rec(f, tmp, xs, 0, n);
  return tmp;
}

template <class Assoc_op, class Lift_func>
value_type reduce_rec(const Assoc_op& op, const Lift_func& lift, value_type v, const_array_ref xs,
                      long lo, long hi) {
  auto seq = [&] {
    value_type x = v;
    for (long i = 0; i < xs.size(); i++)
      x = op(x, lift(xs[i]));
    return x;
  };
  long n = hi - lo;
  if (n < 2) {
    return seq();
  } else {
    long m = (lo+hi)/2;
    value_type v1,v2;
    v1 = reduce_rec(op, lift, v, xs, lo, m);
    v2 = reduce_rec(op, lift, v, xs, m, hi);
    return op(v1, v2);
  }
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
  if (n < 2) {
    seq();
  } else {
    long m = n/2;
    array sums = array(m);
    for (long i = 0; i < m; i++)
      sums[i] = op(lift(xs[i*2]), lift(xs[i*2+1]));
    array scans = scan(op, lift, id, sums);
    for (long i = 0; i < m; i++)
      tmp[2*i] = scans[i];
    for (long i = 0; i < m; i++)
      tmp[2*i+1] = op(lift(xs[2*i]), tmp[2*i]);
    if (n == 2*m + 1) {
      long last = n-1;
      tmp[last] = op(lift(xs[last-1]), tmp[last-1]);
    }
  }
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

array pack(const_array_ref flags, const_array_ref xs) {
  array offsets = partial_sums(flags);
  long n = xs.size();
  long last = n-1;
  value_type m = offsets[last] + flags[last];
  array result = array(m);
  for (long i = 0; i < n; i++)
    if (flags[i] == 1)
      result[offsets[i]] = xs[i];
  return result;
}

template <class Pred>
array filter(const Pred& p, const_array_ref xs) {
  return pack(map(p, xs), xs);
}

array just_evens(const_array_ref xs) {
  return filter(is_even_fct, xs);
}

/*---------------------------------------------------------------------*/

array duplicate(const_array_ref xs) {
  long n = xs.size();
  array tmp(n * 2);
  for (long i = 0; i < n; i++)
    tmp[i*2] = xs[i];
  for (long i = 0; i < n; i++)
    tmp[i*2+1] = xs[i];
  return tmp;
}

array ktimes(const_array_ref xs, long k) {
  long n = xs.size();
  long m = n * k;
  array flags(m);
  for (long i = 0; i < m; i++)
    flags[i] = 0;
  for (long i = 1; i < n; i++)
    flags[i*k-1] = 1l;
  array offsets = partial_sums(flags);
  array result = array(m);
  for (long i = 0; i < m; i++)
    result[i] = xs[offsets[i]];
  return result;
}

template <class Pred, class Assoc_op, class Lift_func>
value_type reduce_filter(const Pred& p, const Assoc_op& op, const Lift_func& lift,
                         value_type id, const_array_ref xs) {
  return reduce(op, lift, id, filter(p, xs));
}

template <class Pred, class Func>
array map_filter(const Pred& p, const Func& f, const_array_ref xs) {
  return map(f, filter(p, xs));
}

/*---------------------------------------------------------------------*/

const value_type open_paren = 1l;
const value_type close_paren = -1l;

value_type p(char c) {
  assert(c == '(' || c == ')');
  value_type v = open_paren;
  if (c == ')')
    v = close_paren;
  return v;
}

char u(value_type v) {
  assert(v == open_paren || v == close_paren);
  char c = '(';
  if (v == close_paren)
    c = ')';
  return c;
}

array from_parens(std::string str) {
  long sz = str.size();
  array tmp = array(sz);
  for (long i = 0; i < sz; i++)
    tmp[i] = p(str[i]);
  return tmp;
}

std::string to_parens(const_array_ref xs) {
  long sz = xs.size();
  std::string str(sz, 'x');
  for (long i = 0; i < sz; i++)
    str[i] = u(xs[i]);
  return str;
}

bool matching_parens(const_array_ref parens) {
  long n = parens.size();
  // ks[i]: nbr. of open parens in positions < i
  array ks = scan(plus_fct, 0l, parens);
  auto lift_fct = [] (value_type x) {
    return x >= 0;
  };
  long last = n-1;
  if (ks[last] + parens[last] != 0)
    return false;
  return reduce(and_fct, lift_fct, true, ks);
}

bool matching_parens(const std::string& xs) {
  return matching_parens(from_parens(xs));
}

/*---------------------------------------------------------------------*/

void doit2() {
  array xs = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
  std::cout << partial_sums(xs) << std::endl;
  std::cout << xs.size() << std::endl;
}

void doit() {
  array xs = { 0, 1, 2, 3, 4, 5, 6 };
  std::cout << "xs=" << xs << std::endl;
  array ys = map(plus1_fct, xs);
  std::cout << "xs(copy)=" << copy(xs) << std::endl;
  std::cout << "ys=" << ys << std::endl;
  value_type v = sum(ys);
  std::cout << "v=" << v << std::endl;
  array zs = partial_sums(xs);
  std::cout << "zs=" << zs << std::endl;
  std::cout << "max=" << max(ys) << std::endl;
  std::cout << "min=" << min(ys) << std::endl;
  std::cout << "tmp=" << map(plus1_fct, array({100, 101})) << std::endl;
  std::cout << "evens=" << just_evens(ys) << std::endl;
  
  std::cout << "take3=" << take(xs, 3) << std::endl;
  std::cout << "drop4=" << drop(xs, 4) << std::endl;
  std::cout << "take0=" << take(xs, 0) << std::endl;
  std::cout << "drop 7=" << drop(xs, 7) << std::endl;

  
  std::cout << "parens=" << to_parens(from_parens("()()((()))")) << std::endl;
  std::cout << "matching=" << matching_parens(from_parens("()()((()))")) << std::endl;
  std::cout << "not_matching=" << matching_parens(from_parens("()(((()))")) << std::endl;

  std::cout << "empty=" << array({}) << std::endl;
  
  std::cout << "duplicate(xs)" << duplicate(xs) << std::endl;
  std::cout << "3x(xs)" << ktimes(xs,3) << std::endl;
  std::cout << "4x(xs)" << ktimes(xs,4) << std::endl;

  std::cout << "reduce_filter=" << reduce_filter(is_even_fct, plus_fct, identity_fct, 0, xs) << std::endl;
  std::cout << "map_filter=" << map_filter(is_even_fct, plus1_fct, xs) << std::endl;
  
  std::cout << matching_parens("()(())(") << std::endl;
  std::cout << matching_parens("()(())((((()()))))") << std::endl;

}

int main(int argc, char** argv) {

  auto init = [&] {

  };
  auto run = [&] (bool) {
    doit2();
  };
  auto output = [&] {
  };
  auto destroy = [&] {
    ;
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}
