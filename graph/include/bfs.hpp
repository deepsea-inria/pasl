/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file bfs.hpp
 *
 */

#include "edgelist.hpp"
#include "adjlist.hpp"
#include "pcontainer.hpp"

#ifndef _PASL_GRAPH_BFS_H_
#define _PASL_GRAPH_BFS_H_

/***********************************************************************/
/* TODO (and same in DFS code)

#ifdef STATS_IGNORE_VISITED_INIT
#define TRAVERSAL_BEGIN() 
  LOG_BASIC(ALGO_PHASE); STAT_IDLE(reset()); STAT_IDLE(enter_launch())
#else
#define TRAVERSAL_BEGIN() 
  LOG_BASIC(ALGO_PHASE)
#endif
  
==> replace below LOG_BASIC(ALGO_PHASE); with TRAVERSAL_BEGIN();

*/


/***********************************************************************/

#define PUSH_ZERO_ARITY_VERTICES 0



/***********************************************************************/

namespace pasl {
namespace graph {

/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/* Breadth first search of a graph in adjacency-list format; serial */

// BFS implemented using a single queue of vertex ids,
// represented as a flat array;
// the queue always contains exactly one special token
// which separates vertices at distance "dist" from those
// at distance "dist+1".
// preallocated array with tokens
template <class Adjlist_seq>
typename adjlist<Adjlist_seq>::vtxid_type*
bfs_by_array(const adjlist<Adjlist_seq>& graph,
             typename adjlist<Adjlist_seq>::vtxid_type source) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  vtxid_type* dists = data::mynew_array<vtxid_type>(nb_vertices);
  fill_array_seq(dists, nb_vertices, unknown);
  LOG_BASIC(ALGO_PHASE);
  vtxid_type* queue = data::mynew_array<vtxid_type>(2 * nb_vertices);
  const vtxid_type next_dist_token = vtxid_type(-2); // todo: change to maxint-1
  vtxid_type head = 0;
  vtxid_type tail = 0;
  vtxid_type dist = 0;
  dists[source] = 0;
  queue[tail++] = source;
  queue[tail++] = next_dist_token;
  while (tail - head > 1) {
    vtxid_type vertex = queue[head++];
    if (vertex == next_dist_token) {
      dist++;
      queue[tail++] = next_dist_token;
      continue;
    }
    vtxid_type degree = graph.adjlists[vertex].get_out_degree();
    vtxid_type* neighbors = graph.adjlists[vertex].get_out_neighbors();
    for (vtxid_type edge = 0; edge < degree; edge++) {
      vtxid_type other = neighbors[edge];
      if (dists[other] != unknown)
        continue;
      dists[other] = dist+1;
      if (PUSH_ZERO_ARITY_VERTICES || graph.adjlists[other].get_out_degree() > 0)
        queue[tail++] = other;
    }
  }
  free(queue);
  return dists;
}

/*---------------------------------------------------------------------*/
// BFS, same as above, except that the queue is no longer
// a flat array but instead an abstract FIFO data structure.
template <class Adjlist_seq, class Fifo>
typename adjlist<Adjlist_seq>::vtxid_type*
bfs_by_dynamic_array(const adjlist<Adjlist_seq>& graph,
                     typename adjlist<Adjlist_seq>::vtxid_type source) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  vtxid_type* dists = data::mynew_array<vtxid_type>(nb_vertices);
  fill_array_seq(dists, nb_vertices, unknown);
  LOG_BASIC(ALGO_PHASE);
  Fifo queue;
  const vtxid_type next_dist_token = vtxid_type(-2); // todo: change to maxint-1
  vtxid_type dist = 0;
  dists[source] = 0;
  queue.push_back(source);
  queue.push_back(next_dist_token);
  while (queue.size() > 1) {
    vtxid_type vertex = queue.pop_front();
    if (vertex == next_dist_token) {
      dist++;
      queue.push_back(next_dist_token);
      continue;
    }
    vtxid_type degree = graph.adjlists[vertex].get_out_degree();
    vtxid_type* neighbors = graph.adjlists[vertex].get_out_neighbors();
    for (vtxid_type edge = 0; edge < degree; edge++) {
      vtxid_type other = neighbors[edge];
      if (dists[other] != unknown)
        continue;
      dists[other] = dist+1;
      if (PUSH_ZERO_ARITY_VERTICES || graph.adjlists[other].get_out_degree() > 0)
        queue.push_back(other);
    }
  }
  return dists;
}

