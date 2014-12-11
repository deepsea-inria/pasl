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
#include "exercises.hpp"

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Examples from text */

void Graph_processing_examples() {
  std::cout << "Graph-processing examples" << std::endl;
  
  std::cout << "Example: Graph creation" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    adjlist graph = { mk_edge(0, 1), mk_edge(0, 3), mk_edge(5, 1), mk_edge(3, 0) };
    std::cout << graph << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Adjacency-list interface" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    adjlist graph = { mk_edge(0, 1), mk_edge(0, 3), mk_edge(5, 1), mk_edge(3, 0),
                      mk_edge(3, 5), mk_edge(3, 2), mk_edge(5, 3) };
    std::cout << "nb_vertices = " << graph.get_nb_vertices() << std::endl;
    std::cout << "nb_edges = " << graph.get_nb_edges() << std::endl;
    std::cout << "out_edges of vertex 3:" << std::endl;
    neighbor_list out_edges_of_3 = graph.get_out_edges_of(3);
    for (long i = 0; i < graph.get_out_degree_of(3); i++)
      std::cout << " " << out_edges_of_3[i];
    std::cout << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Accessing the contents of atomic memory cells" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    const long n = 3;
    std::atomic<bool> visited[n];
    long v = 2;
    visited[v].store(false);
    std::cout << visited[v].load() << std::endl;
    visited[v].store(true);
    std::cout << visited[v].load() << std::endl;
  }
  std::cout << "-----------------------" << std::endl;

  std::cout << "Example: Compare and exchange" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    const long n = 3;
    std::atomic<bool> visited[n];
    long v = 2;
    visited[v].store(false);
    bool orig = false;
    bool was_successful = visited[v].compare_exchange_strong(orig, true);
    std::cout << was_successful << std::endl;
  }
  std::cout << "-----------------------" << std::endl;

  std::cout << "Example: Edge map" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    adjlist graph = { mk_edge(0, 1), mk_edge(0, 3), mk_edge(5, 1), mk_edge(3, 0),
                      mk_edge(3, 5), mk_edge(3, 2), mk_edge(5, 3) };
    const long n = graph.get_nb_vertices();
    std::atomic<bool> visited[n];
    for (long i = 0; i < n; i++)
      visited[i] = false;
    visited[0].store(true);
    visited[1].store(true);
    visited[3].store(true);
    sparray in_frontier = { 3 };
    sparray out_frontier = edge_map(graph, visited, in_frontier);
    std::cout << out_frontier << std::endl;
    sparray out_frontier2 = edge_map(graph, visited, in_frontier);
    std::cout << out_frontier2 << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Parallel BFS" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    adjlist graph = { mk_edge(0, 1), mk_edge(0, 3), mk_edge(5, 1), mk_edge(3, 0),
                      mk_edge(3, 5), mk_edge(3, 2), mk_edge(5, 3) };
    sparray visited = bfs(graph, 0);
    std::cout << visited << std::endl;
    sparray visited2 = bfs(graph, 2);
    std::cout << visited2 << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
}

/*---------------------------------------------------------------------*/
/* Examples from exercises */

void merge_exercise_example() {
  sparray xs = { 2, 4, 6, 8 };
  sparray ys = { 5, 5, 13, 21, 23 };
  long lo_xs = 1;
  long hi_xs = 3;
  long lo_ys = 0;
  long hi_ys = 4;
  long lo_tmp = 0;
  long hi_tmp = (hi_xs-lo_xs) + (hi_ys+lo_ys);
  sparray tmp = sparray(hi_tmp);
  exercises::merge_par(xs, ys, tmp, lo_xs, hi_xs, lo_ys, hi_ys, lo_tmp);
  std::cout << "xs =" << slice(xs, lo_xs, hi_xs) << std::endl;
  std::cout << "ys =" << slice(ys, lo_ys, hi_ys) << std::endl;
  std::cout << "tmp = " << tmp << std::endl;
  /*
   When your merge exercise is complete, the output should be
   the following:
   
   xs ={ 4, 6 }
   ys ={ 5, 5, 13, 21 }
   tmp = { 4, 5, 5, 6, 13, 21 }
   */
}

