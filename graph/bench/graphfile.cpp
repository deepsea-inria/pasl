/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file search.cpp
 *
 */

#include "graphfileshared.hpp"
#include "benchmark.hpp"

/***********************************************************************/

namespace pasl {
namespace graph {
  
bool should_disable_random_permutation_of_vertices;

/*---------------------------------------------------------------------*/

template <class Adjlist>
void convert() {
  bool should_generate;
  bool should_generate_by_nb_edges_target;
  auto init = [&] {
    should_generate = util::cmdline::parse_or_default_string("generator", "") != "";
    should_generate_by_nb_edges_target = util::cmdline::parse_or_default_uint64("nb_edges_target", 0) > 0;
    should_disable_random_permutation_of_vertices = util::cmdline::parse_or_default_bool("should_disable_random_permutation_of_vertices", false, false);
  };
  auto run = [&] (bool sequential) {
    Adjlist graph;
    if (should_generate) {
      if (should_generate_by_nb_edges_target)
        generate_graph_by_nb_edges(graph);
      else
        generate_graph(graph);
    } else {
      load_graph_from_file(graph);
    }
    std::cout << "nb_vertices\t" << graph.get_nb_vertices() << std::endl;
    std::cout << "nb_edges\t" << graph.nb_edges << std::endl;
    write_graph_to_file(graph);
  };
  auto output = [&] { };
  auto destroy = [&] { };
  sched::launch(init, run, output, destroy);
  return;
}
  
} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

using namespace pasl;

int main(int argc, char ** argv) {
  util::cmdline::set(argc, argv);
  
  int randseed = util::cmdline::parse_or_default_int("seed", 123232, false);
  srand(randseed);
  
  using vtxid_type32 = int;
  using adjlist_seq_type32 = graph::flat_adjlist_seq<vtxid_type32>;
  using adjlist_type32 = graph::adjlist<adjlist_seq_type32>;
  
  using vtxid_type64 = long;
  using adjlist_seq_type64 = graph::flat_adjlist_seq<vtxid_type64>;
  using adjlist_type64 = graph::adjlist<adjlist_seq_type64>;
  
  int nb_bits = util::cmdline::parse_or_default_int("bits", 32);
  
  if (nb_bits == 32 )
    graph::convert<adjlist_type32>();
  else if (nb_bits == 64)
    graph::convert<adjlist_type64>();
  else
    util::atomic::die("bits must be either 32 or 64");
  
  return 0;
}

/***********************************************************************/