/*---------------------------------------------------------------------*/
// BFS implemented using two stacks (one for vertices at distance "dist",
// and one for vertices at distance "dist+1"), both stacks being represented
// as flat arrays of vertex ids. The stacks are alternatively treated as previous
// frontier or as next frontier, according to the parity of "dist".
template <class Adjlist_seq>
typename adjlist<Adjlist_seq>::vtxid_type*
bfs_by_dual_arrays(const adjlist<Adjlist_seq>& graph,
                   typename adjlist<Adjlist_seq>::vtxid_type source) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  vtxid_type* dists = data::mynew_array<vtxid_type>(nb_vertices);
  fill_array_seq(dists, nb_vertices, unknown);
  LOG_BASIC(ALGO_PHASE);
  vtxid_type* stacks[2];
  stacks[0] = data::mynew_array<vtxid_type>(graph.get_nb_vertices());
  stacks[1] = data::mynew_array<vtxid_type>(graph.get_nb_vertices());
  vtxid_type nbs[2]; // size of the stacks
  nbs[0] = 0;
  nbs[1] = 0;
  vtxid_type cur = 0; // either 0 or 1, depending on parity of dist
  vtxid_type nxt = 1; // either 1 or 0, depending on parity of dist
  vtxid_type dist = 0;
  dists[source] = 0;
  stacks[cur][nbs[cur]++] = source;
  while (nbs[cur] > 0) {
    vtxid_type nb = nbs[cur];
    for (vtxid_type ix = 0; ix < nb; ix++) {
      vtxid_type vertex = stacks[cur][ix];
      vtxid_type degree = graph.adjlists[vertex].get_out_degree();
      vtxid_type* neighbors = graph.adjlists[vertex].get_out_neighbors();
      for (vtxid_type edge = 0; edge < degree; edge++) {
        vtxid_type other = neighbors[edge];
        if (dists[other] != unknown)
          continue;
        dists[other] = dist+1;
        if (PUSH_ZERO_ARITY_VERTICES || graph.adjlists[other].get_out_degree() > 0)
          stacks[nxt][nbs[nxt]++] = other;
      }
    }
    nbs[cur] = 0;
    cur = 1 - cur;
    nxt = 1 - nxt;
    dist++;
  }
  free(stacks[0]);
  free(stacks[1]);
  return dists;
}

/*---------------------------------------------------------------------*/
// BFS implemented using, as above, two stacks, but now the stacks
// are treated as abstract LIFO data structures, and a foreach loop
// is used to iterate over these stacks.
template <class Adjlist_seq, class Frontier>
typename adjlist<Adjlist_seq>::vtxid_type*
bfs_by_dual_frontiers_and_foreach(const adjlist<Adjlist_seq>& graph,
                                  typename adjlist<Adjlist_seq>::vtxid_type source) {
//FORMIKE: use a macros "GRAPH_STAT()", like we do for STATS and LOGGING
#ifdef GRAPH_SEARCH_STATS
  peak_frontier_size = 0l;
#endif
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  vtxid_type* dists = data::mynew_array<vtxid_type>(nb_vertices);
  fill_array_seq(dists, nb_vertices, unknown);
  LOG_BASIC(ALGO_PHASE);
  Frontier prev;
  Frontier next;
  prev.push_back(source);
  dists[source] = 0;
  vtxid_type dist = 0;
  while (! prev.empty()) {
    prev.for_each([&graph,dists,&prev,&next,dist,unknown] (vtxid_type vertex) {
      vtxid_type degree = graph.adjlists[vertex].get_out_degree();
      vtxid_type* neighbors = graph.adjlists[vertex].get_out_neighbors();
      for (vtxid_type edge = 0; edge < degree; edge++) {
        vtxid_type other = neighbors[edge];
        if (dists[other] != unknown)
          continue;
        dists[other] = dist+1;
        if (PUSH_ZERO_ARITY_VERTICES || graph.adjlists[other].get_out_degree() > 0)
          next.push_back(other);
      }
    } );
    prev.clear();
    next.swap(prev);
    dist++;
#ifdef GRAPH_SEARCH_STATS
    peak_frontier_size = std::max((size_t)frontier.size(), peak_frontier_size);
#endif
  }
  return dists;
}

