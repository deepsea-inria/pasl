#include "graphgenerators.hpp"
#include "graphconversions.hpp"
#include "frontierseg.hpp"
#include "bellman_ford.hpp"
#include "floyd.hpp"
#include "benchmark.hpp"
#include "edgelist.hpp"
#include "adjlist.hpp"
#include <map>
#include <thread>
#include <iostream>
#include <fstream>

using namespace pasl::graph;
using namespace pasl::data;

// Algorithms
enum {
  BELLMAN_FORD,
  FLOYD,
  NB_ALGO
};
std::string const algo_names[] = {
  "Bellman-Ford", 
  "Floyd-Warshall"
};


// Algorithm's thresholds
int pasl::graph::bellman_ford_par_serial_cutoff;
int pasl::graph::bellman_ford_par_bfs_cutoff;
int pasl::graph::floyd_warshall_par_bfs_cutoff;
int pasl::graph::floyd_warshall_par_serial_cutoff;


// Graph properties
using vtxid_type = int;
using adjlist_seq_type = pasl::graph::flat_adjlist_seq<vtxid_type>;
using adjlist_type = adjlist<adjlist_seq_type>;

// Testing constants
int pasl::graph::min_edge_weight = 1;
int pasl::graph::max_edge_weight = 100;
int edges_num = 1000;
int algo_num = 0;
int impl_num = 0;
int test_num = 0;
bool should_check_correctness = false;
bool generate_graph_file = false;
bool print_graph = false;
bool need_shuffle = false;
int cutoff = 1024;
double custom_lex_order_edges_fraction = 0.9;
double custom_avg_degree = 600;

// Testing graph values
int* res;
base_algo<adjlist_seq_type> * algo;
adjlist_type graph;
vtxid_type   source_vertex;

void print_graph_debug_info(const adjlist_type & graph, vtxid_type & src, std::string type) {
  std::cout << std::endl << "GRAPH INFO:" << std::endl;
  std::cout << "Type = " << type << std::endl;
  std::cout << "Source vertex = " << src << std::endl;
  std::cout << "Edges = " << graph.nb_edges << "; Vertices = " << graph.get_nb_vertices() << std::endl;      

  vtxid_type nb_vertices = graph.get_nb_vertices();
  auto num_edges = graph.nb_edges;
  auto num_less = 0;
  for (vtxid_type from = 0; from < nb_vertices; from++) {
    vtxid_type degree = graph.adjlists[from].get_out_degree();
    for (vtxid_type edge = 0; edge < degree; edge++) {
      vtxid_type to = graph.adjlists[from].get_out_neighbor(edge);
      if (from < to) num_less++;
    }
  }
  std::cout << "Fraction = " << (.0 + num_less) / num_edges << "; " << "AvgDegree = " << (.0 + num_edges) / nb_vertices << std::endl;;
}

void print_graph_to_file(const adjlist_type & graph, std::string type) {
  std::ofstream graph_file(graph_types[test_num] + ".dot");
  if (graph_file.is_open())
  {
    auto edge_num = graph.nb_edges; 
    auto nb_vertices = graph.get_nb_vertices(); 
    graph_file << "WeightedAdjacencyGraph\n";
    graph_file << nb_vertices << "\n" << edge_num << "\n";
    auto cur = 0;
    for (vtxid_type i = 0; i < nb_vertices; i++) {
      vtxid_type degree = graph.adjlists[i].get_out_degree();
      graph_file << cur << "\n";
      cur += degree;
    }
    for (vtxid_type i = 0; i < nb_vertices; i++) {
      vtxid_type degree = graph.adjlists[i].get_out_degree();
      for (vtxid_type edge = 0; edge < degree; edge++) {
        vtxid_type other = graph.adjlists[i].get_out_neighbor(edge);
        graph_file << other << "\n";
      }
    }
    for (vtxid_type i = 0; i < nb_vertices; i++) {
      vtxid_type degree = graph.adjlists[i].get_out_degree();
      for (vtxid_type edge = 0; edge < degree; edge++) {
        vtxid_type w = graph.adjlists[i].get_out_neighbor_weight(edge);
        graph_file << w << "\n";
      }
    }
    
    graph_file.close();
  }
}

bool same_arrays(int size, int * candidate, int * correct) {
  for (int i = 0; i < size; ++i) {
    if (candidate[i] != correct[i]) {
      std::cout << "On graph " << graph << std::endl;
      std::cout << "Actual ";
      for (int j = 0; j < size; ++j) std::cout << candidate[j] << " ";
      std::cout << std::endl << "Expected ";
      for (int j = 0; j < size; ++j) std::cout << correct[j] << " ";
      std::cout << std::endl;
      return false;
    }
  }
  return true;
}

static inline void parse_fname(std::string fname, std::string& base, std::string& extension) {
  if (fname == "")
    pasl::util::atomic::die("bogus filename");
  std::stringstream ss(fname);
  std::getline(ss, base, '.');
  std::getline(ss, extension);
}

