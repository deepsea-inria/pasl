#include <iostream>
#include <stdio.h>
#include <set>
#include <vector>
//#include "sparray.hpp"
#include "sequence.hpp"
#include "hash.hpp"
#include "cmdline.hpp"
#include "benchmark.hpp"

void print_array(int* a, int n) {
  for (int i = 0; i < n; i++) {
    std::cerr << a[i] << " ";
  }
  std::cerr << std::endl;
}

struct Node;

struct State {
  int vertex;
  std::set<Node*> children;
  Node* parent;

  State(int _vertex) : vertex(_vertex), children() {}

  State(const State &state) : children(state.children) {
    vertex = state.vertex;
//    children = state.children;
    parent = state.parent;
  }

  ~State() {}
};

struct Node {
  Node* head;
  Node* next;
  State state;

  Node(int _vertex) : state(_vertex) { }

  Node(const Node &node) : state(node.state) {
    head = node.head;
  }

  void add_child(Node* child) {
    state.children.insert(child);
  }

  void remove_child(Node* child) {
    state.children.erase(child);
  }

  void replace_child(Node* child1, Node* child2) {
    remove_child(child1);
    add_child(child2);
  }

  int degree() {
    return state.children.size();
  }

  bool is_root() {
    return state.children.size() == 0 && get_parent()->get_vertex() == state.vertex;
  }

  Node* get_parent() {
    return state.parent;
  }

  void set_parent(Node* parent) {
    state.parent = parent;
  }

  int get_vertex() {
    return state.vertex;
  }

  std::set<Node*> get_children() {
    return state.children;
  }

  void set_children(std::set<Node*> children) {
    state.children = children;
  }

  Node* get_first_child() {
    return *state.children.begin();
  }

  ~Node() { }
};

Node** lists;
bool* contracted;
bool* root;
int* live[2];
int len[2];

void initialization(int n, std::vector<int>* children, int* parent) {
  lists = new Node*[n];
  for (int i = 0; i < n; i++) {
    lists[i] = new Node(i);
    lists[i]->set_parent(lists[i]);
  }

  for (int i = 0; i < n; i++) {
    lists[i]->state.parent = lists[parent[i]];
    for (int child : children[i]) {
      lists[i]->add_child(lists[child]);
    }
  }
  contracted = new bool[n];
  root = new bool[n];
  for (int i = 0; i < n; i++) {
    contracted[i] = 0;
    root[i] = false;
  }

  live[0] = new int[n];
  for (int i = 0; i < n; i++)
    live[0][i] = i;
  live[1] = new int[n];
  for (int i = 0; i < n; i++) live[1][i] = i;
  len[0] = n;
}

bool hash(int a, int b) {
  return (pbbs::utils::hash(a * 100000 + b)) % 2 == 0;
}

bool flips(int p, int v, int u, int round) {
  return (hash(p, round) && !hash(v, round) && hash(u, round));
}

bool is_contracted(int v, int round) {
  if (lists[v]->degree() == 0 && !lists[v]->is_root()) {
    return true;
  }
  if (lists[v]->degree() == 1) {
    Node* u = lists[v]->get_first_child();
    int p = lists[v]->get_parent()->get_vertex();
    if (v != p && u->degree() > 0 && flips(p, v, u->get_vertex(), round)) {
      return true;
    }
  }
  return false;
}

void copy_node(int v) {
  Node* new_node = new Node(*lists[v]);
  lists[v]->next = new_node;
  lists[v] = new_node;
}

void delete_node(int v) {
  Node* p = lists[v]->get_parent();
  lists[p->get_vertex()]->remove_child(lists[v]);
  if (lists[v]->degree() == 1) {
    Node* c = lists[v]->get_first_child();
    lists[p->get_vertex()]->add_child(c);
    lists[c->get_vertex()]->set_parent(p);
  }
}

void contract(int v, int round) {
  if (is_contracted(v, round)) {
    delete_node(v);
  }
}