/*---------------------------------------------------------------------*/
// BFS implemented using, as above, two abstract stacks, but now the
// iteration is performed not using a foreach loop but instead by popping
// the items one by one until the sequence becomes empty
template <class Adjlist_seq, class Frontier>
typename adjlist<Adjlist_seq>::vtxid_type*
bfs_by_dual_frontiers_and_pushpop(const adjlist<Adjlist_seq>& graph,
                                  typename adjlist<Adjlist_seq>::vtxid_type source) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  vtxid_type* dists = data::mynew_array<vtxid_type>(nb_vertices);
  fill_array_seq(dists, nb_vertices, unknown);
  Frontier prev;
  Frontier next;
  prev.push_back(source);
  dists[source] = 0;
  vtxid_type dist = 0;
  while (! prev.empty()) {
    while (! prev.empty()) {
      vtxid_type vertex = prev.pop_back(); // use pop_front for same order as bfs with deque
      vtxid_type degree = graph.adjlists[vertex].get_out_degree();
      vtxid_type* neighbors = graph.adjlists[vertex].get_out_neighbors();
      for (vtxid_type edge = 0; edge < degree; edge++) {
        vtxid_type other = neighbors[edge];
        if (dists[other] != unknown)
          continue;
        dists[other] = dist+1;
        if (PUSH_ZERO_ARITY_VERTICES || graph.adjlists[other].get_out_degree() > 0)
          next.push_back(other);
      }
    }
    next.swap(prev);
    dist++;
  }
  return dists;
}

/*---------------------------------------------------------------------*/
// BFS implemented using two frontiers; the fontiers are abstract bag
// data structures that support:
// - pushing into the frontier the set of all outgoing edges
// - iterating over all the edges stored in the frontier

template <class Adjlist, class Frontier>
typename Adjlist::vtxid_type*
bfs_by_frontier_segment(const Adjlist& graph,
                        typename Adjlist::vtxid_type source) {
  using vtxid_type = typename Adjlist::vtxid_type;
  using edgelist_type = typename Frontier::edgelist_type;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  vtxid_type* dists = data::mynew_array<vtxid_type>(nb_vertices);
  fill_array_seq(dists, nb_vertices, unknown);
  LOG_BASIC(ALGO_PHASE);
  auto graph_alias = get_alias_of_adjlist(graph);
  Frontier frontiers[2];
  frontiers[0].set_graph(graph_alias);
  frontiers[1].set_graph(graph_alias);
  vtxid_type cur = 0; // either 0 or 1, depending on parity of dist
  vtxid_type nxt = 1; // either 1 or 0, depending on parity of dist
  dists[source] = 0;
  vtxid_type dist = 0;
  frontiers[0].push_vertex_back(source);
  while (! frontiers[cur].empty()) {
    frontiers[cur].for_each_outedge([dist,dists,&frontiers,nxt,unknown,&graph] (vtxid_type other) {
      if (dists[other] == unknown) {
        dists[other] = dist+1;
        if (PUSH_ZERO_ARITY_VERTICES || graph.adjlists[other].get_out_degree() > 0)
          frontiers[nxt].push_vertex_back(other);
      }
    });
    frontiers[cur].clear();
    cur = 1 - cur;
    nxt = 1 - nxt;
    dist++;
  }
  return dists;
}

