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
#include "fib.hpp"
#include "mcss.hpp"
#include "numeric.hpp"

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Example applications */

array just_evens(const_array_ref xs) {
  return filter(is_even_fct, xs);
}

void doit() {
  
//  array test = { -2, 1, -3, 4, -1, 2, 1, -5, 4 };
  array test = { -1l, 3, 5, 3, -3l };
  
  std::cout << "mcss_par=" << mcss_par(test) << std::endl;
  std::cout << "mcss_seq=" << mcss_seq(test) << std::endl;
  
  array mtx = { 1, 2, 3, 4 };
  array vec = { 5, 6 };
  std::cout << dmdvmult(mtx, vec) << std::endl;
  
  long n = 20;
  std::cout << "fib(" << n << ")=" << fib_par(n) << std::endl;
  
  array empty;
  std::cout << "empty=" << empty << std::endl;
  std::cout << "array=" << array({1, 2}) << std::endl;

  array xs = { 0, 1, 2, 3, 4, 5, 6 };
  std::cout << "xs=" << xs << std::endl;
  scan_result zs = partial_sums(xs);
  std::cout << "zs=" << zs.prefix << " " << zs.last << std::endl;

  array ys = map(plus1_fct, xs);
  std::cout << "xs(copy)=" << copy(xs) << std::endl;
  std::cout << "ys=" << ys << std::endl;
  value_type v = sum(ys);
  std::cout << "v=" << v << std::endl;

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

