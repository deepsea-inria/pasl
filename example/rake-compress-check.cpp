#include <functional>
#include "cmdline.hpp"
#include "benchmark.hpp"
#include "rake-compress-generators.hpp"
#include "rake-compress-construction-functions.hpp"
#include "rake-compress-update-functions.hpp"

struct GraphNode {
  GraphNode* next;
  std::set<int> children;
  int parent;
  bool root;
  bool contracted;

  GraphNode(State& state) {
    parent = state.parent->get_vertex();
    for (Node* child : state.children) {
      children.insert(child->get_vertex());
    }
    root = state.root;
    contracted = state.contracted;
    next = NULL;
  }

  bool is_equal(GraphNode* node) {
    return node->parent == parent && node->children == children && node->root == root && node->contracted == contracted;
  }

  void print() {
    std::cout << root << " " << contracted << std::endl;
    std::cout << "Parent: " << parent << std::endl;
    std:: cout << "Children: ";
    for (int c : children) 
      std::cout << c << " ";
    std::cout << std::endl;
  }
};

void print_all(int n, GraphNode** a) {
  GraphNode** b = new GraphNode*[n];
  for (int i = 0; i < n; i++) {
    b[i] = a[i];
  }
  bool ok = true;
  int round = 1;
  while (ok) {
    ok = false;
    std::cout << "Round: " << round << std::endl;
    for (int i = 0; i < n; i++) {
      if (b[i] != NULL) {
        std::cout << i << " ";
        b[i]->print();
        ok = true;
        b[i] = b[i]->next;
      }
    }
    std::cout << "============" << std::endl;
    round++;
  }
}

GraphNode** copy_nodes(int n) {
  Node** nd = new Node*[n];
  for (int i = 0; i < n; i++) {
    nd[i] = lists[i]->head;
  }

  GraphNode** to_return = new GraphNode*[n];
  GraphNode** graph = new GraphNode*[n];
  for (int i = 0; i < n; i++) {
    graph[i] = NULL;
  }
  int round = 0;
  bool ok = true;
  while (ok) {
    ok = false;
    for (int i = 0; i < n; i++) {
      if (nd[i] != NULL) {
        GraphNode* last = graph[i];
        graph[i] = new GraphNode(nd[i]->state);
        if (last != NULL) {
          last->next = graph[i];
        } else {
          to_return[i] = graph[i];
        }
        ok = true;
        nd[i] = nd[i]->next;
      }
    }
    round++;
  }

  delete [] nd;

  return to_return;
}

bool is_equal(int n, GraphNode** a, GraphNode** b) {
  GraphNode** ta = new GraphNode*[n];
  GraphNode** tb = new GraphNode*[n];
  for (int i = 0; i < n; i++) {
    ta[i] = a[i];
    tb[i] = b[i];
  }

  int round = 0;
  bool ok = true;
  bool res = true;
  while (ok && res) {
    ok = false;
    for (int i = 0; i < n; i++) {
      if (ta[i] != NULL) {
        if (tb[i] == NULL || !ta[i]->is_equal(tb[i])) {
          std::cerr << "Not equal for the vertex " << i << " on round " << round << std::endl;
          ta[i]->print();
          tb[i]->print();
          res = false;
          break;
        }
        ta[i] = ta[i]->next;
        tb[i] = tb[i]->next;
        ok = true;
      } else {
        if (tb[i] != NULL) {
          std::cerr << "Not equal for the vertex " << i << " on round " << round << std::endl;
          if (ta[i] == NULL)
            std::cout << "NULL" << std::endl;
          else
            ta[i]->print();
          tb[i]->print();
          res = false;
          break;
        }
      }
    }
    round++;
  }

  delete [] ta;
  delete [] tb;

  return res;
}

void destroy(int n, GraphNode** a){
  for (int i = 0; i < n; i++) {
    GraphNode* start = a[i];
    while (start != NULL) {
      GraphNode* next = start->next;
      delete start;
      start = next;
    }
  }
  delete [] a;
}

void destroy(int n) {
  for (int i = 0; i < n; i++) {
    Node* start = lists[i]->head;
    while (start != NULL) {
      Node* next = start->next;
      delete start;
      start = next;
    }
  }

  delete [] live[0];
  delete [] live[1];
  delete [] lists;
  delete [] live_affected_sets;
  delete [] deleted_affected_sets;
  delete [] old_live_affected_sets;
  delete [] old_deleted_affected_sets;
  delete [] ids;
  delete [] vertex_thread;
}

