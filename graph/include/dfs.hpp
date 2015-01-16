/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file dfs.hpp
 *
 */

#include "edgelist.hpp"
#include "adjlist.hpp"
#include "native.hpp"
#include "cldeque.hpp"
#include "barrier.hpp"

#ifndef _PASL_GRAPH_DFS_H_
#define _PASL_GRAPH_DFS_H_



/***********************************************************************/

namespace pasl {
namespace graph {

#ifdef GRAPH_SEARCH_STATS
size_t peak_frontier_size;
#endif

/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/* Depth first search in a graph in adjacency-list format; serial */

/*---------------------------------------------------------------------*/
// Represent the bag of nodes to visit using a flat array.

template <
  class Adjlist_seq,
  bool report_nb_edges_processed = false,
  bool report_nb_vertices_visited = false
>
int* dfs_by_vertexid_array(const adjlist<Adjlist_seq>& graph,
                           typename adjlist<Adjlist_seq>::vtxid_type source,
                           long* nb_edges_processed = nullptr,
                           long* nb_vertices_visited = nullptr,
                           int* visited_from_caller = nullptr) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  if (report_nb_edges_processed)
    *nb_edges_processed = 0;
  if (report_nb_vertices_visited)
    *nb_vertices_visited = 1;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  int* visited;
  if (visited_from_caller != nullptr) {
    visited = visited_from_caller;
    // don't need to initialize visited
  } else {
    visited = data::mynew_array<int>(nb_vertices);
    fill_array_seq(visited, nb_vertices, 0);
  }
  LOG_BASIC(ALGO_PHASE);
  vtxid_type* frontier = data::mynew_array<vtxid_type>(nb_vertices);
  vtxid_type frontier_size = 0;
  frontier[frontier_size++] = source;
  visited[source] = 1;
  while (frontier_size > 0) {
    vtxid_type vertex = frontier[--frontier_size];
    vtxid_type degree = graph.adjlists[vertex].get_out_degree();
    vtxid_type* neighbors = graph.adjlists[vertex].get_out_neighbors();
    if (report_nb_edges_processed)
      (*nb_edges_processed) += degree;
    for (vtxid_type edge = 0; edge < degree; edge++) {
      vtxid_type other = neighbors[edge];
      if (visited[other])
        continue;
      if (report_nb_vertices_visited)
        (*nb_vertices_visited)++;
      visited[other] = 1;
      frontier[frontier_size++] = other;
    }
  }
  data::myfree(frontier);
  return visited;
}

/*---------------------------------------------------------------------*/
// Represent the bag of nodes to visit using an abstract bag data
// structures that supports push and pop.

template <class Adjlist_seq, class Frontier>
int* dfs_by_vertexid_frontier(const adjlist<Adjlist_seq>& graph,
                              typename adjlist<Adjlist_seq>::vtxid_type source) {
#ifdef GRAPH_SEARCH_STATS
  peak_frontier_size = 0l;
#endif
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  int* visited = data::mynew_array<int>(nb_vertices);
  fill_array_seq(visited, nb_vertices, 0);
  LOG_BASIC(ALGO_PHASE);
  Frontier frontier;
  frontier.push_back(source);
  visited[source] = 1;
  while (! frontier.empty()) {
    vtxid_type vertex = frontier.pop_back();
    vtxid_type degree = graph.adjlists[vertex].get_out_degree();
    vtxid_type* neighbors = graph.adjlists[vertex].get_out_neighbors();
    for (vtxid_type edge = 0; edge < degree; edge++) {
      vtxid_type other = neighbors[edge];
      if (visited[other])
        continue;
      visited[other] = 1;
      frontier.push_back(other);
    }
#ifdef GRAPH_SEARCH_STATS
    peak_frontier_size = std::max((size_t)frontier.size(), peak_frontier_size);
#endif
  }
  return visited;
}

/*---------------------------------------------------------------------*/
// Represent the set outgoing edges from the nodes to visit using 
// the "frontier" data structure, which supports:
// - pushing all the outedges from a given node
// - popping the next range of edges to process

// custom frontier based on fast finger trees
template <class Adjlist, class Frontier>
int* dfs_by_frontier_segment(const Adjlist& graph,
                             typename Adjlist::vtxid_type source) {
  using vtxid_type = typename Adjlist::vtxid_type;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  int* visited = data::mynew_array<int>(nb_vertices);
  fill_array_seq(visited, nb_vertices, 0);
  LOG_BASIC(ALGO_PHASE);
  Frontier frontier(get_alias_of_adjlist(graph));
  frontier.push_vertex_back(source);
  visited[source] = 1;
  while (! frontier.empty()) {
    auto neighbors = frontier.pop_edgelist_back();
    for (auto i = neighbors.lo; i < neighbors.hi; i++) {
      vtxid_type other = *i;
      if (visited[other])
        continue;
      visited[other] = 1;
      frontier.push_vertex_back(other);
    }
  }
  return visited;
}

/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
// PARALLEL

/*---------------------------------------------------------------------*/
/* Parallel (pseudo) depth-first search of a graph in adacency-list format */

template <class Index, class Item>
bool try_to_mark_non_idempotent(std::atomic<Item>* visited, Index target) {
  Item orig = 0;
  if (! visited[target].compare_exchange_strong(orig, 1))
    return false;
  return true;
}
  
template <class Adjlist, class Item, bool idempotent>
bool try_to_mark(const Adjlist& graph,
                 std::atomic<Item>* visited,
                 typename Adjlist::vtxid_type target) {
  using vtxid_type = typename Adjlist::vtxid_type;
  const vtxid_type max_outdegree_for_idempotent = 30;
  if (visited[target].load(std::memory_order_relaxed))
    return false;
  if (idempotent) {
    if (graph.adjlists[target].get_out_degree() <= max_outdegree_for_idempotent) {
      visited[target].store(1, std::memory_order_relaxed);
      return true;
    } else {
      return try_to_mark_non_idempotent<vtxid_type,Item>(visited, target);
    }
  } else {
    return try_to_mark_non_idempotent<vtxid_type,Item>(visited, target);
  }
}

extern int our_pseudodfs_cutoff;
  
#ifndef DISABLE_NEW_PSEUDODFS
#define PARALLEL_WHILE sched::native::parallel_while_cas_ri
#else
#define PARALLEL_WHILE sched::native::parallel_while
#endif

template <class Adjlist, class Frontier, bool idempotent = false>
std::atomic<int>* our_pseudodfs(const Adjlist& graph, typename Adjlist::vtxid_type source) {
  using vtxid_type = typename Adjlist::vtxid_type;
  using edgelist_type = typename Frontier::edgelist_type;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  std::atomic<int>* visited = data::mynew_array<std::atomic<int>>(nb_vertices);
  fill_array_par(visited, nb_vertices, 0);
  LOG_BASIC(ALGO_PHASE);
  auto graph_alias = get_alias_of_adjlist(graph);
  Frontier frontier(graph_alias);
  frontier.push_vertex_back(source);
  visited[source].store(1, std::memory_order_relaxed);
  auto size = [] (Frontier& frontier) {
    return frontier.nb_outedges();
  };
  auto fork = [&] (Frontier& src, Frontier& dst) {
    vtxid_type m = vtxid_type((src.nb_outedges() + 1) / 2);
    src.split(m, dst);
  };
  auto set_in_env = [graph_alias] (Frontier& f) {
    f.set_graph(graph_alias);
  };
  if (frontier.nb_outedges() == 0)
    return visited;
  PARALLEL_WHILE(frontier, size, fork, set_in_env, [&] (Frontier& frontier) {
    frontier.for_at_most_nb_outedges(our_pseudodfs_cutoff, [&](vtxid_type other_vertex) {
      if (try_to_mark<Adjlist, int, idempotent>(graph, visited, other_vertex))
        frontier.push_vertex_back(other_vertex);
    });
  });
  return visited;
}

/*---------------------------------------------------------------------*/
/* Cong et al's adaptive parallel pseudo DFS */

extern int cong_pdfs_cutoff;
static constexpr int max_deque_sz = 60;