void round(int round) {
//  std::cerr << live.size() << " " << live << std::endl;
//  for (int i  =0; i < live.size(); i++) {
//    std::cerr << "link from " << i << " : ";
//    for (Node* node : lists[i]->get_children()) {
//      std::cerr << node << " ";
//    }
//    std::cerr << "\nparent " << lists[i]->get_parent() << std::endl;
//  }
  if (round % 100 == 0) {
    std::cerr << round << " " << len[round % 2] << std::endl;
  }
//  print_array(live[round % 2], len[round % 2]);

  pasl::sched::native::parallel_for(0, len[round % 2], [&] (int i) {
    int v = live[round % 2][i];
    bool is_contr = is_contracted(v, round);
    bool is_root = lists[v]->is_root();
    if (!is_contr && !is_root) {
      copy_node(v);
    } else {
      root[v] = is_root;
      contracted[v] = is_contr;
    }
  });

  len[1 - round % 2] = pbbs::sequence::filter(live[round % 2], live[1 - round % 2], len[round % 2], [&] (int v) {
    return !is_contracted(v, round) && !root[v];
  });

//  print_array(live[1 - round % 2], len[1 - round % 2]);

  pasl::sched::native::parallel_for(0, len[1 - round % 2], [&] (int i) {
    int v = live[1 - round % 2][i];
    std::set<Node*> copy_children = lists[v]->get_children();
    for (auto child : copy_children) {
      int u = child->get_vertex();
      if (contracted[u])
        delete_node(u);
    }
  });

  pasl::sched::native::parallel_for(0, len[1 - round % 2], [&] (int i) {
    int v = live[1 - round % 2][i];
    lists[v]->set_parent(lists[v]->get_parent()->next);
    std::set<Node*> new_children;
    for (Node* child : lists[v]->get_children()) {
      new_children.insert(child->next);
    }
    lists[v]->set_children(new_children);
  });
}

void round_seq(int round) {
  if (round % 100 == 0) {
    std::cerr << round << " " << len[round % 2] << std::endl;
  }

  for (int i = 0; i < len[round % 2]; i++) {
    int v = live[round % 2][i];
    bool is_contr = is_contracted(v, round);
    bool is_root = lists[v]->is_root();
    if (!is_contr && !is_root) {
      copy_node(v);
    } else {
      root[v] = is_root;
      contracted[v] = is_contr;
    }
  }

  len[1 - round % 2] = 0;
  for (int i = 0; i < len[round % 2]; i++) {
    int v = live[round % 2][i];
    if (contracted[v]) {
      delete_node(v);
    } else {
      if (!root[v])
        live[1 - round % 2][len[1 - round % 2]++] = v;
    }
  }

  for (int i = 0; i < len[1 - round % 2]; i++) {
    int v = live[1 - round % 2][i];
    lists[v]->set_parent(lists[v]->get_parent()->next);
    std::set<Node*> new_children;
    for (Node* child : lists[v]->get_children()) {
      new_children.insert(child->next);
    }
    lists[v]->set_children(new_children);
  }
}

void construction(int n) {
  int round_no = 0;
  while (len[round_no % 2] > 0) {
    round_seq(round_no);
    round_no++;
  }

  int* roots = new int[n];
  for (int i = 0; i < n; i++) {
    roots[i] = i;
  }

  int* result = new int[n];
  int roots_number = pbbs::sequence::filter(roots, result, n, [&] (int v) {
    return root[v];
  });
  std::cout << "Number of rounds: " << round_no << std::endl;
  std::cout << "number of roots: " << roots_number << std::endl;
  for (int i = 0; i < roots_number; i++)
    std::cout << result[i] << " ";
  std::cout << std::endl;
}

int main(int argc, char** argv) {
   int cutoff, n;
   auto init = [&] {
     cutoff = (long)pasl::util::cmdline::parse_or_default_int("cutoff", 25);
     n = (long)pasl::util::cmdline::parse_or_default_int("n", 24);

     std::vector<int>* children = new std::vector<int>[n];
     int* parent = new int[n];
     if (true) {
       // let us firstly think about binary tree
       for (int i = 0; i < n; i++) {
         parent[i] = i == 0 ? 0 : (i - 1) / 2;
         children[i] = std::vector<int>();
         if (2 * i + 1 < n) {
           children[i].push_back(2 * i + 1);
         }
         if (2 * i + 2 < n) {
           children[i].push_back(2 * i + 2);
         }
       }
     } else {
       // bambooooooo
       for (int i = 0; i < n; i++) {
         parent[i] = i == 0 ? 0 : i - 1;
         children[i] = std::vector<int>();
         if (i + 1 < n)
           children[i].push_back(i + 1);
       }
     }
  
     initialization(n, children, parent);
   };

   auto run = [&] (bool sequential) {
     construction(n);
   };

   auto output = [&] {
     std::cout << "the construction has finished." << std::endl;
   };

   auto destroy = [&] {
     ;
   };

   pasl::sched::launch(argc, argv, init, run, output, destroy);

   return 0;
}                