int test = 1;
bool seq;

void test_one_way(int n, std::string type, int m, int* p, int* v, bool add, int k = 2, int seed = 239, int degree = 4, double f = 0.5) {
  std::cout << "Passing test " << test << std::endl;  
  std::vector<int>* first_children = new std::vector<int>[n];
  int* first_parent = new int[n];
  std::vector<int>* second_children = new std::vector<int>[n];
  int* second_parent = new int[n];

  if (add) {
    generate_graph(type, n, first_children, first_parent, k, seed, degree, f);
    if (p == NULL || v == NULL) {
      p = new int[m];
      v = new int[m];
      choose_edges(n, first_children, first_parent, m, p, v, seed);
    }
    remove_edges(n, first_children, first_parent, second_children, second_parent, m, p, v);
  } else {
    generate_graph(type, n, second_children, second_parent, k, seed, degree, f);
    if (p == NULL || v == NULL) {
      p = new int[m];
      v = new int[m];
      choose_edges(n, second_children, second_parent, m, p, v, seed);
    }
    remove_edges(n, second_children, second_parent, first_children, first_parent, m, p, v);
  }

  initialization_construction(n, first_children, first_parent);
  construction(n, [&] (int round_no) {construction_round_seq(round_no);});

  GraphNode** first = copy_nodes(n);

  int add_no, delete_no;
  int* add_p;
  int* add_v;
  int* delete_p;
  int* delete_v;

  if (add) {
    add_no = m;
    add_p = p;
    add_v = v;
    delete_no = 0;
    delete_p = new int[0];
    delete_v = new int[0];
  } else {
    add_no = 0;
    add_p = new int[0];
    add_v = new int[0];
    delete_no = m;
    delete_p = p;
    delete_v = v;
  }

  initialization_construction(n, second_children, second_parent);
  construction(n, [&] (int round_no) {construction_round_seq(round_no);});

  GraphNode** previous = copy_nodes(n);

  if (seq) {
    initialization_update_seq(n, add_no, add_p, add_v, delete_no, delete_p, delete_v);
    update(n, std::bind(update_round_seq, std::placeholders::_1), std::bind(end_condition_seq));
  } else {
    initialization_update(n, add_no, add_p, add_v, delete_no, delete_p, delete_v);
//    initialization_update_seq(n, add_no, add_p, add_v, delete_no, delete_p, delete_v);
    update(n, std::bind(update_round, std::placeholders::_1), std::bind(end_condition));
  }

  GraphNode** second = copy_nodes(n);
/*
  std::cout << "========================================================" << std::endl;
  print_all(n, first);
  std::cout << "========================================================" << std::endl;
  print_all(n, second);
  std::cout << "========================================================" << std::endl;
  print_all(n, previous);
*/
  if (is_equal(n, first, second)) {
    std::cout << "test " << test << " passed" << std::endl;
  } else {
    std::cout << "test " << test << " failed" << std::endl;
    exit(1);
  }
  test++;
  destroy(n);
  destroy(n, first);
  destroy(n, second);
  delete [] first_children;
  delete [] first_parent;
  delete [] second_children;
  delete [] second_parent;
  delete [] add_p;
  delete [] add_v;
  delete [] delete_p;
  delete [] delete_v;
}

void binary_tree_edges(int n, int* p, int* v) {
  int id = 0;
  for (int i = 0; i < n; i++) {
    if (2 * i + 1 < n) {
      p[id] = i;
      v[id] = 2 * i + 1;
      id++;
    }
    if (2 * i + 2 < n) {
      p[id] = i;
      v[id] = 2 * i + 2;
      id++;
    }
  }
}

void test1(int n) {
  int* p = new int[n];
  int* v = new int[n];
  binary_tree_edges(n, p, v);
  test_one_way(n, "binary_tree", n - 1, p, v, true);
}

void test2(int n) {
  int* p = new int[n];
  int* v = new int[n];
  binary_tree_edges(n, p, v);
  test_one_way(n, "binary_tree", n - 1, p, v, false);
}

void bamboo_edges(int n, int* p, int* v) {
  for (int i = 0; i < n - 1; i++) {
    p[i] = i;
    v[i] = i + 1;
  }
}

void test3(int n) {
  int* p = new int[n];
  int* v = new int[n];
  bamboo_edges(n, p, v);
  test_one_way(n, "bamboo", n - 1, p, v, true);
}

