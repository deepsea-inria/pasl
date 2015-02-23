#include <fstream>

#include "graphgenerators.hpp"
#include "graphconversions.hpp"
#include "ls_bag.hpp"
#include "frontierseg.hpp"
#include "bellman_ford.hpp"
#include "benchmark.hpp"
#include <gtest/gtest.h>
#include "edgelist.hpp"
#include "adjlist.hpp"
#include <map>

using namespace pasl::graph;
using namespace pasl::data;

template <class Adjlist_seq>
int generate(generator_type& which_generator, size_t _tgt_nb_edges, adjlist<Adjlist_seq>& graph,
             double fraction = -1,
             double avg_degree = -1,
             bool need_shuffle = true) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  using edge_type = wedge<vtxid_type>;
  using edgelist_bag_type = pasl::data::array_seq<edge_type>;
  using edgelist_type = edgelist<edgelist_bag_type>;
  edgelist_type edg;
  generate(_tgt_nb_edges, which_generator, edg, fraction, avg_degree);
  std::vector<int> map_vector;
  for (int i = 0; i < edg.nb_vertices; ++i) map_vector.push_back(i);
  if (fraction == -1 && need_shuffle) std::random_shuffle ( map_vector.begin(), map_vector.end() );
  for (edgeid_type i = 0; i < edg.edges.size(); i++) {
    edg.edges[i].dst = map_vector[edg.edges[i].dst];
    edg.edges[i].src = map_vector[edg.edges[i].src];
  }
  adjlist_from_edgelist(edg, graph);
  return map_vector[0];
}

using vtxid_type = long;
using adjlist_seq_type = pasl::graph::flat_adjlist_seq<vtxid_type>;
using adjlist_type = adjlist<adjlist_seq_type>;
int * res_by_type[30];
bool enabled_mask[30];
adjlist_type graph_by_type[30];
vtxid_type   src_by_type[30];


int main(int argc, char ** argv) {
  
  auto init = [&] {
    for (int i = 0; i < NB_GENERATORS; ++i) {
      enabled_mask[i] = false;
    }
    enabled_mask[COMPLETE]      = true;
    enabled_mask[BALANCED_TREE] = true;
    enabled_mask[CHAIN]         = true;
    enabled_mask[STAR]          = true;
    enabled_mask[SQUARE_GRID]   = true;
    enabled_mask[RANDOM_SPARSE] = true;
    enabled_mask[RANDOM_DENSE]  = true;
    enabled_mask[RANDOM_CUSTOM] = false;
    
    
    std::cout << "Generating graphs..." << std::endl;
    
    for (int i = 0; i < NB_GENERATORS; ++i) {
      if (!enabled_mask[i]) continue;
      generator_type which_generator;
      which_generator.ty = i;
      graph_by_type[i] = adjlist_type();
      if (i == RANDOM_CUSTOM) {
        src_by_type[i] = generate(which_generator, 5000000, graph_by_type[i], 0.5, 20);
      } else {
        src_by_type[i] = generate(which_generator, 1000, graph_by_type[i]);
      }
      std::cout << "Done generating " << graph_types[i] << " with ";
      
      vtxid_type nb_vertices = graph_by_type[i].get_nb_vertices();
      int num_less = 0, num_edges = graph_by_type[i].nb_edges;
      for (size_t from = 0; from < nb_vertices; from++) {
        vtxid_type degree = graph_by_type[i].adjlists[from].get_out_degree();
        for (vtxid_type edge = 0; edge < degree; edge++) {
          vtxid_type to = graph_by_type[i].adjlists[from].get_out_neighbor(edge);
          if (from < to) num_less++;
        }
      }
      std::cout << "Fraction = " << (.0 + num_less) / num_edges << " ";
      std::cout << "AvgDegree = " << (.0 + num_edges) / nb_vertices << " ";
      
      res_by_type[i] = bellman_ford_seq_classic(graph_by_type[i], (int) src_by_type[i]);
    }
    std::cout << "DONE" << std::endl;
  };
  auto run = [&] (bool sequential) {
    std::string str ("");
    bool is_first = true;
    for (int i = 0; i < NB_GENERATORS; ++i) {
      if (enabled_mask[i]) {
        if (!is_first) str += ":";
        is_first = false;
        str += "*" + graph_types[i] + "*";
      }
    }
    int size = 0;
    while (argv[argc - 1][size] != 0) size++;
    char * cstr = new char [str.length() + size + 1];
    std::strcpy (cstr, argv[argc - 1]);
    std::strcpy (cstr + size, str.c_str());
    argv[argc - 1] = cstr;
    testing::InitGoogleTest(&argc, argv);
    RUN_ALL_TESTS();
  };
  auto output = [&] {};
  auto destroy = [&] {
    for (int i = 0; i < NB_GENERATORS; i++) {
      if (enabled_mask[i]) delete(res_by_type[i]);
    }
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
  
  return 0;
}


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
  if (!enabled_mask[graph_type]) return;
  adjlist_type& g = graph_by_type[graph_type];
  EXPECT_TRUE(same_arrays(graph_type, g.get_nb_vertices(), algo(g, src_by_type[graph_type]), res_by_type[graph_type]));
}

