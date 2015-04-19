/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file exercises.hpp
 * \brief File that students are to use to enter solutions to the
 *  exercises that are assigned in the book.
 *
 */

#include <limits.h>

#include "sparray.hpp"
#include "sort.hpp"
#include "graph.hpp"

#ifndef _MINICOURSE_EXERCISES_H_
#define _MINICOURSE_EXERCISES_H_

/***********************************************************************/

namespace exercises {
  
/*---------------------------------------------------------------------*/
/* Stub functions for required exercises */
  
void map_incr(const value_type* source, value_type* dest, long n) {
  // todo: fill in
}
  
// source: pointer to the first item of the source array
// n: number of items in the source array
// seed: value to return in the case where `n` == 0
value_type max(const value_type* source, long n, value_type seed) {
  return seed; // todo: fill in
}
  
value_type max(const value_type* source, long n) {
  return max(source, n, VALUE_MIN);
}
  
value_type plus_rec(const value_type* source, long lo, long hi, value_type seed) {
  value_type result = seed;
  if (hi - lo == 0)
    result = seed;
  else if (hi - lo == 1)
    result = source[lo];
  else {
    long mid = (hi + lo) / 2;
    value_type a = plus_rec(source, lo, mid, seed);
    value_type b = plus_rec(source, mid, hi, seed);
    result = a + b;
  }
  return result;
}
  
value_type plus(const value_type* source, long n, value_type seed) {
  return plus_rec(source, 0, n, seed);
}
  
value_type plus(const value_type* source, long n) {
  return plus(source, n, (value_type)0);
}
  
template <class Assoc_comb_op>
value_type reduce(Assoc_comb_op op, value_type seed, const value_type* source, long n) {
  return seed; // todo fill in
}
  
sparray mergesort(const sparray& xs) {
  // students insert their solution here
  return copy(xs);
}
  
sparray edge_map_ex(const adjlist& graph,
                    std::atomic<bool>* visited,
                    const sparray& in_frontier) {
  // students insert their solution here
  return copy(in_frontier);
}
  
loop_controller_type bfs_par_init_contr("bfs_init");

sparray bfs_par_ex(const adjlist& graph, vtxid_type source) {
  long n = graph.get_nb_vertices();
  std::atomic<bool>* visited = my_malloc<std::atomic<bool>>(n);
  par::parallel_for(bfs_par_init_contr, 0l, n, [&] (long i) {
    visited[i].store(false);
  });
  visited[source].store(true);
  sparray cur_frontier = { source };
  while (cur_frontier.size() > 0)
    cur_frontier = edge_map_ex(graph, visited, cur_frontier);
  sparray result = tabulate([&] (value_type i) { return visited[i].load(); }, n);
  free(visited);
  return result;
}

sparray bfs(const adjlist& graph, vtxid_type source) {
#ifdef SEQUENTIAL_BASELINE
  return bfs_seq(graph, source);
#else
  return bfs_par_ex(graph, source);
#endif
}

/*---------------------------------------------------------------------*/
/* Stub functions for suggested exercises */

sparray duplicate(const sparray& xs) {
  return empty(); // todo: fill in
}

sparray ktimes(const sparray& xs, long k) {
  return empty(); // todo: fill in
}
  
sparray pack_ex(const sparray& flags, const sparray& xs) {
  return empty(); // todo: fill in
}
  
// This function is the one you will debug and benchmark.
// As such, use "-check filter_ex" and "-bench filter_ex"
// where appropriate.
template <class Predicate>
sparray filter(Predicate p, const sparray& xs) {
  return pack_ex(map(p, xs), xs);
}
  
} // end namespace

/***********************************************************************/

#endif /*! _MINICOURSE_EXERCISES_H_ */
