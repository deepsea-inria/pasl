#include "graphgenerators.hpp"
#include "graphconversions.hpp"
#include "frontierseg.hpp"
#include "bellman_ford.hpp"
#include "benchmark.hpp"
#include <gtest/gtest.h>
#include "edgelist.hpp"
#include "adjlist.hpp"
#include <map>
#include <thread>

using namespace pasl::graph;
using namespace pasl::data;

unsigned int nthreads = std::thread::hardware_concurrency();

// Algorithm's thresholds
const int pasl::graph::bellman_ford_par_by_vertices_cutoff 	= 100000;
const int pasl::graph::bellman_ford_par_by_edges_cutoff 		= 1000000;
const int pasl::graph::bellman_ford_bfs_process_layer_cutoff = 1000000;
const int pasl::graph::bellman_ford_bfs_process_next_vertices_cutoff = 1000000;
const std::function<bool(double, double)> pasl::graph::algo_chooser_pred = [] (double fraction, double avg_deg) -> bool {
  if (avg_deg < 20) {
    return false;
  }
  if (avg_deg > 200) {
    return true;
  }
  return fraction > 0.75;
}; 

// Graph properties
using vtxid_type = long;
using adjlist_seq_type = pasl::graph::flat_adjlist_seq<vtxid_type>;
using adjlist_type = adjlist<adjlist_seq_type>;

// Testing constants
const int pasl::graph::min_edge_weight = 1;
const int pasl::graph::max_edge_weight = 100;

const std::set<int> enabled_algo {
  SERIAL_CLASSIC,
//  SERIAL_YEN,  
//  SERIAL_BFS,
//  PAR_NUM_VERTICES,
//  PAR_NUM_EDGES,
  PAR_BFS,
//  PAR_COMBINED  
};
const std::set<int> enabled_tests {
  COMPLETE,
//  BALANCED_TREE,
//  CHAIN,
//  STAR,
//  SQUARE_GRID, 
//  RANDOM_SPARSE,
//  RANDOM_DENSE,
//  RANDOM_CUSTOM
};
std::map<int, size_t> test_edges_number {
  {COMPLETE, 			4000000},
  {BALANCED_TREE, 10000},
  {CHAIN, 				10000},
  {STAR, 					100000},
  {SQUARE_GRID, 	100000},
  {RANDOM_SPARSE, 10000},
  {RANDOM_DENSE, 	100000},
  {RANDOM_CUSTOM, 1000}
};
const double custom_lex_order_edges_fraction = 0.5;
const double custom_avg_degree = 20;

int* res_by_type[30];
bool enabled_graph[30];
adjlist_type graph_by_type[30];
vtxid_type   src_by_type[30];


void print_graph_debug_info(const adjlist_type & graph) {
  vtxid_type nb_vertices = graph.get_nb_vertices();
  auto num_edges = graph.nb_edges;
  auto num_less = 0;
  for (size_t from = 0; from < nb_vertices; from++) {
    vtxid_type degree = graph.adjlists[from].get_out_degree();
    for (vtxid_type edge = 0; edge < degree; edge++) {
      vtxid_type to = graph.adjlists[from].get_out_neighbor(edge);
      if (from < to) num_less++;
    }
  }
  std::cout << "Fraction = " << (.0 + num_less) / num_edges << " ";
  std::cout << "AvgDegree = " << (.0 + num_edges) / nb_vertices << " ";
}

int main(int argc, char ** argv) {
  auto init = [&] {
    for (int i = 0; i < NB_GENERATORS; ++i) {
      enabled_graph[i] = enabled_tests.count(i) != 0;
    }
    
    std::cout << "Generating graphs..." << std::endl;    
    for (int i = 0; i < NB_GENERATORS; ++i) {
      if (!enabled_graph[i]) continue;
      generator_type which_generator;
      which_generator.ty = i;
      graph_by_type[i] = adjlist_type();
      if (i == RANDOM_CUSTOM) {
        src_by_type[i] = generate(which_generator, test_edges_number[i], graph_by_type[i], custom_lex_order_edges_fraction, custom_avg_degree);
      } else {
        src_by_type[i] = generate(which_generator, test_edges_number[i], graph_by_type[i], -1, -1, true);
      }
      std::cout << "Done generating " << graph_types[i] << " with ";      
      print_graph_debug_info(graph_by_type[i]);      
      res_by_type[i] = bellman_ford_seq_classic(graph_by_type[i], (int) src_by_type[i]);
    }
    std::cout << "DONE" << std::endl;
  };
  
  auto run = [&] (bool sequential) {
    // Composing filter argument
    std::string filter_argument;
    bool is_first = true;
    for (auto it = enabled_algo.begin(); it != enabled_algo.end(); it++) {
      auto algo = *it;
      for (auto it_test = enabled_tests.begin(); it_test != enabled_tests.end(); it_test++) {
        auto test = *it_test;
        if (!is_first) filter_argument += ":";
        is_first = false;
        filter_argument += "*" + algo_names[algo] + "*" + graph_types[test] + "*";        
      }
    }
    int size = 0;
    while (argv[argc - 1][size] != 0) size++;
    char * cstr = new char [filter_argument.length() + size + 1];
    std::strcpy (cstr, argv[argc - 1]);
    std::strcpy (cstr + size, filter_argument.c_str());
    argv[argc - 1] = cstr;
    testing::InitGoogleTest(&argc, argv);
    RUN_ALL_TESTS();
  };
  auto output = [&] {};
  auto destroy = [&] {
    for (int i = 0; i < NB_GENERATORS; i++) {
      if (enabled_graph[i]) delete(res_by_type[i]);
    }
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);  
  return 0;
}

