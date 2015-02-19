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
int generate(generator_type& which_generator, size_t _tgt_nb_edges, adjlist<Adjlist_seq>& graph) {
    using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
    using edge_type = wedge<vtxid_type>;
    using edgelist_bag_type = pasl::data::array_seq<edge_type>;
    using edgelist_type = edgelist<edgelist_bag_type>;
    edgelist_type edg;
    generate(_tgt_nb_edges, which_generator, edg);
    std::vector<int> map_vector;
    for (int i = 0; i < edg.nb_vertices; ++i) map_vector.push_back(i);
    std::random_shuffle ( map_vector.begin(), map_vector.end() );
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
        enabled_mask[COMPLETE]      = false;
        enabled_mask[BALANCED_TREE] = false;
        enabled_mask[CHAIN]         = false;
        enabled_mask[STAR]          = false;
        enabled_mask[SQUARE_GRID]   = false;
        enabled_mask[RANDOM_SPARSE] = true;
        enabled_mask[RANDOM_DENSE]  = true;
        

        std::cout << "Generating graphs..." << std::endl;

        for (int i = 0; i < NB_GENERATORS; ++i) {
            if (!enabled_mask[i]) continue;
            generator_type which_generator;
            which_generator.ty = i;
            graph_by_type[i] = adjlist_type();
            src_by_type[i] = generate(which_generator, 7000, graph_by_type[i]);
//            std::cout << graph_by_type[i] << std::endl;
            res_by_type[i] = bellman_ford_seq(graph_by_type[i], (int) src_by_type[i]);
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

//// Seq1
//TEST(Seq1, CompleteGraph)   {help_test(COMPLETE,        bellman_ford_seq<adjlist_seq_type>);}
//TEST(Seq1, BalancedTree)    {help_test(BALANCED_TREE,   bellman_ford_seq<adjlist_seq_type>);}
//TEST(Seq1, Chain)           {help_test(CHAIN,           bellman_ford_seq<adjlist_seq_type>);}
//TEST(Seq1, SquareGrid)      {help_test(SQUARE_GRID,     bellman_ford_seq<adjlist_seq_type>);}
//TEST(Seq1, Star)            {help_test(STAR,            bellman_ford_seq<adjlist_seq_type>);}
//TEST(Seq1, RandomSparse)    {help_test(RANDOM_SPARSE,   bellman_ford_seq<adjlist_seq_type>);}
//TEST(Seq1, RandomDense)     {help_test(RANDOM_DENSE,    bellman_ford_seq<adjlist_seq_type>);}
//
//// Par1
//TEST(Par1, CompleteGraph)   {help_test(COMPLETE,        bellman_ford_par1<adjlist_seq_type>);}
//TEST(Par1, BalancedTree)    {help_test(BALANCED_TREE,   bellman_ford_par1<adjlist_seq_type>);}
//TEST(Par1, Chain)           {help_test(CHAIN,           bellman_ford_par1<adjlist_seq_type>);}
//TEST(Par1, SquareGrid)      {help_test(SQUARE_GRID,     bellman_ford_par1<adjlist_seq_type>);}
//TEST(Par1, Star)            {help_test(STAR,            bellman_ford_par1<adjlist_seq_type>);}
//TEST(Par1, RandomSparse)    {help_test(RANDOM_SPARSE,   bellman_ford_par1<adjlist_seq_type>);}
//TEST(Par1, RandomDense)     {help_test(RANDOM_DENSE,    bellman_ford_par1<adjlist_seq_type>);}
//
//// Par2
//TEST(Par2, CompleteGraph)   {help_test(COMPLETE,        bellman_ford_par2<adjlist_seq_type>);}
//TEST(Par2, BalancedTree)    {help_test(BALANCED_TREE,   bellman_ford_par2<adjlist_seq_type>);}
//TEST(Par2, Chain)           {help_test(CHAIN,           bellman_ford_par2<adjlist_seq_type>);}
//TEST(Par2, SquareGrid)      {help_test(SQUARE_GRID,     bellman_ford_par2<adjlist_seq_type>);}
//TEST(Par2, Star)            {help_test(STAR,            bellman_ford_par2<adjlist_seq_type>);}
//TEST(Par2, RandomSparse)    {help_test(RANDOM_SPARSE,   bellman_ford_par2<adjlist_seq_type>);}
//TEST(Par2, RandomDense)     {help_test(RANDOM_DENSE,    bellman_ford_par2<adjlist_seq_type>);}

// Par4
TEST(Par4, CompleteGraph)   {help_test(COMPLETE,        bellman_ford_par4<adjlist_seq_type>);}
TEST(Par4, BalancedTree)    {help_test(BALANCED_TREE,   bellman_ford_par4<adjlist_seq_type>);}
TEST(Par4, Chain)           {help_test(CHAIN,           bellman_ford_par4<adjlist_seq_type>);}
TEST(Par4, SquareGrid)      {help_test(SQUARE_GRID,     bellman_ford_par4<adjlist_seq_type>);}
TEST(Par4, Star)            {help_test(STAR,            bellman_ford_par4<adjlist_seq_type>);}
TEST(Par4, RandomSparse)    {help_test(RANDOM_SPARSE,   bellman_ford_par4<adjlist_seq_type>);}
TEST(Par4, RandomDense)     {help_test(RANDOM_DENSE,    bellman_ford_par4<adjlist_seq_type>);}