template <class Adjlist, class Frontier>
typename Adjlist::vtxid_type*
bfs_by_frontier_segment_with_swap(const Adjlist& graph,
                        typename Adjlist::vtxid_type source) {
  using vtxid_type = typename Adjlist::vtxid_type;
  using edgelist_type = typename Frontier::edgelist_type;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  vtxid_type* dists = data::mynew_array<vtxid_type>(nb_vertices);
  fill_array_seq(dists, nb_vertices, unknown);
  auto graph_alias = get_alias_of_adjlist(graph);
  Frontier prev(graph_alias);
  Frontier next(graph_alias);
  prev.push_vertex_back(source);
  dists[source] = 0;
  vtxid_type dist = 0;
  while (! prev.empty()) {
    prev.for_each_outedge([dist,dists,&prev,&next,unknown,graph] (vtxid_type other) {
      if (dists[other] == unknown) {
        dists[other] = dist+1;
        if (PUSH_ZERO_ARITY_VERTICES || graph.adjlists[other].get_out_degree() > 0)
          next.push_vertex_back(other);
      }
    });
    prev.clear();
    next.swap(prev);
    dist++;
  }
  return dists;
}

/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/* Breadth-first search of a graph in adjacency-list format; parallel */

// Parallel BFS using Shardl and Leiserson's algorithm:
// Process each frontier by doing a parallel_for on the vertex ids 
// from the frontier, and handling the edges of each vertex using
// a parallel_for.

extern int ls_pbfs_cutoff;
extern int ls_pbfs_loop_cutoff;

template <bool idempotent = false>
class ls_pbfs {
public:
  
  using self_type = ls_pbfs<idempotent>;
  using idempotent_ls_pbfs = ls_pbfs<true>;
  
  //! \todo: move this to a common namespace
  template <class Index, class Item>
  static bool try_to_set_dist(Index target,
                              Item unknown, Item dist,
                              std::atomic<Item>* dists) {
    if (dists[target].load(std::memory_order_relaxed) != unknown)
      return false;
    if (idempotent)
      dists[target].store(dist, std::memory_order_relaxed);
    else if (! dists[target].compare_exchange_strong(unknown, dist))
      return false;
    return true;
  }
  
  template <class Adjlist_seq, class Frontier>
  static void process_layer(const adjlist<Adjlist_seq>& graph,
                            std::atomic<typename adjlist<Adjlist_seq>::vtxid_type>* dists,
                            typename adjlist<Adjlist_seq>::vtxid_type dist_of_next,
                            typename adjlist<Adjlist_seq>::vtxid_type source,
                            Frontier& prev,
                            Frontier& next) {
    using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
    using size_type = typename Frontier::size_type;
    vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
    auto cutoff = [] (Frontier& f) {
      return f.size() <= vtxid_type(ls_pbfs_cutoff);
    };
    auto split = [] (Frontier& src, Frontier& dst) {
      src.split_approximate(dst);
    };
    auto append = [] (Frontier& src, Frontier& dst) {
      src.concat(dst);
    };
    sched::native::forkjoin(prev, next, cutoff, split, append, [&] (Frontier& prev, Frontier& next) {
     prev.for_each([&] (vtxid_type vertex) {
        vtxid_type degree = graph.adjlists[vertex].get_out_degree();
        vtxid_type* neighbors = graph.adjlists[vertex].get_out_neighbors();
        data::pcontainer::combine(vtxid_type(0), degree, next, [&] (vtxid_type edge, Frontier& next) {
          vtxid_type other = neighbors[edge];
          if (try_to_set_dist(other, unknown, dist_of_next, dists)) {
            if (PUSH_ZERO_ARITY_VERTICES || graph.adjlists[other].get_out_degree() > 0)
              next.push_back(other);
          }
        }, ls_pbfs_loop_cutoff);
      });
     prev.clear();
    });
  }
  
  template <class Adjlist_seq, class Frontier>
  static std::atomic<typename adjlist<Adjlist_seq>::vtxid_type>*
  main(const adjlist<Adjlist_seq>& graph,
       typename adjlist<Adjlist_seq>::vtxid_type source) {
#ifdef GRAPH_SEARCH_STATS
    peak_frontier_size = 0l;
#endif
    using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
    using size_type = typename Frontier::size_type;
    vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
    vtxid_type nb_vertices = graph.get_nb_vertices();
    std::atomic<vtxid_type>* dists = data::mynew_array<std::atomic<vtxid_type>>(nb_vertices);
    fill_array_par(dists, nb_vertices, unknown);
    LOG_BASIC(ALGO_PHASE);
    Frontier prev;
    Frontier next;
    vtxid_type dist = 0;
    prev.push_back(source);
    dists[source].store(dist);
    while (! prev.empty()) {
      dist++;
      if (prev.size() <= ls_pbfs_cutoff)
        idempotent_ls_pbfs::process_layer(graph, dists, dist, source, prev, next);
      else
        self_type::process_layer(graph, dists, dist, source, prev, next);
     prev.swap(next);
#ifdef GRAPH_SEARCH_STATS
    peak_frontier_size = std::max((size_t)prev.size(), peak_frontier_size);
#endif
    }
    return dists;
  }
  
};