sparray just_evens(const sparray& xs) {
  return filter(is_even_fct, xs);
}

void doit() {
  
//  sparray test = { -2, 1, -3, 4, -1, 2, 1, -5, 4 };
  sparray test = { -1l, 3, 5, 3, -3l };
  
  std::cout << "mcss_par=" << mcss_par(test) << std::endl;
  std::cout << "mcss_seq=" << mcss_seq(test) << std::endl;
  
  sparray mtx = { 1, 2, 3, 4 };
  sparray vec = { 5, 6 };
  std::cout << dmdvmult(mtx, vec) << std::endl;
  
  long n = 20;
  std::cout << "fib(" << n << ")=" << fib_par(n) << std::endl;
  
  sparray empty;
  std::cout << "empty=" << empty << std::endl;
  std::cout << "sparray=" << sparray({1, 2}) << std::endl;

  sparray xs = { 0, 1, 2, 3, 4, 5, 6 };
  std::cout << "xs=" << xs << std::endl;
  scan_excl_result zs = prefix_sums_excl(xs);
  std::cout << "zs=" << zs.partials << " " << zs.total << std::endl;

  sparray ys = map(plus1_fct, xs);
  std::cout << "xs(copy)=" << copy(xs) << std::endl;
  std::cout << "ys=" << ys << std::endl;
  value_type v = sum(ys);
  std::cout << "v=" << v << std::endl;

  std::cout << "max=" << max(ys) << std::endl;
  std::cout << "min=" << min(ys) << std::endl;
  std::cout << "tmp=" << map(plus1_fct, sparray({100, 101})) << std::endl;
  std::cout << "evens=" << just_evens(ys) << std::endl;
  
  std::cout << "take3=" << take(xs, 3) << std::endl;
  std::cout << "drop4=" << drop(xs, 4) << std::endl;
  std::cout << "take0=" << take(xs, 0) << std::endl;
  std::cout << "drop 7=" << drop(xs, 7) << std::endl;
  
  std::cout << "parens=" << to_parens(from_parens("()()((()))")) << std::endl;
  std::cout << "matching=" << matching_parens(from_parens("()()((()))")) << std::endl;
  std::cout << "not_matching=" << matching_parens(from_parens("()(((()))")) << std::endl;
  
  std::cout << "empty=" << sparray({}) << std::endl;
  
  std::cout << "duplicate(xs)" << duplicate(xs) << std::endl;
  std::cout << "3x(xs)" << ktimes(xs,3) << std::endl;
  std::cout << "4x(xs)" << ktimes(xs,4) << std::endl;
  
  std::cout << matching_parens("()(())(") << std::endl;
  std::cout << matching_parens("()(())((((()()))))") << std::endl;
  
  std::cout << prefix_sums_excl(fill(6, 1)).partials << std::endl;
  
  sparray rs = gen_random_sparray(15);
  std::cout << rs << std::endl;
  std::cout << mergesort(rs) << std::endl;
  std::cout << quicksort(rs) << std::endl;

  {
    adjlist graph = { mk_edge(0, 1), mk_edge(0, 3), mk_edge(5, 1), mk_edge(3, 0) };
    std::cout << graph << std::endl;
  }
  
  {
    adjlist graph = { mk_edge(0, 1), mk_edge(0, 3), mk_edge(5, 1), mk_edge(3, 0),
      mk_edge(3, 5), mk_edge(3, 2), mk_edge(5, 3) };
    std::cout << "nb_vertices = " << graph.get_nb_vertices() << std::endl;
    std::cout << "nb_edges = " << graph.get_nb_edges() << std::endl;
    std::cout << "out_edges of vertex 3:" << std::endl;
    neighbor_list out_edges_of_3 = graph.get_out_edges_of(3);
    for (long i = 0; i < graph.get_out_degree_of(3); i++)
      std::cout << " " << out_edges_of_3[i];
    std::cout << std::endl;
  }
}

/*---------------------------------------------------------------------*/
/* PASL Driver */

int main(int argc, char** argv) {
  
  auto init = [&] {
   
  };
  auto run = [&] (bool) {
    Graph_processing_examples();
  };
  auto output = [&] {
  };
  auto destroy = [&] {
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}

/***********************************************************************/

