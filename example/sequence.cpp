#include <iostream>
#include <limits.h>
#include <vector>
#include <memory>

#include "benchmark.hpp"
#include "granularity.hpp"

/*
 todo
 
   - add check to ensure unique names for controller objects
      + can be dynamic; just check all names at pasl-init time
   - quickcheck?
   - small examples
   - look into parallel random number generation
 
 */

namespace par = pasl::sched::granularity;

using controller_type = par::control_by_prediction;
using loop_controller_type = par::loop_by_eager_binary_splitting<controller_type>;

/*---------------------------------------------------------------------*/

using value_type = long;

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

/*---------------------------------------------------------------------*/

array duplicate(const_array_ref xs) {
  long m = xs.size() * 2;
  return tabulate([&] (long i) { return xs[i/2]; }, m);
}

array ktimes(const_array_ref xs, long k) {
  long m = xs.size() * k;
  return tabulate([&] (long i) { return xs[i/k]; }, m);
}

/*---------------------------------------------------------------------*/

const value_type open_paren = 1l;
const value_type close_paren = -1l;

value_type c2v(char c) {
  assert(c == '(' || c == ')');
  return (c == '(') ? open_paren : close_paren;
}

char v2c(value_type v) {
  assert(v == open_paren || v == close_paren);
  return (v == open_paren) ? '(' : ')';
}

array from_parens(std::string str) {
  long sz = str.size();
  array tmp = array(sz);
  for (long i = 0; i < sz; i++)
    tmp[i] = c2v(str[i]);
  return tmp;
}

std::string to_parens(const_array_ref xs) {
  long sz = xs.size();
  std::string str(sz, 'x');
  for (long i = 0; i < sz; i++)
    str[i] = v2c(xs[i]);
  return str;
}

/*---------------------------------------------------------------------*/

using mychar = value_type;
using mystring = array;
using const_mystring_ref = const_array_ref;

mychar char_compare(mychar x, mychar y) {
  if (x < y)
    return -1l;
  else if (x == y)
    return 0l;
  else
    return 1l;
}

value_type string_compare(const_mystring_ref xs, const_mystring_ref ys) {
  long n = xs.size();
  long m = ys.size();
  if (n < m)
    return -1l * string_compare(ys, xs);
  array cs = map2([&] (value_type x, value_type y) { return char_compare(x, y); }, xs, ys);
  return reduce([&] (value_type a, value_type b) { return (a == 0) ? b : a; }, 0, cs);
}

bool matching_parens(const_mystring_ref parens) {
  long n = parens.size();
  // ks[i]: nbr. of open parens in positions < i
  array ks = scan(plus_fct, 0l, parens);
  auto is_nonneg_fct = [] (value_type x) {
    return x >= 0;
  };
  long last = n-1;
  if (ks[last] + parens[last] != 0)
    return false;
  return reduce(and_fct, is_nonneg_fct, true, ks);
}

bool matching_parens(const std::string& xs) {
  return matching_parens(from_parens(xs));
}

/*---------------------------------------------------------------------*/

array gen_random_array(long n) {
  array tmp = array(n);
  for (long i = 0; i < n; i++)
    tmp[i] = random() % 1024;
  return tmp;
}

long nlogn(long n) {
  return pasl::data::estimator::annotation::nlgn(n);
}

void in_place_quicksort(array_ref xs, long lo, long hi) {
  if (hi-lo > 0)
    std::sort(&xs[lo], &xs[hi-1]+1);
}

void in_place_quicksort(array_ref xs) {
  in_place_quicksort(xs, 0l, xs.size());
}

controller_type quicksort_contr("quicksort");

array quicksort(const_array_ref xs) {
  long n = xs.size();
  array result = empty();
  auto seq = [&] {
    result = copy(xs);
    in_place_quicksort(result);
  };
  par::cstmt(quicksort_contr, [&] { return nlogn(n); }, [&] {
    if (n < 2) {
      seq();
    } else {
      long m = n/2;
      value_type p = xs[m];
      array less = filter([&] (value_type x) { return x < p; }, xs);
      array equal = filter([&] (value_type x) { return x == p; }, xs);
      array greater = filter([&] (value_type x) { return x > p; }, xs);
      array left = array(0);
      array right = array(0);
      par::fork2([&] {
        left = quicksort(less);
      }, [&] {
        right = quicksort(greater);
      });
      result = concat(left, concat(equal, right));
    }
  });
  return result;
}

void merge_seq(const_array_ref xs, array_ref tmp,
               long lo1_xs, long hi1_xs,
               long lo2_xs, long hi2_xs,
               long lo_tmp) {
  long i = lo1_xs;
  long j = lo2_xs;
  long z = lo_tmp;
  
  // merge two halves until one is empty
  while (i < hi1_xs and j < hi2_xs)
    tmp[z++] = (xs[i] < xs[j]) ? xs[i++] : xs[j++];
  
  // copy remaining items
  prim::copy(&xs[0], &tmp[0], i, hi1_xs, z);
  prim::copy(&xs[0], &tmp[0], j, hi2_xs, z);
}

void merge_seq(array_ref xs, array_ref tmp,
               long lo, long mid, long hi) {
  merge_seq(xs, tmp, lo, mid, mid, hi, lo);
  // copy back to source array
  prim::copy(&tmp[0], &xs[0], lo, hi, lo);
}

long lower_bound(const_array_ref xs, long lo, long hi, value_type val) {
  const value_type* first_xs = &xs[0];
  return std::lower_bound(first_xs+lo, first_xs+hi, val)-first_xs;
}