/*---------------------------------------------------------------------*/

// Parallel BFS using our frontier-segment-based algorithm:
// Process each frontier by doing a parallel_for on the set of 
// outgoing edges, which is represented using our "frontier" data
// structures that supports splitting according to the nb of edges.

extern int our_bfs_cutoff;

template <bool idempotent = false>
class our_bfs {
public:
  
  using self_type = our_bfs<idempotent>;
  using idempotent_our_bfs = our_bfs<true>;
  
  template <class Adjlist_alias, class Frontier>
  static void process_layer(Adjlist_alias graph_alias,
                            std::atomic<typename Adjlist_alias::vtxid_type>* dists,
                            typename Adjlist_alias::vtxid_type& dist_of_next,
                            typename Adjlist_alias::vtxid_type source,
                            Frontier& prev,
                            Frontier& next) {
    using vtxid_type = typename Adjlist_alias::vtxid_type;
    using size_type = typename Frontier::size_type;
    using edgelist_type = typename Frontier::edgelist_type;
    vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
    auto cutoff = [] (Frontier& f) {
      return f.nb_outedges() <= vtxid_type(our_bfs_cutoff);
    };
    auto split = [] (Frontier& src, Frontier& dst) {
      assert(src.nb_outedges() > 1);
      src.split(src.nb_outedges() / 2, dst);
    };
    auto append = [] (Frontier& src, Frontier& dst) {
      src.concat(dst);
    };
    auto set_env = [graph_alias] (Frontier& f) {
      f.set_graph(graph_alias);
    };
    sched::native::forkjoin(prev, next, cutoff, split, append, set_env, set_env,
                          [&] (Frontier& prev, Frontier& next) {
      prev.for_each_outedge([&] (vtxid_type other) {
        if (ls_pbfs<idempotent>::try_to_set_dist(other, unknown, dist_of_next, dists))
          next.push_vertex_back(other);
          // warning: always does DONT_PUSH_ZERO_ARITY_VERTICES
      });
      prev.clear();
    });
  }
  /*
  template <class Adjlist_alias, class Frontier>
  static void process_layer_sequentially(Adjlist_alias graph_alias,
                            std::atomic<typename Adjlist_alias::vtxid_type>* dists,
                            typename Adjlist_alias::vtxid_type& dist_of_next,
                            typename Adjlist_alias::vtxid_type source,
                            Frontier& prev,
                            Frontier& next) {
    using vtxid_type = typename Adjlist_alias::vtxid_type;
    using size_type = typename Frontier::size_type;
    using edgelist_type = typename Frontier::edgelist_type;
    vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
    prev.for_each_outedge_when_front_and_back_empty([&] (vtxid_type& other) {
      if (ls_pbfs<idempotent>::try_to_set_dist(other, unknown, dist_of_next, dists))
        next.push_vertex_back(other);
    });
    prev.clear_when_front_and_back_empty();
  }
  */