// Using custom function to print extra info about graph
bool same_arrays(int graph_type, int size, int * candidate, int * correct) {
  for (int i = 0; i < size; ++i) {
    if (candidate[i] != correct[i]) {
      std::cout << "On graph " << graph_by_type[graph_type] << std::endl;
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

template <class Algo>
void help_test(int graph_type, Algo algo) {
  if (!enabled_graph[graph_type]) return;
  adjlist_type& g = graph_by_type[graph_type];
  EXPECT_TRUE(same_arrays(graph_type, g.get_nb_vertices(), algo(g, src_by_type[graph_type]), res_by_type[graph_type]));
}

// SerialClassic
TEST(SerialClassic, CompleteGraph)   {help_test(COMPLETE,        bellman_ford_seq_classic<adjlist_seq_type>);}
TEST(SerialClassic, BalancedTree)    {help_test(BALANCED_TREE,   bellman_ford_seq_classic<adjlist_seq_type>);}
TEST(SerialClassic, Chain)           {help_test(CHAIN,           bellman_ford_seq_classic<adjlist_seq_type>);}
TEST(SerialClassic, SquareGrid)      {help_test(SQUARE_GRID,     bellman_ford_seq_classic<adjlist_seq_type>);}
TEST(SerialClassic, Star)            {help_test(STAR,            bellman_ford_seq_classic<adjlist_seq_type>);}
TEST(SerialClassic, RandomSparse)    {help_test(RANDOM_SPARSE,   bellman_ford_seq_classic<adjlist_seq_type>);}
TEST(SerialClassic, RandomDense)     {help_test(RANDOM_DENSE,    bellman_ford_seq_classic<adjlist_seq_type>);}
TEST(SerialClassic, RandomCustom)    {help_test(RANDOM_CUSTOM,   bellman_ford_seq_classic<adjlist_seq_type>);}

// SerialYen
TEST(SerialYen, CompleteGraph)   {help_test(COMPLETE,        bellman_ford_seq_classic_opt<adjlist_seq_type>);}
TEST(SerialYen, BalancedTree)    {help_test(BALANCED_TREE,   bellman_ford_seq_classic_opt<adjlist_seq_type>);}
TEST(SerialYen, Chain)           {help_test(CHAIN,           bellman_ford_seq_classic_opt<adjlist_seq_type>);}
TEST(SerialYen, SquareGrid)      {help_test(SQUARE_GRID,     bellman_ford_seq_classic_opt<adjlist_seq_type>);}
TEST(SerialYen, Star)            {help_test(STAR,            bellman_ford_seq_classic_opt<adjlist_seq_type>);}
TEST(SerialYen, RandomSparse)    {help_test(RANDOM_SPARSE,   bellman_ford_seq_classic_opt<adjlist_seq_type>);}
TEST(SerialYen, RandomDense)     {help_test(RANDOM_DENSE,    bellman_ford_seq_classic_opt<adjlist_seq_type>);}
TEST(SerialYen, RandomCustom)    {help_test(RANDOM_CUSTOM,   bellman_ford_seq_classic_opt<adjlist_seq_type>);}

// SerialBFS
TEST(SerialBFS, CompleteGraph)   {help_test(COMPLETE,        bellman_ford_seq_bfs<adjlist_seq_type>);}
TEST(SerialBFS, BalancedTree)    {help_test(BALANCED_TREE,   bellman_ford_seq_bfs<adjlist_seq_type>);}
TEST(SerialBFS, Chain)           {help_test(CHAIN,           bellman_ford_seq_bfs<adjlist_seq_type>);}
TEST(SerialBFS, SquareGrid)      {help_test(SQUARE_GRID,     bellman_ford_seq_bfs<adjlist_seq_type>);}
TEST(SerialBFS, Star)            {help_test(STAR,            bellman_ford_seq_bfs<adjlist_seq_type>);}
TEST(SerialBFS, RandomSparse)    {help_test(RANDOM_SPARSE,   bellman_ford_seq_bfs<adjlist_seq_type>);}
TEST(SerialBFS, RandomDense)     {help_test(RANDOM_DENSE,    bellman_ford_seq_bfs<adjlist_seq_type>);}
TEST(SerialBFS, RandomCustom)    {help_test(RANDOM_CUSTOM,   bellman_ford_seq_bfs<adjlist_seq_type>);}

// ParNumVertices
TEST(ParNumVertices, CompleteGraph)   {help_test(COMPLETE,        bellman_ford_par_vertices<adjlist_seq_type>);}
TEST(ParNumVertices, BalancedTree)    {help_test(BALANCED_TREE,   bellman_ford_par_vertices<adjlist_seq_type>);}
TEST(ParNumVertices, Chain)           {help_test(CHAIN,           bellman_ford_par_vertices<adjlist_seq_type>);}
TEST(ParNumVertices, SquareGrid)      {help_test(SQUARE_GRID,     bellman_ford_par_vertices<adjlist_seq_type>);}
TEST(ParNumVertices, Star)            {help_test(STAR,            bellman_ford_par_vertices<adjlist_seq_type>);}
TEST(ParNumVertices, RandomSparse)    {help_test(RANDOM_SPARSE,   bellman_ford_par_vertices<adjlist_seq_type>);}
TEST(ParNumVertices, RandomDense)     {help_test(RANDOM_DENSE,    bellman_ford_par_vertices<adjlist_seq_type>);}
TEST(ParNumVertices, RandomCustom)    {help_test(RANDOM_CUSTOM,   bellman_ford_par_vertices<adjlist_seq_type>);}

// ParNumEdges
TEST(ParNumEdges, CompleteGraph)   {help_test(COMPLETE,        bellman_ford_par_edges<adjlist_seq_type>);}
TEST(ParNumEdges, BalancedTree)    {help_test(BALANCED_TREE,   bellman_ford_par_edges<adjlist_seq_type>);}
TEST(ParNumEdges, Chain)           {help_test(CHAIN,           bellman_ford_par_edges<adjlist_seq_type>);}
TEST(ParNumEdges, SquareGrid)      {help_test(SQUARE_GRID,     bellman_ford_par_edges<adjlist_seq_type>);}
TEST(ParNumEdges, Star)            {help_test(STAR,            bellman_ford_par_edges<adjlist_seq_type>);}
TEST(ParNumEdges, RandomSparse)    {help_test(RANDOM_SPARSE,   bellman_ford_par_edges<adjlist_seq_type>);}
TEST(ParNumEdges, RandomDense)     {help_test(RANDOM_DENSE,    bellman_ford_par_edges<adjlist_seq_type>);}
TEST(ParNumEdges, RandomCustom)    {help_test(RANDOM_CUSTOM,   bellman_ford_par_edges<adjlist_seq_type>);}

// ParBFS
TEST(ParBFS, CompleteGraph)   {help_test(COMPLETE,        bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par_bfs);}
TEST(ParBFS, BalancedTree)    {help_test(BALANCED_TREE,   bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par_bfs);}
TEST(ParBFS, Chain)           {help_test(CHAIN,           bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par_bfs);}
TEST(ParBFS, SquareGrid)      {help_test(SQUARE_GRID,     bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par_bfs);}
TEST(ParBFS, Star)            {help_test(STAR,            bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par_bfs);}
TEST(ParBFS, RandomSparse)    {help_test(RANDOM_SPARSE,   bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par_bfs);}
TEST(ParBFS, RandomDense)     {help_test(RANDOM_DENSE,    bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par_bfs);}
TEST(ParBFS, RandomCustom)    {help_test(RANDOM_CUSTOM,   bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par_bfs);}

// ParCombined
TEST(ParCombined, CompleteGraph)   {help_test(COMPLETE,        bellman_ford_par_combined<adjlist_seq_type>);}
TEST(ParCombined, BalancedTree)    {help_test(BALANCED_TREE,   bellman_ford_par_combined<adjlist_seq_type>);}
TEST(ParCombined, Chain)           {help_test(CHAIN,           bellman_ford_par_combined<adjlist_seq_type>);}
TEST(ParCombined, SquareGrid)      {help_test(SQUARE_GRID,     bellman_ford_par_combined<adjlist_seq_type>);}
TEST(ParCombined, Star)            {help_test(STAR,            bellman_ford_par_combined<adjlist_seq_type>);}
TEST(ParCombined, RandomSparse)    {help_test(RANDOM_SPARSE,   bellman_ford_par_combined<adjlist_seq_type>);}
TEST(ParCombined, RandomDense)     {help_test(RANDOM_DENSE,    bellman_ford_par_combined<adjlist_seq_type>);}
TEST(ParCombined, RandomCustom)    {help_test(RANDOM_CUSTOM,   bellman_ford_par_combined<adjlist_seq_type>);}