void test4(int n) {
  int* p = new int[n];
  int* v = new int[n];
  bamboo_edges(n, p, v);
  test_one_way(n, "bamboo", n - 1, p, v, false);
}

void test5(int n) {
  int* p = new int[n];
  int* v = new int[n];
  p[0] = n / 2 - 1;
  v[0] = n / 2;
  test_one_way(n, "bamboo", 1, p, v, true);
}

void test6(int n) {
  int* p = new int[n];
  int* v = new int[n];
  p[0] = n / 2 - 1;
  v[0] = n / 2;
  test_one_way(n, "bamboo", 1, p, v, false);
}

void test7(int n, int k) {
  int* p = new int[n];
  int* v = new int[n];
  for (int i = 0; i < k - 1; i++) {
    p[i] = (i + 1) * (n / k) - 1;
    v[i] = (i + 1) * (n / k);
  }
  test_one_way(n, "bamboo", k - 1, p, v, true, k);
}

void test8(int n, int k) {
  int* p = new int[n];
  int* v = new int[n];
  for (int i = 0; i < k - 1; i++) {
    p[i] = (i + 1) * (n / k) - 1;
    v[i] = (i + 1) * (n / k);
  }
  test_one_way(n, "bamboo", k - 1, p, v, false, k);
}

void test9(int n, int k, int seed, int degree, double fraction) {
  test_one_way(n, "random_graph", k, NULL, NULL, true, 0, seed, degree, fraction);
}

void test10(int n, int k, int seed, int degree, double fraction) {
  test_one_way(n, "random_graph", k, NULL, NULL, false, 0, seed, degree, fraction);
}

int main(int argc, char** argv) {
   seq = false;
   bool inf;
   int max_n = 0;
/*   test1(10000);
   test2(10000);
   test3(10000);
   test4(10000);
   test5(10000);
   test6(10000);
   test7(10000, 10);
   test8(10000, 10);*/

   auto init = [&] {
     seq = pasl::util::cmdline::parse_or_default_int("seq", 1) == 1;
     max_n = pasl::util::cmdline::parse_or_default_int("n", 100000);
     int seed = pasl::util::cmdline::parse_or_default_int("seed", 239);
     srand(seed);
     inf = pasl::util::cmdline::parse_or_default_int("inf", 0) == 1;
   };
//   seq = false;
   auto run = [&] (bool sequential) {
     if (inf) {
       while (true) {
         test1(rand() % max_n + 2);
         test2(rand() % max_n + 2);
         test3(rand() % max_n + 2);
         test4(rand() % max_n + 2);
         test5(rand() % max_n + 2);
         test6(rand() % max_n + 2);
         test7(rand() % max_n + 10, 10);
         test8(rand() % max_n + 10, 10);
         {
           int n = rand() % max_n + 10;
           int k = rand() % (n / 2);
           test9(n, k, rand(), rand() % 4 + 2, 1. * (rand() % 100) / 100);
         }
         {
           int n = rand() % max_n + 10;
           int k = rand() % (n / 2);
           test10(n, k, rand(), rand() % 4 + 2, 1. * (rand() % 100) / 100);
         }
       }
     } else {
       for (int i = 0; i < 10; i++)
         test1(rand() % max_n + 2);
       for (int i = 0; i < 10; i++)
         test2(rand() % max_n + 2);
       for (int i = 0; i < 10; i++)
         test3(rand() % max_n + 2);
       for (int i = 0; i < 10; i++)
         test4(rand() % max_n + 2);
       for (int i = 0; i < 10; i++)
        test5(rand() % max_n + 2);
       for (int i = 0; i < 10; i++)
         test6(rand() % max_n + 2);
       for (int i = 0; i < 10; i++)
         test7(rand() % max_n + 10, 10);
       for (int i = 0; i < 10; i++)
         test8(rand() % max_n + 10, 10);
       for (int i = 0; i < 10; i++) {
         int n = rand() % max_n + 10;
         int k = rand() % (n / 2);
         test9(n, k, rand(), rand() % 4 + 2, 1. * (rand() % 100) / 100);
       }

       for (int i = 0; i < 10; i++) {
         int n = rand() % max_n + 10;
         int k = rand() % (n / 2);
         test10(n, k, rand(), rand() % 4 + 2, 1. * (rand() % 100) / 100);
       }
     }
   };

   auto output = [&] {};

   auto destroy = [&] {};

   pasl::sched::launch(argc, argv, init, run, output, destroy);
   return 0;
}                