  template <class Adjlist, class Frontier>
  static std::atomic<typename Adjlist::vtxid_type>*
  main(const Adjlist& graph,
       typename Adjlist::vtxid_type source) {
    using vtxid_type = typename Adjlist::vtxid_type;
    using size_type = typename Frontier::size_type;
    vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
    vtxid_type nb_vertices = graph.get_nb_vertices();
    std::atomic<vtxid_type>* dists = data::mynew_array<std::atomic<vtxid_type>>(nb_vertices);
    fill_array_par(dists, nb_vertices, unknown);
    LOG_BASIC(ALGO_PHASE);
    auto graph_alias = get_alias_of_adjlist(graph);
    vtxid_type dist = 0;
    dists[source].store(dist);
    Frontier frontiers[2];
    frontiers[0].set_graph(graph_alias);
    frontiers[1].set_graph(graph_alias);
    vtxid_type cur = 0; // either 0 or 1, depending on parity of dist
    vtxid_type nxt = 1; // either 1 or 0, depending on parity of dist
    frontiers[0].push_vertex_back(source);
    while (! frontiers[cur].empty()) {
      dist++;
      if (frontiers[cur].nb_outedges() <= our_bfs_cutoff) {
        // idempotent_our_bfs::process_layer(graph_alias, dists, dist, source, prev, next);
        // idempotent_our_bfs::process_layer_sequentially(graph_alias, dists, dist, source, prev, next);
        frontiers[cur].for_each_outedge_when_front_and_back_empty([&] (vtxid_type other) {
          if (ls_pbfs<true>::try_to_set_dist(other, unknown, dist, dists))
            frontiers[nxt].push_vertex_back(other);
        });
        frontiers[cur].clear_when_front_and_back_empty();
      } else {
        self_type::process_layer(graph_alias, dists, dist, source, frontiers[cur], frontiers[nxt]);
      }
      cur = 1 - cur;
      nxt = 1 - nxt;
    }
    return dists;
  }
  
  template <class Adjlist, class Frontier>
  static std::atomic<typename Adjlist::vtxid_type>*
  main_with_swap(const Adjlist& graph,
    typename Adjlist::vtxid_type source) {
    using vtxid_type = typename Adjlist::vtxid_type;
    using size_type = typename Frontier::size_type;
    vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
    vtxid_type nb_vertices = graph.get_nb_vertices();
    std::atomic<vtxid_type>* dists = data::mynew_array<std::atomic<vtxid_type>>(nb_vertices);
    fill_array_par(dists, nb_vertices, unknown);
    LOG_BASIC(ALGO_PHASE);
    auto graph_alias = get_alias_of_adjlist(graph);
    vtxid_type dist = 0;
    dists[source].store(dist);
    Frontier prev(graph_alias);
    Frontier next(graph_alias);
    prev.push_vertex_back(source);
    while (! prev.empty()) {
      dist++;
      if (prev.nb_outedges() <= our_bfs_cutoff) {
        // idempotent_our_bfs::process_layer(graph_alias, dists, dist, source, prev, next);
        // idempotent_our_bfs::process_layer_sequentially(graph_alias, dists, dist, source, prev, next);
        prev.for_each_outedge_when_front_and_back_empty([&] (vtxid_type other) {
          if (ls_pbfs<true>::try_to_set_dist(other, unknown, dist, dists))
            next.push_vertex_back(other);
        });
        prev.clear_when_front_and_back_empty();
      } else {
        self_type::process_layer(graph_alias, dists, dist, source, prev, next);
      }
      prev.swap(next);
    }
    return dists;
  }
};

/*---------------------------------------------------------------------*/

// Parallel BFS using our frontier-segment-based algorithm:
// Process each frontier by doing a parallel_for on the set of 
// outgoing edges, which is represented using our "frontier" data
// structures that supports splitting according to the nb of edges.

extern int our_lazy_bfs_cutoff;

// todo: take as argument
const int communicate_cutoff = 256;

template <bool idempotent = false>
class our_lazy_bfs {
public:
  
  static bool should_call_communicate() {
#ifndef USE_CILK_RUNTIME
    return sched::threaddag::my_sched()->should_call_communicate();
#else
    return sched::native::my_deque_size() == 0;
#endif
  }

  static void reject() {
#ifndef USE_CILK_RUNTIME
    sched::threaddag::my_sched()->reject();
#else
#endif
  }

  static void unblock() {
#ifndef USE_CILK_RUNTIME
    sched::threaddag::my_sched()->unblock();
#else
#endif
  }

  using self_type = our_lazy_bfs<idempotent>;
  using idempotent_our_lazy_bfs = our_lazy_bfs<true>;
  
