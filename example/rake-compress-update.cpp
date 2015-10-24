#include <functional>
#include "cmdline.hpp"
#include "benchmark.hpp"
#include "rake-compress-generators.hpp"
#include "rake-compress-construction-functions.hpp"
#include "rake-compress-update-functions.hpp"

int main(int argc, char** argv) {
   bool seq;
   int n;
   auto init = [&] {
     n = (long)pasl::util::cmdline::parse_or_default_int("n", 24);
     std::string graph = pasl::util::cmdline::parse_or_default_string("graph", std::string("bamboo"));
     seq = pasl::util::cmdline::parse_or_default_int("seq", 1) == 1;
     std::string type = pasl::util::cmdline::parse_or_default_string("type", std::string("add"));
     int k = pasl::util::cmdline::parse_or_default_int("k", n - 1);
     int seed = pasl::util::cmdline::parse_or_default_int("seed", 239);
     int degree = pasl::util::cmdline::parse_or_default_int("degree", 4);
     double f = pasl::util::cmdline::parse_or_default_double("fraction", 0.5);

     std::vector<int>* children = new std::vector<int>[n];
     int* parent = new int[n];
     int* add_p;
     int* add_v;
     int* delete_p;
     int* delete_v;
     int add_no, delete_no;

     std::vector<int>* tmp_children = new std::vector<int>[n];
     int* tmp_parent = new int[n];

     generate_graph(graph, n, tmp_children, tmp_parent, k, seed, degree, f);
     int* p = new int[k];
     int* v = new int[k];

     if (graph.compare("bamboo") == 0) {
       for (int i = 0; i < k; i++) {
         p[i] = (i + 1) * (n / (k + 1)) - 1;
         v[i] = p[i] + 1;
       }
     } else {
       choose_edges(n, tmp_children, tmp_parent, k, p, v, seed);
     }

     if (type.compare("add") == 0) {
       add_no = k;
       add_p = p;
       add_v = v;
       delete_no = 0;
       delete_p = new int[0];
       delete_v = new int[0];
       remove_edges(n, tmp_children, tmp_parent, children, parent, k, add_p, add_v);
       delete[] tmp_children;
       delete[] tmp_parent;
     } else {
       add_no = 0;
       add_p = new int[0];
       add_v = new int[0];
       delete_no = k;
       delete_p = p;
       delete_v = v;
       children = tmp_children;
       parent = tmp_parent;
     }
     
     initialization_construction(n, children, parent);
     construction(n, [&] (int round_no) {construction_round_seq(round_no);});

     delete [] live[0];
     delete [] live[1];

//     print_roots(n);
     if (seq) {
       initialization_update_seq(n, add_no, add_p, add_v, delete_no, delete_p, delete_v);
     } else {
       initialization_update(n, add_no, add_p, add_v, delete_no, delete_p, delete_v);
     }

     delete [] children;
     delete [] parent;
     delete [] add_p;
     delete [] add_v;
     delete [] delete_p;
     delete [] delete_v;
   };

   auto run = [&] (bool sequential) {
     if (seq) {
       std::cerr << "Sequential run" << std::endl;
//       update(n, [&] (int round_no) {update_round_seq(round_no);}, [&] () {});
       update(n, std::bind(update_round_seq, std::placeholders::_1), std::bind(end_condition_seq, std::placeholders::_1));
     } else {
       std::cerr << "Parallel run" << std::endl;
       update(n, std::bind(update_round, std::placeholders::_1), std::bind(end_condition, std::placeholders::_1));
     }
   };

   auto output = [&] {
     std::cerr << "the update has finished." << std::endl;
//     print_roots(n);
   };

   auto destroy = [&] {
#ifdef STANDART
     for (int i = 0; i < n; i++) {
       Node* start = lists[i]->head;
       while (start != NULL) {
         Node* next = start->next;
         delete start;
         start = next;
       }
     }
#endif

     if (!seq) {
       delete [] live[0];
       delete [] live[1];
     }
     delete [] lists;
     delete [] live_affected_sets;
     delete [] deleted_affected_sets;
     delete [] old_live_affected_sets;
     delete [] old_deleted_affected_sets;
     delete [] ids;
     delete [] vertex_thread;
   };

   pasl::sched::launch(argc, argv, init, run, output, destroy);

   return 0;
}