// SeqClassic
TEST(SeqClassic, CompleteGraph)   {help_test(COMPLETE,        bellman_ford_seq_classic<adjlist_seq_type>);}
TEST(SeqClassic, BalancedTree)    {help_test(BALANCED_TREE,   bellman_ford_seq_classic<adjlist_seq_type>);}
TEST(SeqClassic, Chain)           {help_test(CHAIN,           bellman_ford_seq_classic<adjlist_seq_type>);}
TEST(SeqClassic, SquareGrid)      {help_test(SQUARE_GRID,     bellman_ford_seq_classic<adjlist_seq_type>);}
TEST(SeqClassic, Star)            {help_test(STAR,            bellman_ford_seq_classic<adjlist_seq_type>);}
TEST(SeqClassic, RandomSparse)    {help_test(RANDOM_SPARSE,   bellman_ford_seq_classic<adjlist_seq_type>);}
TEST(SeqClassic, RandomDense)     {help_test(RANDOM_DENSE,    bellman_ford_seq_classic<adjlist_seq_type>);}
TEST(SeqClassic, RandomCustom)    {help_test(RANDOM_CUSTOM,   bellman_ford_seq_classic<adjlist_seq_type>);}
//
//// SeqBfs
//TEST(SeqBfs, CompleteGraph)   {help_test(COMPLETE,        bellman_ford_seq_bfs<adjlist_seq_type>);}
//TEST(SeqBfs, BalancedTree)    {help_test(BALANCED_TREE,   bellman_ford_seq_bfs<adjlist_seq_type>);}
//TEST(SeqBfs, Chain)           {help_test(CHAIN,           bellman_ford_seq_bfs<adjlist_seq_type>);}
//TEST(SeqBfs, SquareGrid)      {help_test(SQUARE_GRID,     bellman_ford_seq_bfs<adjlist_seq_type>);}
//TEST(SeqBfs, Star)            {help_test(STAR,            bellman_ford_seq_bfs<adjlist_seq_type>);}
//TEST(SeqBfs, RandomSparse)    {help_test(RANDOM_SPARSE,   bellman_ford_seq_bfs<adjlist_seq_type>);}
//TEST(SeqBfs, RandomDense)     {help_test(RANDOM_DENSE,    bellman_ford_seq_bfs<adjlist_seq_type>);}
//TEST(SeqBfs, RandomCustom)    {help_test(RANDOM_CUSTOM,   bellman_ford_seq_bfs<adjlist_seq_type>);}

//// Par1
//TEST(Par1, CompleteGraph)   {help_test(COMPLETE,        bellman_ford_par1<adjlist_seq_type>);}
//TEST(Par1, BalancedTree)    {help_test(BALANCED_TREE,   bellman_ford_par1<adjlist_seq_type>);}
//TEST(Par1, Chain)           {help_test(CHAIN,           bellman_ford_par1<adjlist_seq_type>);}
//TEST(Par1, SquareGrid)      {help_test(SQUARE_GRID,     bellman_ford_par1<adjlist_seq_type>);}
//TEST(Par1, Star)            {help_test(STAR,            bellman_ford_par1<adjlist_seq_type>);}
//TEST(Par1, RandomSparse)    {help_test(RANDOM_SPARSE,   bellman_ford_par1<adjlist_seq_type>);}
//TEST(Par1, RandomDense)     {help_test(RANDOM_DENSE,    bellman_ford_par1<adjlist_seq_type>);}
//TEST(Par1, RandomCustom)    {help_test(RANDOM_CUSTOM,   bellman_ford_par1<adjlist_seq_type>);}

// Par2
TEST(Par2, CompleteGraph)   {help_test(COMPLETE,        bellman_ford_par2<adjlist_seq_type>);}
TEST(Par2, BalancedTree)    {help_test(BALANCED_TREE,   bellman_ford_par2<adjlist_seq_type>);}
TEST(Par2, Chain)           {help_test(CHAIN,           bellman_ford_par2<adjlist_seq_type>);}
TEST(Par2, SquareGrid)      {help_test(SQUARE_GRID,     bellman_ford_par2<adjlist_seq_type>);}
TEST(Par2, Star)            {help_test(STAR,            bellman_ford_par2<adjlist_seq_type>);}
TEST(Par2, RandomSparse)    {help_test(RANDOM_SPARSE,   bellman_ford_par2<adjlist_seq_type>);}
TEST(Par2, RandomDense)     {help_test(RANDOM_DENSE,    bellman_ford_par2<adjlist_seq_type>);}
TEST(Par2, RandomCustom)    {help_test(RANDOM_CUSTOM,   bellman_ford_par2<adjlist_seq_type>);}

// Par4
TEST(Par4, CompleteGraph)   {help_test(COMPLETE,        bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par4);}
TEST(Par4, BalancedTree)    {help_test(BALANCED_TREE,   bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par4);}
TEST(Par4, Chain)           {help_test(CHAIN,           bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par4);}
TEST(Par4, SquareGrid)      {help_test(SQUARE_GRID,     bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par4);}
TEST(Par4, Star)            {help_test(STAR,            bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par4);}
TEST(Par4, RandomSparse)    {help_test(RANDOM_SPARSE,   bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par4);}
TEST(Par4, RandomDense)     {help_test(RANDOM_DENSE,    bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par4);}
TEST(Par4, RandomCustom)    {help_test(RANDOM_CUSTOM,   bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par4);}