  /*
template <class Adjlist_alias, class Frontier, bool idempotent = false>
void cong_pseudodfs_rec(Frontier* incoming_frontier,
                        Adjlist_alias graph,
                        std::atomic<int>* visited,
                        sched::native::multishot* join) {
  using vtxid_type = typename Adjlist_alias::vtxid_type;
  // transfer contents of incoming frontier to private frontier
  Frontier cur;
  Frontier next;
  incoming_frontier->swap(cur);
  delete incoming_frontier;
  // process private frontier
  while (! cur.empty()) {
    while (! cur.empty()) {
      vtxid_type v = cur.pop_back();
      vtxid_type degree = graph.adjlists[v].get_out_degree();
      vtxid_type* neighbors = graph.adjlists[v].get_out_neighbors();
      for (vtxid_type edge = 0; edge < degree; edge++) {
        vtxid_type other = neighbors[edge];
        if (try_to_mark<vtxid_type, int, idempotent>(visited, other)) {
          next.push_back(other);
          int deque_sz = sched::native::my_deque_size();
          auto frontier_sz = next.size();
          bool should_spawn =
                  (frontier_sz >= cong_pdfs_cutoff)
              ||  ( (deque_sz < max_deque_sz) && (frontier_sz >= (1<<deque_sz)));
          if (should_spawn) {
            Frontier* cur2 = new Frontier();
            cur2->swap(next);
            sched::native::async([=] {
              cong_pseudodfs_rec(cur2, graph, visited, join);
            }, join);
          }
        }
      }
    }
    cur.swap(next);
  }
}

template <class Adjlist_seq, class Frontier, bool idempotent = false>
std::atomic<int>* cong_pseudodfs(const adjlist<Adjlist_seq>& graph,
                                 typename adjlist<Adjlist_seq>::vtxid_type source) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  auto graph_alias = get_alias_of_adjlist(graph);
  vtxid_type nb_vertices = graph.get_nb_vertices();
  std::atomic<int>* visited = data::mynew_array<std::atomic<int>>(nb_vertices);
  fill_array_par(visited, nb_vertices, 0);
  LOG_BASIC(ALGO_PHASE);
  Frontier* frontier = new Frontier();
  frontier->push_back(source);
  visited[source].store(1, std::memory_order_relaxed);
  sched::native::finish([&] (sched::native::multishot* join) {
    cong_pseudodfs_rec<typeof(graph_alias), Frontier, idempotent>(frontier, graph_alias, visited, join);
  });
  return visited;
}
*/
  
template <class Adjlist_seq, bool idempotent = false>
std::atomic<int>* cong_pseudodfs(const adjlist<Adjlist_seq>& graph,
                                 typename adjlist<Adjlist_seq>::vtxid_type source) {
  const int cong_pdfs_cutoff = 32;
  const int init_deque_capacity = 1024;
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  using chunk_type = data::fixedcapacity::heap_allocated::stack<vtxid_type, cong_pdfs_cutoff>;
  //  using chunk_type = Frontier;
  using deque_type = data::cldeque<chunk_type>;
  using deque_pop_return_type = typename deque_type::pop_result_type;
  std::atomic<int>* visited = data::mynew_array<std::atomic<int>>(nb_vertices);
  fill_array_par(visited, nb_vertices, 0);
  LOG_BASIC(ALGO_PHASE);
  worker_id_t leader_id = sched::threaddag::get_my_id();
  std::atomic<bool> is_done(false);
  util::barrier::spin ready;
  ready.init(sched::threaddag::get_nb_workers());
   using counter_type = data::perworker::counter::carray<long>;
   //using counter_type = std::atomic<long>;
  counter_type counter;
  data::perworker::array<deque_type> deques;
  counter.init(0);
  counter++;
  deques.for_each([] (worker_id_t, deque_type& deque) {
    deque.init(init_deque_capacity);
  });
  chunk_type* c0 = new chunk_type();
  c0->push_back(source);
  deques.mine().push_back(c0);
  visited[source].store(1, std::memory_order_relaxed);
  sched::native::parallel_while([&] {
    worker_id_t my_id = sched::threaddag::get_my_id();
    sched::scheduler_p sched = sched::threaddag::my_sched();
    sched::native::multishot* thread = sched::native::my_thread();
    STAT_IDLE_ONLY(ticks_t date_enter_wait);
    STAT_IDLE_ONLY(date_enter_wait = util::microtime::now());
    deque_type& my_deque = deques.mine();
    chunk_type* bot = nullptr;
    bool init = false;
    ready.wait([&] { thread->yield(); });
    
    while (true) {
      if (! init && my_id != leader_id) {
        init = true;
        goto acquire;
      }
      if (is_done.load())
        break;
      // try to find work
      while (true) {
        if (is_done.load())
          break;
        if (bot == nullptr) {
          typename deque_type::pop_result_type result = deque_type::Pop_bogus;
          bot = my_deque.pop_back(result);
          if (result == deque_type::Pop_succeeded) {
            if (bot == nullptr) {
              assert(my_deque.empty());
              break;
            }
          } else if (result == deque_type::Pop_failed_with_empty_deque) {
            assert(my_deque.empty());
            break;
          } else if (result == deque_type::Pop_failed_with_cas_abort) {
            util::ticks::microseconds_sleep(20.0);
            continue;
          } else {
            assert(false);
          }
        }
        
      exec:
        
        //        LOG_THREAD(THREAD_EXEC, (thread_p)bot);
        STAT_COUNT(THREAD_EXEC);
        
        chunk_type cur;
        chunk_type next;
        bot->swap(cur);
        delete bot;
        bot = nullptr;
        while (! cur.empty()) {
          while (! cur.empty()) {
            vtxid_type v = cur.pop_back();
            vtxid_type degree = graph.adjlists[v].get_out_degree();
            vtxid_type* neighbors = graph.adjlists[v].get_out_neighbors();
            for (vtxid_type edge = 0; edge < degree; edge++) {
              vtxid_type other = neighbors[edge];
              if (try_to_mark<adjlist<Adjlist_seq>, int, idempotent>(graph, visited, other)) {
                next.push_back(other);
                int deque_sz = (int)my_deque.size();
                auto frontier_sz = next.size();
                bool should_spawn =
                (frontier_sz == cong_pdfs_cutoff)
                ||  ( (deque_sz < max_deque_sz) && (frontier_sz >= (1<<deque_sz)));
                if (should_spawn) {
                  STAT_COUNT(THREAD_CREATE);
                  chunk_type* p = new chunk_type();
                  p->swap(next);
                  my_deque.push_back(p);
                }
              }
            }
          }
          cur.swap(next);
        }
        assert(cur.empty());
        assert(next.empty());
      }
      counter--;
      
    acquire:
      
      LOG_BASIC(ENTER_WAIT);
      STAT_COUNT(ENTER_WAIT);
      // STAT_IDLE_ONLY(date_enter_wait = ticks::now());
      STAT_IDLE_ONLY(date_enter_wait = util::microtime::now());
      
      // try to steal work
      //      const long nb_tries_before_sleep = 4;
//      long i = 0;
      while (true) {
        if (my_id == leader_id &&  counter.sum() == 0) {
          LOG_BASIC(ALGO_PHASE);
          is_done.store(true);
        }
        if (is_done.load())
          break;
        worker_id_t id = sched->random_other();
        typename deque_type::pop_result_type result = deque_type::Pop_bogus;
        if (deques[id].empty())
          goto failed_steal_attempt;
        counter++;
        bot = deques[id].pop_front(result);
        if (result == deque_type::Pop_succeeded) {
          if (bot != nullptr) {
            LOG_BASIC(STEAL_SUCCESS);
            STAT_COUNT(THREAD_SEND);
            break;
          }
          LOG_BASIC(STEAL_FAIL);
        } else if (result == deque_type::Pop_failed_with_empty_deque) {
          LOG_BASIC(STEAL_FAIL);
        } else if (result == deque_type::Pop_failed_with_cas_abort) {
          LOG_BASIC(STEAL_ABORT);
        } else {
          assert(false);
        }
        counter--;
      failed_steal_attempt:
        //        i = (i + 1) % nb_tries_before_sleep;
        if (! is_done.load() /* && i == (nb_tries_before_sleep-1) */) {
          thread->yield();
          util::ticks::microseconds_sleep(10.0);
        }
      }
      
      STAT_IDLE(add_to_idle_time(util::microtime::seconds_since(date_enter_wait)));
      LOG_BASIC(EXIT_WAIT);
      // end acquire
      
    }
  });
  deques.for_each([] (worker_id_t, deque_type& deque) {
    assert(deque.empty());
    deque.destroy();
  });
  LOG_BASIC(ALGO_PHASE);
  return visited;
}


} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_GRAPH_DFS_H_ */