controller_type merge_contr("merge");

void merge_par(const_array_ref xs, array_ref tmp,
               long lo1_xs, long hi1_xs,
               long lo2_xs, long hi2_xs,
               long lo_tmp) {
  long n1 = hi1_xs-lo1_xs;
  long n2 = hi2_xs-lo2_xs;
  auto seq = [&] {
    merge_seq(xs, tmp, lo1_xs, hi1_xs, lo2_xs, hi2_xs, lo_tmp);
  };
  par::cstmt(merge_contr, [&] { return n1+n2; }, [&] {
    if (n1 < n2) {
      // to ensure that the first subarray being sorted is the larger or the two
      merge_par(xs, tmp, lo2_xs, hi2_xs, lo1_xs, hi1_xs, lo_tmp);
    } else if (n1 == 1) {
      if (n2 == 0) {
        // a1 singleton; a2 empty
        tmp[lo_tmp] = xs[lo1_xs];
      } else {
        // both singletons
        tmp[lo_tmp+0] = std::min(xs[lo1_xs], xs[lo2_xs]);
        tmp[lo_tmp+1] = std::max(xs[lo1_xs], xs[lo2_xs]);
      }
    } else {
      // select pivot positions
      long mid1_xs = (lo1_xs+hi1_xs)/2;
      long mid2_xs = lower_bound(xs, lo2_xs, hi2_xs, xs[mid1_xs]);
      // number of items to be treated by the first parallel call
      long k = (mid1_xs-lo1_xs) + (mid2_xs-lo2_xs);
      par::fork2([&] {
        merge_par(xs, tmp, lo1_xs, mid1_xs, lo2_xs, mid2_xs, lo_tmp);
      }, [&] {
        merge_par(xs, tmp, mid1_xs, hi1_xs, mid2_xs, hi2_xs, lo_tmp+k);
      });
    }}, seq);
}

void merge(array_ref xs, array_ref tmp,
           long lo, long mid, long hi) {
  merge_par(xs, tmp, lo, mid, mid, hi, lo);
  // copy back to source array
  prim::pcopy(&tmp[0], &xs[0], lo, hi, lo);
}

controller_type mergesort_contr("mergesort");

void mergesort_par(array_ref xs, array_ref tmp,
                   long lo, long hi) {
  long n = hi-lo;
  auto seq = [&] {
    std::sort(&xs[0]+lo, &xs[0]+hi);
  };
  par::cstmt(mergesort_contr, [&] { return nlogn(n); }, [&] {
    if (n == 0) {
      return;
    } else if (n == 1) {
      tmp[lo] = xs[lo];
    } else {
      long mid = (lo+hi)/2;
      par::fork2([&] {
        mergesort_par(xs, tmp, lo, mid);
      }, [&] {
        mergesort_par(xs, tmp, mid, hi);
      });
      merge(xs, tmp, lo, mid, hi);
    }
  }, seq);
}

array mergesort(const_array_ref xs) {
  array tmp = copy(xs);
  long n = xs.size();
  array scratch = array(n);
  mergesort_par(tmp, scratch, 0l, n);
  return tmp;
}

/*---------------------------------------------------------------------*/

using thunk_type = std::function<void ()>;
using benchmark_type = std::pair<std::pair<thunk_type,thunk_type>,
                                 std::pair<thunk_type, thunk_type>>;

benchmark_type make_benchmark(thunk_type init, thunk_type bench,
                              thunk_type output, thunk_type destroy) {
  return std::make_pair(std::make_pair(init, bench),
                        std::make_pair(output, destroy));
}

void bench_init(const benchmark_type& b) {
  b.first.first();
}

void bench_run(const benchmark_type& b) {
  b.first.second();
}

void bench_output(const benchmark_type& b) {
  b.second.first();
}

void bench_destroy(const benchmark_type& b) {
  b.second.second();
}

/*---------------------------------------------------------------------*/

benchmark_type scan_bench() {
  long n = pasl::util::cmdline::parse_or_default_long("n", 1l<<20);
  array* inp = new array(0);
  array* outp = new array(0);
  auto init = [=] {
    array t = fill(n, 1);
    inp->swap(t);
  };
  auto bench = [=] {
    array t = partial_sums(*inp);
    outp->swap(t);
  };
  auto output = [=] {
    std::cout << "result\t" << (*outp)[outp->size()-1] << std::endl;
  };
  auto destroy = [=] {
    delete inp;
    delete outp;
  };
  return make_benchmark(init, bench, output, destroy);
}

/*---------------------------------------------------------------------*/

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

  std::cout << matching_parens("()(())(") << std::endl;
  std::cout << matching_parens("()(())((((()()))))") << std::endl;
  
  std::cout << partial_sums(fill(6, 1)) << std::endl;

  array rs = gen_random_array(15);
  std::cout << rs << std::endl;
  std::cout << mergesort(rs) << std::endl;
  std::cout << quicksort(rs) << std::endl;
}

int main(int argc, char** argv) {
  
  benchmark_type bench;

  auto init = [&] {
    pasl::util::cmdline::argmap<benchmark_type> m;
    m.add("scan", scan_bench());
    bench = m.find_by_arg("bench");
    bench_init(bench);
  };
  auto run = [&] (bool) {
    bench_run(bench);
  };
  auto output = [&] {
    bench_output(bench);
  };
  auto destroy = [&] {
    bench_destroy(bench);
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}
