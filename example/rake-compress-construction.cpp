#include "cmdline.hpp"
#include "benchmark.hpp"
#include "rake-compress-generators.hpp"
#include "rake-compress-construction-functions.hpp"
#include "static-contract-functions.hpp"

int main(int argc, char** argv) {
   int seq;
   int n;
   contraction::hash::Forest* forest;
   auto init = [&] {
     n = (long)pasl::util::cmdline::parse_or_default_int("n", 24);
     std::string graph = pasl::util::cmdline::parse_or_default_string("graph", std::string("bamboo"));
     seq = pasl::util::cmdline::parse_or_default_int("seq", 1);
     int k = pasl::util::cmdline::parse_or_default_int("k", 1);
     int seed = pasl::util::cmdline::parse_or_default_int("seed", 239);
     int degree = pasl::util::cmdline::parse_or_default_int("degree", 4);
     double f = pasl::util::cmdline::parse_or_default_double("fraction", 0.5);

     std::vector<int>* children = new std::vector<int>[n];
     int* parent = new int[n];
     generate_graph(graph, n, children, parent, k, seed, degree, f);
     if (seq < 2) {
       initialization_construction(n, children, parent);
     } else {
       forest = contraction::hash::initialization_forest(n, children, parent);
     }
     delete [] children;
     delete [] parent;
   };

   auto run = [&] (bool sequential) {
     if (seq == 1) {
       std::cerr << "Sequential linked lists run" << std::endl;
       construction(n, [&] (int round_no) {construction_round_seq(round_no);});
     } else if (seq == 0) {
       std::cerr << "Parallel linked lists run" << std::endl;
       construction(n, [&] (int round_no) {construction_round(round_no);});
     } else {
       std::cerr << "Hashmap run" << std::endl;
       contraction::hash::forest_contract(forest);
     }
   };

   auto output = [&] {
     if (seq < 2) {
       print_roots(n);
     }
     std::cout << "the construction has finished." << std::endl;
   };

   auto destroy = [&] {
     if (seq < 2) {
       delete [] live[0];
       delete [] live[1];

       for (int i = 0; i < n; i++) {
         Node* start = lists[i]->head;
         while (start != NULL) {
           Node* next = start->next;
           delete start;
           start = next;
         }
       }

       delete [] lists;
     }
   };

   pasl::sched::launch(argc, argv, init, run, output, destroy);

   return 0;
}                