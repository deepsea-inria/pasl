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

     std::vector<int>* children = new std::vector<int>[n];
     int* parent = new int[n];
     int* add_p;
     int* add_v;
     int* delete_p;
     int* delete_v;
     int add_no, delete_no;

     if (type.compare("add") == 0) { 
       generate_graph("empty_graph", n, children, parent);
 
       add_no = n - 1;
       delete_no = 0;
       add_p = new int[n - 1];
       add_v = new int[n - 1];
       delete_p = new int[0];
       delete_v = new int[0];
       if (graph.compare("binary_tree") == 0) {
         // let us firstly think about binary tree
         int id = 0;
         for (int i = 0; i < n; i++) {
           if (2 * i + 1 < n) {
             add_p[id] = i;
             add_v[id] = 2 * i + 1;
             id++;
           }
           if (2 * i + 2 < n) {
             add_p[id] = i;
             add_v[id] = 2 * i + 2;
             id++;
           }
         }
       } else {
         // bambooooooo
         for (int i = 0; i < n - 1; i++) {
           add_p[i] = i;
           add_v[i] = i + 1;
         }
       }
     } else {
       generate_graph(graph, n, children, parent);

       add_no = 0;
       delete_no = n - 1;
       add_p = new int[0];
       add_v = new int[0];
       delete_p = new int[n - 1];
       delete_v = new int[n - 1];
       if (graph.compare("binary_tree") == 0) {
         // let us firstly think about binary tree
         int id = 0;
         for (int i = 0; i < n; i++) {
           if (2 * i + 1 < n) {
             delete_p[id] = i;
             delete_v[id] = 2 * i + 1;
             id++;
           }
           if (2 * i + 2 < n) {
             delete_p[id] = i;
             delete_v[id] = 2 * i + 2;
             id++;
           }
         }
       } else {
         // bambooooooo
         for (int i = 0; i < n - 1; i++) {
           delete_p[i] = i;
           delete_v[i] = i + 1;
         }
       }
     }
  
     initialization_construction(n, children, parent);
     construction(n, [&] (int round_no) {construction_round_seq(round_no);});
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
     delete [] live[0];
     delete [] live[1];
   };

   auto run = [&] (bool sequential) {
     if (seq) {
       std::cerr << "Sequential run" << std::endl;
//       update(n, [&] (int round_no) {update_round_seq(round_no);}, [&] () {});
       update(n, std::bind(update_round_seq, std::placeholders::_1), std::bind(end_condition_seq));
     } else {
       std::cerr << "Parallel run" << std::endl;
       update(n, std::bind(update_round, std::placeholders::_1), std::bind(end_condition));
     }
   };

   auto output = [&] {
     std::cout << "the update has finished." << std::endl;
     print_roots(n);
   };

   auto destroy = [&] {
     for (int i = 0; i < n; i++) {
       Node* start = lists[i]->head;
       while (start != NULL) {
         Node* next = start->next;
         delete start;
         start = next;
       }
     }

     delete [] lists;
   };

   pasl::sched::launch(argc, argv, init, run, output, destroy);

   return 0;
}                