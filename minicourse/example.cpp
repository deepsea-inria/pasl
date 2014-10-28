/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file example.cpp
 * \brief Example applications
 *
 */

#include <math.h>
#include <thread>

#include "benchmark.hpp"
#include "dup.hpp"
#include "string.hpp"
#include "sort.hpp"
#include "graph.hpp"

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Maximum contiguous subsequence */

value_type mcss_seq(const_array_ref xs) {
  value_type max_ending_here = 0;
  value_type max_so_far = 0;
  for (long i = 0; i < xs.size(); i++) {
    value_type x = xs[i];
    max_ending_here = std::max(0l, max_ending_here+x);
    max_so_far = std::max(max_so_far, max_ending_here);
  }
  return max_so_far;
}

value_type mcss(const_array_ref xs) {
  array ys = partial_sums_inclusive(xs);
  scan_result m = scan(min_fct, 0l, ys);
  array zs = tabulate([&] (long i) { return ys[i]-m.prefix[i]; }, xs.size());
  return max(zs);
}

/*---------------------------------------------------------------------*/
/* Parallel fibonacci */

long de_moivre_fib(long n) {
  const double phi = 1.61803399;
  const double omega = -0.6180339887;
  double res = (pow(phi, (double)n)-pow(omega, (double)n))/sqrt(5);
  return (long)res;
}

long fib_seq(long n) {
  long result;
  if (n < 2) {
    result = n;
  } else {
    long a,b;
    a = fib_seq(n-1);
    b = fib_seq(n-2);
    result = a+b;
  }
  return result;
}

controller_type fib_contr("fib");

long fib_par(long n) {
  long result;
  auto seq = [&] {
    result = fib_seq(n);
  };
  par::cstmt(fib_contr, [n] { return de_moivre_fib(n); }, [&] {
    if (n < 2) {
      seq();
    } else {
      long a,b;
      par::fork2([&] {
        a = fib_par(n-1);
      }, [&] {
        b = fib_par(n-2);
      });
      result = a+b;
    }
  }, seq);
  return result;
}

/*---------------------------------------------------------------------*/
/* Example applications */

void doit() {
  
  array test = { -2, 1, -3, 4, -1, 2, 1, -5, 4 };
  std::cout << mcss(test) << std::endl;
  std::cout << mcss_seq(test) << std::endl;

  long n = 35;
  std::cout << "fib(" << n << ")=" << fib_par(n) << std::endl;
  
  array empty;
  std::cout << "empty=" << empty << std::endl;
  std::cout << "array=" << array({1, 2}) << std::endl;
  array xs = { 0, 1, 2, 3, 4, 5, 6 };
  std::cout << "xs=" << xs << std::endl;
  array ys = map(plus1_fct, xs);
  std::cout << "xs(copy)=" << copy(xs) << std::endl;
  std::cout << "ys=" << ys << std::endl;
  value_type v = sum(ys);
  std::cout << "v=" << v << std::endl;
  scan_result zs = partial_sums(xs);
  std::cout << "zs=" << zs.prefix << " " << zs.last << std::endl;
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
  
  std::cout << partial_sums(fill(6, 1)).prefix << std::endl;
  
  array rs = gen_random_array(15);
  std::cout << rs << std::endl;
  std::cout << mergesort(rs) << std::endl;
  std::cout << quicksort(rs) << std::endl;

  adjlist graph = { mk_edge(0, 1), mk_edge(0, 3), mk_edge(5, 1) };
  std::cout << graph << std::endl;
}

/*---------------------------------------------------------------------*/
/* PASL Driver */

int main(int argc, char** argv) {
  
  auto init = [&] {
   
  };
  auto run = [&] (bool) {
    doit();
  };
  auto output = [&] {
  };
  auto destroy = [&] {
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}

/***********************************************************************/

