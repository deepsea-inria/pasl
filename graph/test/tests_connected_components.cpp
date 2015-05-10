#include <fstream>

#include "nb_components.hpp"
#include "graphgenerators.hpp"
#include "graphconversions.hpp"
#include "ls_bag.hpp"
#include "frontierseg.hpp"
#include "bfs.hpp"
#include "dfs.hpp"
#include "benchmark.hpp"

using namespace pasl::graph;
using namespace pasl::data;
  
int nb_tests = 1000;

namespace pasl {
namespace graph {
int our_bfs_cutoff = 8;
};
};

// Graph properties
using vtxid_type = long;
//using adjlist_seq_type = pasl::graph::flat_adjlist_seq<vtxid_type>;
//using edgelist_type = pasl::graph::edgelist<pasl::graph::edge<vtxid_type> >;
using edge_type = pasl::graph::edge<vtxid_type>;
using edgelist_bag_type = pasl::data::array_seq<edge_type>;
using edgelist_type = pasl::graph::edgelist<edgelist_bag_type>;

using adjlist_seq_type = pasl::graph::flat_adjlist_seq<vtxid_type>;
using adjlist_type = adjlist<adjlist_seq_type>;

using adjlist_alias_type = typename adjlist_type::alias_type;

using frontiersegbag_type = pasl::graph::frontiersegbag<adjlist_alias_type>;

// Testing constants
//int pasl::graph::min_edge_weight = 1;
//int pasl::graph::max_edge_weight = 100;
unsigned long edges_num = 1000000;//000000;
long vertices_num = 100000;
unsigned int algo_id = 0;

void check(int argc, char ** argv, bool check_only_correctness = false) {
  edgelist_type graph;
  adjlist_type adjlist;
  vtxid_type algo_result, correct_result;

  auto init = [&] {
    if (!check_only_correctness)
      std::cout << "Generating graph..." << std::endl;        
    generator_type which_generator;
    // which_generator.ty = RANDOM_SPARSE; //COMPLETE;
    which_generator.ty = RANDOM_BY_EDGES_AND_VERTICES;
    //graph = adjlist_type();
    generate(edges_num, which_generator, graph, vertices_num);

    adjlist_from_edgelist(graph, adjlist, false);

    if (!check_only_correctness)
      std::cout << "Done generating" << std::endl;  
    if (!check_only_correctness)
      std::cout << "number of vertices = " << adjlist.get_nb_vertices() << std::endl;

    vtxid_type nb_edges = graph.get_nb_edges();
    if (!check_only_correctness)
      std::cout << "edges_count = " << nb_edges << std::endl;
    if (!check_only_correctness)
      std::cout << "calculate number of components (for correctness)" << std::endl;
    correct_result = nb_components_dfs_by_array(adjlist);
    if (!check_only_correctness)
      std::cout << "number of components = " << correct_result << std::endl;
  };
  
  auto run = [&] (bool sequential) {
    switch (algo_id) {
      case 0:
        algo_result = nb_components_bfs_by_array(adjlist); 
        break;
      case 1:
        algo_result = nb_components_disjoint_set_union(graph);
        break;
      case 2:
        algo_result = nb_components_pbbs_pbfs(adjlist);
        break;
      case 3:
        algo_result = our_bfs_nc<false>::main<adjlist_type, frontiersegbag_type>(adjlist);
        break;
      case 4:
        algo_result = nb_components_star_contraction_seq(graph);
        break;
      case 5:
        algo_result = nb_components_star_contraction_par(graph);
        break;
      default:
        break;
    }
    // 
    // algo_result = nb_components_disjoint_set_union(graph);
    // algo_result = nb_components_pbbs_pbfs(adjlist);
    // algo_result = our_bfs_nc<false>::main<adjlist_type, frontiersegbag_type>(adjlist);
    // algo_result = nb_components_star_contraction_seq(graph);
    // algo_result = nb_components_star_contraction_par(graph);
  };
  if (!check_only_correctness) {
    auto output = [&] {
      assert(algo_result == correct_result);
      std::cout << "All tests complete" << std::endl;
    };
    auto destroy = [&] {
      ;
    };
    pasl::sched::launch(argc, argv, init, run, output, destroy);
  } else {
    init();
    run(true);
    assert(algo_result == correct_result);
  }
}

int main(int argc, char ** argv) {  
  pasl::util::cmdline::set(argc, argv);
  bool check_only_correctness = pasl::util::cmdline::parse_or_default_bool("check_only_correctness", false);
  edges_num = pasl::util::cmdline::parse_or_default_int("edges_num", edges_num);
  vertices_num = pasl::util::cmdline::parse_or_default_int("vertices_num", vertices_num);
  algo_id = pasl::util::cmdline::parse_or_default_int("algo_id", algo_id);
  size_t cur_nb_tests = (check_only_correctness ? nb_tests : 1);
  int last_done = 0;
  for (size_t test_n = 0; test_n != cur_nb_tests; ++test_n) {
    int cur_done = test_n * 100 / cur_nb_tests;
    if (cur_done != last_done) {
      std::cout << "test = " << test_n << std::endl;
      last_done = cur_done;
    }
    check(argc, argv, check_only_correctness);
  }
  return 0;
}

/***********************************************************************/

