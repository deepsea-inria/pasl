/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file search.cpp
 *
 */

#include <sys/mman.h>

#include "graphfileshared.hpp"
#include "bfs.hpp"
#include "dfs.hpp"
#include "benchmark.hpp"
#include "container.hpp"
#include "ls_bag.hpp"
#include "frontierseg.hpp"

#ifdef USE_MALLOC_COUNT
#include "malloc_count.h"
#endif

/***********************************************************************/

namespace pasl {
namespace graph {

bool should_disable_random_permutation_of_vertices;

/*---------------------------------------------------------------------*/

template <class Adjlist>
using chunkedseq_stack = data::pcontainer::stack<typename Adjlist::vtxid_type>;

template <class Adjlist>
using chunkedseq_deque = data::pcontainer::deque<typename Adjlist::vtxid_type>;

template <class Adjlist>
using chunkedseq_bag = data::pcontainer::bag<typename Adjlist::vtxid_type>;

template <class Adjlist>
using chunkedftree_stack = data::pcontainer::ftree_stack<typename Adjlist::vtxid_type>;

template <class Adjlist>
using chunkedftree_deque = data::pcontainer::ftree_deque<typename Adjlist::vtxid_type>;

template <class Adjlist>
using chunkedftree_bag = data::pcontainer::ftree_bag<typename Adjlist::vtxid_type>;

template <class Adjlist>
using stl_deque = data::stl::deque_seq<typename Adjlist::vtxid_type>;

#ifdef HAVE_ROPE
template <class Adjlist>
using stl_rope = data::stl::rope_seq<typename Adjlist::vtxid_type>;
#endif

#ifdef HAVE_CORD
template <class Adjlist>
using cord = data::stl::cord_seq<typename Adjlist::vtxid_type>;
#endif

template <class Adjlist>
using ls_bag = data::Bag<typename Adjlist::vtxid_type>;

/*---------------------------------------------------------------------*/

//int cong_pdfs_cutoff = 10000;
int our_pseudodfs_cutoff = 10000;
int ls_pbfs_cutoff = 10000;
int ls_pbfs_loop_cutoff = 10000;
int our_bfs_cutoff = 10000;
int our_lazy_bfs_cutoff = 10000;


/*---------------------------------------------------------------------*/

template <class Adjlist, class Search, class Report, class Destroy>
void search_benchmark_select_input_graph(const Search& search,
                                         const Report& report,
                                         const Destroy& destroy) {
  using vtxid_type = typename Adjlist::vtxid_type;
  Adjlist graph;
  vtxid_type source = vtxid_type(util::cmdline::parse_or_default_uint64("source", 0));
  auto init = [&] {
    util::cmdline::argmap_dispatch tmg;
    tmg.add("from_file",          [&] { load_graph_from_file(graph); });
    tmg.add("by_generator",       [&] { generate_graph(graph); });
    util::cmdline::dispatch_by_argmap(tmg, "load");
    mlockall(0);
  };
  auto run = [&] (bool sequential) {
    search(graph, source);
  };
  auto output = [&] {
    report(graph);
    print_adjlist_summary(graph);
    std::cout << "chunk_capacity\t" << data::pcontainer::chunk_capacity << std::endl;
  };
  sched::launch(init, run, output, destroy);
}
  
void report_common_results() {
  std::cout << "chunk_capacity\t" << data::pcontainer::chunk_capacity << std::endl;
#ifdef GRAPH_SEARCH_STATS
  std::cout << "peak_frontier_size\t" << peak_frontier_size << std::endl;
#endif
}
  
template <class Adjlist, class Load_visited_fct>
void report_dfs_results(const Adjlist& graph,
                        const Load_visited_fct& load_visited_fct) {
  using vtxid_type = typename Adjlist::vtxid_type;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  vtxid_type nb_visited = pbbs::sequence::plusReduce((vtxid_type*)nullptr, nb_vertices, load_visited_fct);
  std::cout << "nb_visited\t" << nb_visited << std::endl;
  report_common_results();
}
  
template <class Adjlist, class Counter_cell, class Load_dist_fct>
void report_bfs_results(const Adjlist& graph,
                        Counter_cell unknown,
                        const Load_dist_fct& load_dist_fct) {
  using vtxid_type = typename Adjlist::vtxid_type;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  vtxid_type max_dist = pbbs::sequence::maxReduce((vtxid_type*)nullptr, nb_vertices, load_dist_fct);
  auto is_visited = [&] (vtxid_type i) { return load_dist_fct(i) == unknown ? 0 : 1; };
  vtxid_type nb_visited = pbbs::sequence::plusReduce((vtxid_type*)nullptr, nb_vertices, is_visited);
  std::cout << "max_dist\t" << max_dist << std::endl;
  std::cout << "nb_visited\t" << nb_visited << std::endl;
  report_common_results();
}

/*---------------------------------------------------------------------*/

template <class Adjlist>
void search_benchmark_sequential_select_algo() {
  using adjlist_type = Adjlist;
  using adjlist_seq_type = typename adjlist_type::adjlist_seq_type;
  using adjlist_alias_type = typename adjlist_type::alias_type;
  using vtxid_type = typename Adjlist::vtxid_type;
  using search_type = std::function<void (const adjlist_type& graph, vtxid_type source)>;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  vtxid_type* dists = nullptr;
  int* visited = nullptr;
  util::cmdline::argmap<search_type> m;
#ifndef SKIP_FAST
  m.add("bfs_by_dual_arrays", [&] (const adjlist_type& graph, vtxid_type source) {
    dists = bfs_by_dual_arrays<adjlist_seq_type>(graph, source); });
  m.add("bfs_by_frontier_segment", [&] (const adjlist_type& graph, vtxid_type source) {
    dists = bfs_by_frontier_segment<adjlist_type, frontiersegbag<adjlist_alias_type>>(graph, source); });
#endif
  m.add("dfs_by_vertexid_array", [&] (const adjlist_type& graph, vtxid_type source) {
    visited = dfs_by_vertexid_array(graph, source); });
#ifndef SKIP_FAST
  m.add("dfs_by_frontier_segment", [&] (const adjlist_type& graph, vtxid_type source) {
    visited = dfs_by_frontier_segment<adjlist_type, frontiersegbag<adjlist_alias_type>>(graph, source); });
  m.add("report_nb_edges_processed", [&] (const adjlist_type& graph, vtxid_type source) {
    long nb_edges_processed;
    visited = dfs_by_vertexid_array<adjlist_seq_type,true>(graph, source, &nb_edges_processed);
    std::cout << "nb_edges_processed\t" << nb_edges_processed << std::endl; });
#endif
#ifndef SKIP_OTHER_SEQUENTIAL
  m.add("bfs_by_array", [&] (const adjlist_type& graph, vtxid_type source) {
    dists = bfs_by_array(graph, source); });
#endif

  auto search = m.find_by_arg("algo");
  auto report = [&] (const adjlist_type& graph) {
    if (dists != nullptr)
      report_bfs_results(graph, unknown, [&] (vtxid_type i) { return dists[i]; });
    else
      report_dfs_results(graph, [&] (vtxid_type i) { return visited[i]; });
  };
  auto destroy = [&] {
    if (dists != NULL)
      data::myfree(dists);
    else if (visited != NULL)
      data::myfree(visited);
  };
  search_benchmark_select_input_graph<Adjlist>(search, report, destroy);
}

//--------------

template <class Adjlist, class Frontier>
void search_benchmark_frontier_sequential_select_algo() {
  using adjlist_type = Adjlist;
  using adjlist_seq_type = typename adjlist_type::adjlist_seq_type;
  using adjlist_alias_type = typename adjlist_type::alias_type;
  using vtxid_type = typename Adjlist::vtxid_type;
  using search_type = std::function<void (const adjlist_type& graph, vtxid_type source)>;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  vtxid_type* dists = NULL;
  int* visited = NULL;
  util::cmdline::argmap<search_type> m;
#ifndef SKIP_FAST
  m.add("dfs_by_vertexid_frontier", [&] (const adjlist_type& graph, vtxid_type source) {
    visited = dfs_by_vertexid_frontier<adjlist_seq_type, Frontier>(graph, source); });
#endif
#ifndef SKIP_OTHER_SEQUENTIAL
  m.add("bfs_by_dynamic_array", [&] (const adjlist_type& graph, vtxid_type source) {
    dists = bfs_by_dynamic_array<adjlist_seq_type, Frontier>(graph, source); });
  m.add("bfs_by_dual_frontiers_and_foreach", [&] (const adjlist_type& graph, vtxid_type source) {
    dists = bfs_by_dual_frontiers_and_foreach<adjlist_seq_type, Frontier>(graph, source); });
  m.add("bfs_by_dual_frontiers_and_pushpop", [&] (const adjlist_type& graph, vtxid_type source) {
    dists = bfs_by_dual_frontiers_and_pushpop<adjlist_seq_type, Frontier>(graph, source); });
#endif
  auto search = m.find_by_arg("algo");
  auto report = [&] (const adjlist_type& graph) {
    if (dists != nullptr)
      report_bfs_results(graph, unknown, [&] (vtxid_type i) { return dists[i]; });
    else
      report_dfs_results(graph, [&] (vtxid_type i) { return visited[i]; });
  };
  auto destroy = [&] {
    if (dists != nullptr)
      data::myfree(dists);
    else if (visited != nullptr)
      data::myfree(visited);
  };
  search_benchmark_select_input_graph<Adjlist>(search, report, destroy);
}

//--------------

template <class Adjlist, bool idempotent>
void search_benchmark_parallel_select_algo() {
  using adjlist_type = Adjlist;
  using adjlist_seq_type = typename adjlist_type::adjlist_seq_type;
  using adjlist_alias_type = typename adjlist_type::alias_type;
  using vtxid_type = typename Adjlist::vtxid_type;
  using search_type = std::function<void (const adjlist_type& graph, vtxid_type source)>;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  std::atomic<vtxid_type>* dists = nullptr;
  std::atomic<int>* visited = nullptr;
  util::cmdline::argmap<search_type> m;
#ifndef SKIP_FAST
  m.add("pbbs_pbfs",   [&] (const adjlist_type& graph, vtxid_type source) {
    dists = pbbs_pbfs<idempotent, adjlist_type>(graph, source); });
  m.add("our_pbfs",    [&] (const adjlist_type& graph, vtxid_type source) {
    our_bfs_cutoff = util::cmdline::parse_or_default_int("our_pbfs_cutoff", 1024);
    dists = our_bfs<idempotent>::template main<adjlist_type, frontiersegbag<adjlist_alias_type>>(graph, source); });
  m.add("our_pbfs_with_swap",    [&] (const adjlist_type& graph, vtxid_type source) {
    our_bfs_cutoff = util::cmdline::parse_or_default_int("our_pbfs_cutoff", 1024);
    dists = our_bfs<idempotent>::template main_with_swap<adjlist_type, frontiersegbag<adjlist_alias_type>>(graph, source); });
  m.add("our_lazy_pbfs",    [&] (const adjlist_type& graph, vtxid_type source) {
    our_lazy_bfs_cutoff = util::cmdline::parse_or_default_int("our_lazy_pbfs_cutoff", 1024);
    dists = our_lazy_bfs<idempotent>::template main<adjlist_type, frontiersegbag<adjlist_alias_type>>(graph, source); });
#endif
  m.add("our_pseudodfs",   [&] (const adjlist_type& graph, vtxid_type source) {
    our_pseudodfs_cutoff = util::cmdline::parse_or_default_int("our_pseudodfs_cutoff", 1024);
    visited = our_pseudodfs<adjlist_type, frontiersegbag<adjlist_alias_type>, idempotent>(graph, source); });
  m.add("cong_pseudodfs",   [&] (const adjlist_type& graph, vtxid_type source) {
    visited = cong_pseudodfs<adjlist_seq_type, idempotent>(graph, source); });

  auto search = m.find_by_arg("algo");
  auto report = [&] (const adjlist_type& graph) {
    if (dists != nullptr)
      report_bfs_results(graph, unknown, [&] (vtxid_type i) { return dists[i].load(); });
    else
      report_dfs_results(graph, [&] (vtxid_type i) { return vtxid_type(visited[i].load()); });
  };
  auto destroy = [&] {
    if (dists != nullptr)
      data::myfree(dists);
    else if (visited != nullptr)
      data::myfree(visited);
  };
  search_benchmark_select_input_graph<Adjlist>(search, report, destroy);
}

//--------------

template <class Adjlist, class Frontier, bool idempotent>
void search_benchmark_frontier_parallel_select_algo() {
  using adjlist_type = Adjlist;
  using adjlist_seq_type = typename adjlist_type::adjlist_seq_type;
  using adjlist_alias_type = typename adjlist_type::alias_type;
  using vtxid_type = typename Adjlist::vtxid_type;
  using search_type = std::function<void (const adjlist_type& graph, vtxid_type source)>;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  std::atomic<vtxid_type>* dists = nullptr;
  std::atomic<int>* visited = nullptr;
  util::cmdline::argmap<search_type> m;
#ifndef SKIP_FAST
  m.add("ls_pbfs",   [&] (const adjlist_type& graph, vtxid_type source) {
    ls_pbfs_cutoff = util::cmdline::parse_or_default_int("ls_pbfs_cutoff", 1024);
    ls_pbfs_loop_cutoff = util::cmdline::parse_or_default_int("ls_pbfs_loop_cutoff", 1024);
    dists = ls_pbfs<idempotent>::template main<adjlist_seq_type, Frontier>(graph, source); });
#endif

  auto search = m.find_by_arg("algo");
  auto report = [&] (const adjlist_type& graph) {
    if (dists != nullptr)
      report_bfs_results(graph, unknown, [&] (vtxid_type i) { return dists[i].load(); });
    else
      report_dfs_results(graph, [&] (vtxid_type i) { return vtxid_type(visited[i].load()); });
  };
  auto destroy = [&] {
    if (dists != nullptr)
      data::myfree(dists);
    else if (visited != nullptr)
      data::myfree(visited);
  };
  search_benchmark_select_input_graph<Adjlist>(search, report, destroy);
}


/*---------------------------------------------------------------------*/

static std::string get_algo() {
  return util::cmdline::parse_or_default_string("algo", "");
}

static bool is_parallel_algo() {
  std::string algo = get_algo();
  if (algo == "ls_pbfs")               return true;
  if (algo == "our_pbfs")              return true;
  if (algo == "our_pbfs_with_swap")    return true;
  if (algo == "our_lazy_pbfs")         return true;
  if (algo == "our_pseudodfs")         return true;
  if (algo == "cong_pseudodfs")        return true;
  if (algo == "pbbs_pbfs")             return true;
  return false;
}

static bool is_frontier_algo() {
  std::string algo = get_algo();
  return (algo == "bfs_by_dynamic_array" || algo == "bfs_by_dual_frontiers_and_foreach"
       || algo == "bfs_by_dual_frontiers_and_pushpop" || algo == "dfs_by_vertexid_frontier"
       || algo == "ls_pbfs");
}


/*---------------------------------------------------------------------*/

template <class Adjlist>
void search_benchmark_select_parallelism() {
  if (is_parallel_algo()) {
    bool idempotent = util::cmdline::parse_or_default_bool("idempotent", false);
    if (idempotent)
      search_benchmark_parallel_select_algo<Adjlist, true>();
    else
      search_benchmark_parallel_select_algo<Adjlist, false>();
  } else {
    search_benchmark_sequential_select_algo<Adjlist>();
  }
}

template <class Adjlist, class Frontier>
void search_benchmark_frontier_select_parallelism() {
  if (is_parallel_algo()) {
    bool idempotent = util::cmdline::parse_or_default_bool("idempotent", false);
    if (idempotent)
      search_benchmark_frontier_parallel_select_algo<Adjlist, Frontier, true>();
    else
      search_benchmark_frontier_parallel_select_algo<Adjlist, Frontier, false>();
  } else {
    search_benchmark_frontier_sequential_select_algo<Adjlist, Frontier>();
  }
}

template <class Adjlist>
void search_benchmark_select_frontier() {
  util::cmdline::argmap_dispatch c;
#ifndef SKIP_FAST
  c.add("chunkedseq_bag",       [&] {
    search_benchmark_frontier_select_parallelism<Adjlist, chunkedseq_bag<Adjlist>>(); });
  c.add("stl_deque",       [&] {
    search_benchmark_frontier_select_parallelism<Adjlist, stl_deque<Adjlist>>(); });
  c.add("ls_bag",       [&] {
    search_benchmark_frontier_select_parallelism<Adjlist, ls_bag<Adjlist>>(); });
#ifndef SKIP_OTHER_FRONTIERS
  c.add("chunkedseq",       [&] {
    search_benchmark_frontier_select_parallelism<Adjlist, chunkedseq_deque<Adjlist>>(); });
  c.add("chunkedseq_stack",       [&] {
    search_benchmark_frontier_select_parallelism<Adjlist, chunkedseq_stack<Adjlist>>(); });
  c.add("chunkedftree_stack",       [&] {
    search_benchmark_frontier_select_parallelism<Adjlist, chunkedftree_stack<Adjlist>>(); });
  c.add("chunkedftree",       [&] {
    search_benchmark_frontier_select_parallelism<Adjlist, chunkedftree_deque<Adjlist>>(); });
  c.add("chunkedftree_bag",       [&] {
    search_benchmark_frontier_select_parallelism<Adjlist, chunkedftree_bag<Adjlist>>(); });
#endif
#ifdef HAVE_ROPE
  c.add("stl_rope",       [&] {
    search_benchmark_frontier_select_parallelism<Adjlist, stl_rope<Adjlist>>(); });
#endif
#ifdef HAVE_CORD
  c.add("cord",       [&] {
    search_benchmark_frontier_select_parallelism<Adjlist, cord<Adjlist>>(); });
#endif
#endif
  util::cmdline::dispatch_by_argmap(c, "frontier");
}


/*---------------------------------------------------------------------*/

template <class Adjlist>
void search_benchmark_select_mode() {
  if (is_frontier_algo()) {
     search_benchmark_select_frontier<Adjlist>();
  } else {
     search_benchmark_select_parallelism<Adjlist>();
  }
}

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

using namespace pasl;

int main(int argc, char ** argv) {
  util::cmdline::set(argc, argv);

  graph::should_disable_random_permutation_of_vertices = util::cmdline::parse_or_default_bool("should_disable_random_permutation_of_vertices", false, false);

  using vtxid_type32 = int;
  using adjlist_seq_type32 = graph::flat_adjlist_seq<vtxid_type32>;
  using adjlist_type32 = graph::adjlist<adjlist_seq_type32>;

  using vtxid_type64 = long;
  using adjlist_seq_type64 = graph::flat_adjlist_seq<vtxid_type64>;
  using adjlist_type64 = graph::adjlist<adjlist_seq_type64>;

  int nb_bits = util::cmdline::parse_or_default_int("bits", 32);

  #ifndef SKIP_32_BITS
  if (nb_bits == 32)
    graph::search_benchmark_select_mode<adjlist_type32>();
  else
  #endif
  #ifndef SKIP_64_BITS
  if (nb_bits == 64)
    graph::search_benchmark_select_mode<adjlist_type64>();
  else
  #endif
    util::atomic::die("bits must be either 32 or 64");

#ifdef USE_MALLOC_COUNT
  malloc_pasl_report();
#endif

  return 0;
}

/***********************************************************************/