  template <class Adjlist_alias, class Frontier>
  static void process_layer(Adjlist_alias graph_alias,
                            std::atomic<typename Adjlist_alias::vtxid_type>* dists,
                            typename Adjlist_alias::vtxid_type& dist_of_next,
                            typename Adjlist_alias::vtxid_type source,
                            Frontier& prev,
                            Frontier& next) {
    using vtxid_type = typename Adjlist_alias::vtxid_type;
    using size_type = typename Frontier::size_type;
    using edgelist_type = typename Frontier::edgelist_type;
    vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
    size_t nb_outedges = prev.nb_outedges();
    bool blocked = false;
    while (nb_outedges > 0) {
      if (nb_outedges <= our_lazy_bfs_cutoff) {
        blocked = true;
        reject();
      }
      if (should_call_communicate()) {
        if (nb_outedges > our_lazy_bfs_cutoff) {
          Frontier prev2(graph_alias);
          Frontier next2(graph_alias);
          prev.split(prev.nb_outedges() / 2, prev2);
          sched::native::fork2([&] { process_layer(graph_alias, dists, dist_of_next, source, prev, next); },
                               [&] { process_layer(graph_alias, dists, dist_of_next, source, prev2, next2); });
          next.concat(next2);
          if (blocked) // should always be false due to the order of the conditionals; yet, keep it for safety
            unblock(); 
          return;
        }
        else
        {
          blocked = true;
          reject();
        }
      }
      prev.for_at_most_nb_outedges(communicate_cutoff, [&](vtxid_type other) {
        if (ls_pbfs<idempotent>::try_to_set_dist(other, unknown, dist_of_next, dists))
          next.push_vertex_back(other);
        // warning: always does DONT_PUSH_ZERO_ARITY_VERTICES
      });
      nb_outedges = prev.nb_outedges();
    }
    if (blocked)
      unblock();
  }

  template <class Adjlist, class Frontier>
  static std::atomic<typename Adjlist::vtxid_type>*
  main(const Adjlist& graph,
       typename Adjlist::vtxid_type source) {
    using vtxid_type = typename Adjlist::vtxid_type;
    using size_type = typename Frontier::size_type;
    vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
    vtxid_type nb_vertices = graph.get_nb_vertices();
    std::atomic<vtxid_type>* dists = data::mynew_array<std::atomic<vtxid_type>>(nb_vertices);
    fill_array_par(dists, nb_vertices, unknown);
    LOG_BASIC(ALGO_PHASE);
    auto graph_alias = get_alias_of_adjlist(graph);
    vtxid_type dist = 0;
    dists[source].store(dist);
    Frontier frontiers[2];
    frontiers[0].set_graph(graph_alias);
    frontiers[1].set_graph(graph_alias);
    vtxid_type cur = 0; // either 0 or 1, depending on parity of dist
    vtxid_type nxt = 1; // either 1 or 0, depending on parity of dist
    frontiers[0].push_vertex_back(source);
    while (! frontiers[cur].empty()) {
      dist++;
      if (frontiers[cur].nb_outedges() <= our_lazy_bfs_cutoff) {
        // idempotent_our_bfs::process_layer(graph_alias, dists, dist, source, prev, next);
        // idempotent_our_bfs::process_layer_sequentially(graph_alias, dists, dist, source, prev, next);
        frontiers[cur].for_each_outedge_when_front_and_back_empty([&] (vtxid_type other) {
          if (ls_pbfs<true>::try_to_set_dist(other, unknown, dist, dists))
            frontiers[nxt].push_vertex_back(other);
        });
        frontiers[cur].clear_when_front_and_back_empty();
      } else {
        self_type::process_layer(graph_alias, dists, dist, source, frontiers[cur], frontiers[nxt]);
      }
      cur = 1 - cur;
      nxt = 1 - nxt;
    }
    return dists;
  }
  
