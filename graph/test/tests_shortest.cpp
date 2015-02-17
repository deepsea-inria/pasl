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
void generate(generator_type& which_generator, size_t _tgt_nb_edges, adjlist<Adjlist_seq>& graph) {
    using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
    using edge_type = wedge<vtxid_type>;
    using edgelist_bag_type = pasl::data::array_seq<edge_type>;
    using edgelist_type = edgelist<edgelist_bag_type>;
    edgelist_type edg;
    generate(_tgt_nb_edges, which_generator, edg);
    adjlist_from_edgelist(edg, graph);
}

using vtxid_type = long;
using adjlist_seq_type = pasl::graph::flat_adjlist_seq<vtxid_type>;
using adjlist_type = adjlist<adjlist_seq_type>;
int* res_by_type[30];
adjlist_type graph_by_type[30];

int main(int argc, char ** argv) {
    
    auto init = [&] {
        std::cout << "Generating graphs..." << std::endl;
        for (int i = 0; i < NB_GENERATORS; ++i) {
            generator_type which_generator;
            which_generator.ty = i;
            graph_by_type[i] = adjlist_type();
            generate(which_generator, 5000, graph_by_type[i]);
            res_by_type[i] = bellman_ford_seq(graph_by_type[i], 0);
        }
        std::cout << "DONE" << std::endl;
    };
    auto run = [&] (bool sequential) {
        testing::InitGoogleTest(&argc, argv);
        RUN_ALL_TESTS();
    };
    auto output = [&] {};
    auto destroy = [&] {
        for (int i = 0; i < NB_GENERATORS; i++) {
            delete(res_by_type[i]);
        }
    };
    pasl::sched::launch(argc, argv, init, run, output, destroy);

    return 0;
}


bool same_arrays(int size, int * candidate, int * correct) {
    for (int i = 0; i < size; ++i) {
        if (candidate[i] != correct[i]) {
            std::cout << "Actual ";
            for (int j = 0; j < size; ++j) std::cout << candidate[j] << " ";
            std::cout << std::endl;
            std::cout << "Expected ";
            for (int j = 0; j < size; ++j) std::cout << correct[j] << " ";
            std::cout << std::endl;
            return false;
        }
    }
    return true;
}

void help_seq(int graph_type) {
    adjlist_type& g = graph_by_type[graph_type];
    EXPECT_TRUE(same_arrays(g.get_nb_vertices(), bellman_ford_seq(g, 0), res_by_type[graph_type]));
}

TEST(Seq1, CompleteGraph) {help_seq(COMPLETE);}
TEST(Seq1, BalancedTree) {help_seq(BALANCED_TREE);}
TEST(Seq1, Chain) {help_seq(CHAIN);}
TEST(Seq1, SquareGrid) {help_seq(SQUARE_GRID);}
TEST(Seq1, Star) {help_seq(STAR);}

void help_par1(int graph_type) {
    adjlist_type& g = graph_by_type[graph_type];
    EXPECT_TRUE(same_arrays(g.get_nb_vertices(), bellman_ford_par1(g, 0), res_by_type[graph_type]));
}
TEST(Par1, CompleteGraph) {help_par1(COMPLETE);}
TEST(Par1, BalancedTree) {help_par1(BALANCED_TREE);}
TEST(Par1, Chain) {help_par1(CHAIN);}
TEST(Par1, SquareGrid) {help_par1(SQUARE_GRID);}
TEST(Par1, Star) {help_par1(STAR);}