template <class Adjlist>
bool load_graph_from_file(Adjlist& graph) {
  std::string infile = pasl::util::cmdline::parse_or_default_string("infile", "");
  if (infile == "") return false;
  std::string base;
  std::string extension;
  parse_fname(infile, base, extension);
  if (extension == "dot") {
    std::cout << "Reading dot graph from file ..." << std::endl;   
    using vtxid_type = typename Adjlist::vtxid_type;
    using edge_type = wedge<vtxid_type>;
    using edgelist_bag_type = pasl::data::array_seq<edge_type>;
    using edgelist_type = edgelist<edgelist_bag_type>;
    edgelist_type edg;
    
    std::ifstream dot_file(infile);
    std::string name;
    getline(dot_file, name);
    int vertices, edges;
    dot_file >> vertices >> edges;
    int* offsets = mynew_array<int>(vertices);
    int prev;
    dot_file >> prev;
    for (int i = 0; i < vertices - 1; ++i) {
      int cur_off;      
      dot_file >> cur_off;
      offsets[i] = cur_off - prev;
      prev = cur_off;
    }
    offsets[vertices - 1] = edges - prev;

    edg.edges.alloc(edges);
    int cur = 0;
    for (int i = 0; i < vertices; ++i) {
      for (int j = 0; j < offsets[i]; ++j) {
        int to;
        dot_file >> to;
        edg.edges[cur++] = edge_type(i, to);                
      }
    }
    cur = 0;
    for (int i = 0; i < vertices; ++i) {
      for (int j = 0; j < offsets[i]; ++j) {
        int w;
        dot_file >> w;
        edg.edges[cur++].w = w;
      }
    }
    
    edg.nb_vertices = vertices;
    edg.check();
    adjlist_from_edgelist(edg, graph);
    dot_file.close();
    return true;    
  }
  else 
    return false;
}

int main(int argc, char ** argv) {
  
  auto init = [&] {   
    
    // Parsing arguments
    should_check_correctness = pasl::util::cmdline::parse_or_default_bool("check", should_check_correctness, should_check_correctness);
    need_shuffle = pasl::util::cmdline::parse_or_default_bool("shuffle", need_shuffle, need_shuffle);    
    generate_graph_file = pasl::util::cmdline::parse_or_default_bool("gen_file", generate_graph_file, generate_graph_file);
    print_graph = pasl::util::cmdline::parse_or_default_bool("graph", print_graph, print_graph);    
    algo_num = pasl::util::cmdline::parse_or_default_int("algo_num", algo_num);    
    impl_num = pasl::util::cmdline::parse_or_default_int("impl_num", impl_num);    
    custom_avg_degree = pasl::util::cmdline::parse_or_default_int("custom_deg", custom_avg_degree);
    custom_lex_order_edges_fraction = pasl::util::cmdline::parse_or_default_int("custom_fraction", custom_lex_order_edges_fraction);
    test_num = pasl::util::cmdline::parse_or_default_int("test_num", test_num);
    edges_num = pasl::util::cmdline::parse_or_default_int("edges", edges_num);
    cutoff = pasl::util::cmdline::parse_or_default_int("cutoff", cutoff);
    pasl::graph::min_edge_weight = pasl::util::cmdline::parse_or_default_int("min", pasl::graph::min_edge_weight);
    pasl::graph::max_edge_weight = pasl::util::cmdline::parse_or_default_int("max", pasl::graph::max_edge_weight);
    
    // Init thresholds and check args
    assert(algo_num < NB_ALGO);
    assert(test_num < NB_GENERATORS);
    
    if (test_num != RANDOM_CUSTOM) {
      custom_avg_degree = -1;
      custom_lex_order_edges_fraction = -1;
    } else {
      assert(custom_avg_degree >= 0);
      assert(0 <= custom_lex_order_edges_fraction && custom_lex_order_edges_fraction <= 1);      
    }
    pasl::graph::bellman_ford_par_serial_cutoff = cutoff;
    pasl::graph::bellman_ford_par_bfs_cutoff = cutoff;
    pasl::graph::floyd_warshall_par_bfs_cutoff = cutoff;
    pasl::graph::floyd_warshall_par_serial_cutoff = cutoff;
    
    switch (algo_num) {
      case BELLMAN_FORD:
        algo = new bellman_ford_algo<adjlist_seq_type>();
        break;
      case FLOYD:
        algo = new floyd_algo<adjlist_seq_type>();
        break;        
    }
    assert(impl_num < algo->get_impl_count());
    
    // Reading/generating graph
    std::cout << "Testing " << algo_names[algo_num] << "(" << algo->get_impl_name(impl_num) << ")" << " with " << graph_types[test_num] << std::endl;      
    graph = adjlist_type();
    source_vertex = 0;
    std::string graph_type = "from_file";
    if (!load_graph_from_file(graph)) {
      std::cout << "Generating graph..." << std::endl;        
      graph_type = graph_types[test_num]; 
      generator_type which_generator;
      which_generator.ty = test_num;
      source_vertex = generate(which_generator, edges_num, graph, custom_lex_order_edges_fraction, custom_avg_degree);
      std::cout << "Done generating" << std::endl;        
    }
    print_graph_debug_info(graph, source_vertex, graph_types[test_num]);      
    
    // Generating dot file with graph
    if (generate_graph_file) {
      std::cout << "Writing graph to file..." << std::endl;
      print_graph_to_file(graph, graph_type);
      std::cout << "Done" << std::endl;      
    }

    // Other stuff
    if (print_graph) {
      std::cout << std::endl << "Source : " << source_vertex << std::endl;
      std::cout << graph << std::endl;
    }
    if (!should_check_correctness) {
      std::cout << std::endl << "WARNING! Check only performance" << std::endl;
      return;
    } else {
      res = algo->get_dist(0, graph, source_vertex);      
    }

  };
  
  auto run = [&] (bool sequential) {
    int* our_res = algo->get_dist(impl_num, graph, source_vertex);
    if (should_check_correctness && same_arrays(graph.get_nb_vertices(), our_res, res)) {
      std::cout << "OK" << std::endl;
    }
    delete(our_res);
  };
  auto output = [&] {};
  auto destroy = [&] {
    delete(res);
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);  
  return 0;
}