  template <class Adjlist, class Frontier>
  static std::atomic<typename Adjlist::vtxid_type>*
  main_with_swap(const Adjlist& graph,
    typename Adjlist::vtxid_type source) {
    using vtxid_type = typename Adjlist::vtxid_type;
    using size_type = typename Frontier::size_type;
    vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
    vtxid_type nb_vertices = graph.get_nb_vertices();
    std::atomic<vtxid_type>* dists = data::mynew_array<std::atomic<vtxid_type>>(nb_vertices);
    fill_array_par(dists, nb_vertices, unknown);
    LOG_BASIC(ALGO_PHASE);
    auto graph_alias = get_alias_of_adjlist(graph);
    vtxid_type dist = 0;
    dists[source].store(dist);
    Frontier prev(graph_alias);
    Frontier next(graph_alias);
    prev.push_vertex_back(source);
    while (! prev.empty()) {
      dist++;
      if (prev.nb_outedges() <= our_lazy_bfs_cutoff) {
        // idempotent_our_bfs::process_layer(graph_alias, dists, dist, source, prev, next);
        // idempotent_our_bfs::process_layer_sequentially(graph_alias, dists, dist, source, prev, next);
        prev.for_each_outedge_when_front_and_back_empty([&] (vtxid_type other) {
          if (ls_pbfs<true>::try_to_set_dist(other, unknown, dist, dists))
            next.push_vertex_back(other);
        });
        prev.clear_when_front_and_back_empty();
      } else {
        self_type::process_layer(graph_alias, dists, dist, source, prev, next);
      }
      prev.swap(next);
    }
    return dists;
  }
};

  
/*---------------------------------------------------------------------*/
// BFS algorithm taken from Problem Based Benchmark Suite
// Process each frontier by doing a parallel_for on the set of outgoing
// edges. The frontier data structure is a flat array of vertex ids.
// Each round the next frontier is constructed by using parallel
// prefix scan to allocate space for each newly discovered list
// of outedges.

template <bool idempotent, class Adjlist>
std::atomic<typename Adjlist::vtxid_type>*
pbbs_pbfs(const Adjlist& graph, typename Adjlist::vtxid_type source) {
  using vtxid_type = typename Adjlist::vtxid_type;
  struct nonNegF{bool operator() (vtxid_type a) {return (a>=0);}};
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  edgeid_type nb_edges = graph.nb_edges;
  std::atomic<vtxid_type>* dists = data::mynew_array<std::atomic<vtxid_type>>(nb_vertices);
  fill_array_par(dists, nb_vertices, unknown);
  LOG_BASIC(ALGO_PHASE);
  vtxid_type* Frontier = data::mynew_array<vtxid_type>(nb_edges);
  vtxid_type* FrontierNext = data::mynew_array<vtxid_type>(nb_edges);
  vtxid_type* Counts = data::mynew_array<vtxid_type>(nb_vertices);
  vtxid_type dist = 0;
  Frontier[0] = source;
  vtxid_type frontierSize = 1;
  dists[source].store(dist);
  while (frontierSize > 0) {
    dist++;
    sched::native::parallel_for(vtxid_type(0), frontierSize, [&] (vtxid_type i) {
      Counts[i] = graph.adjlists[Frontier[i]].get_out_degree();
    });
    vtxid_type nr = pbbs::sequence::scan(Counts,Counts,frontierSize,pbbs::utils::addF<vtxid_type>(),vtxid_type(0));
    sched::native::parallel_for(vtxid_type(0), frontierSize, [&] (vtxid_type i) {
      vtxid_type v = Frontier[i];
      vtxid_type o = Counts[i];
      vtxid_type degree = graph.adjlists[v].get_out_degree();
      vtxid_type* neighbors = graph.adjlists[v].get_out_neighbors();
      sched::native::parallel_for(vtxid_type(0), degree, [&] (vtxid_type j) {
        vtxid_type other = neighbors[j];
        if (ls_pbfs<idempotent>::try_to_set_dist(other, unknown, dist, dists)) {
          if (PUSH_ZERO_ARITY_VERTICES || graph.adjlists[other].get_out_degree() > 0)
            FrontierNext[o+j] = other;
          else
            FrontierNext[o+j] = vtxid_type(-1);
        } else {
          FrontierNext[o+j] = vtxid_type(-1);
        }
      });
    });
    frontierSize = pbbs::sequence::filter(FrontierNext,Frontier,nr,nonNegF());
  }
  free(FrontierNext); free(Frontier); free(Counts);
  return dists;
}

} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_GRAPH_BFS_H_ */
