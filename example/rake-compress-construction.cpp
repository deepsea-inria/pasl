#include <iostream>
#include <set>
#include <vector>
#include "sparray.hpp"
#include "hash.hpp"
#include "cmdline.hpp"
#include "benchmark.hpp"

struct Node;

struct State {
  int vertex;
  std::set<Node*> children;
  Node* parent;

  State(int _vertex) : vertex(_vertex) {}

  State(const State &state) {
    vertex = state.vertex;
    children = state.children;
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
    return state.children.size() == 0 && get_parent() == state.vertex;
  }

  int get_parent() {
    return state.parent->get_vertex();
  }

  int set_parent(Node* parent) {
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
bool* root;

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
}

bool hash(int a, int b) {
  return (hash_signed(a) + hash_signed(b)) % 2 == 0;
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
    int p = lists[v]->get_parent();
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
  int p = lists[v]->get_parent();
  lists[p]->remove_child(lists[v]);
  if (lists[v]->degree() == 1) {
    int c = lists[v]->get_first_child()->get_vertex();
    lists[p]->add_child(lists[c]);
    lists[c]->set_parent(lists[p]);
  }
}

void contract(int v, int round) {
  if (is_contracted(v, round)) {
    delete_node(v);
  }
}

void round(sparray& live, int round) {
//  std::cerr << live.size() << " " << live << std::endl;
//  for (int i  =0; i < live.size(); i++) {
//    std::cerr << "link from " << i << " : ";
//    for (Node* node : lists[i]->get_children()) {
//      std::cerr << node << " ";
//    }
//    std::cerr << "\nparent " << lists[i]->get_parent() << std::endl;
//  }
  pasl::sched::native::parallel_for(0, (int)live.size(), [&] (int i) {
    int v = live[i];
    bool is_contr = is_contracted(v, round);
    bool is_root = lists[v]->is_root();
    if (!is_contr && !is_root) {
      copy_node(v);
    } else {
      root[v] = is_root;
    }
  });

  live = filter([&] (int i) {
    int v = live[i];
    return !is_contracted(v, round) && !root[v];
  }, live);

  pasl::sched::native::parallel_for(0, (int)live.size(), [&] (int i) {
    int v = live[i];
    std::set<Node*> copy_children = lists[v]->get_children();
    for (auto child : copy_children) {
      int u = child->get_vertex();
      contract(u, round);
    }
  });

  pasl::sched::native::parallel_for(0, (int)live.size(), [&] (int i) {
    int v = live[i];
    lists[v]->set_parent(lists[lists[v]->get_parent()]);
    ;
    std::set<Node*> new_children;
    for (Node* child : lists[v]->get_children()) {
      new_children.insert(child->next);
    }
    lists[v]->set_children(new_children);
  });
}

void construction(int n) {
  sparray live(n);
  for (int i = 0; i < n; i++) {
    live[i] = i;
  }
  root = new bool[n];
  int round_no = 0;
  while (live.size() > 0) {
    round(live, round_no);
    round_no++;
  }

  sparray roots(n);
  for (int i = 0; i < n; i++) {
    roots[i] = i;
  }

  roots = filter([&] (int v) {
    return root[v];
  }, roots);
  std::cerr << roots << std::endl;
}

int cutoff, n;

int main(int argc, char** argv) {
   int cutoff, n;
   auto init = [&] {
     cutoff = (long)pasl::util::cmdline::parse_or_default_int("cutoff", 25);
     n = (long)pasl::util::cmdline::parse_or_default_int("n", 24);

     std::vector<int>* children = new std::vector<int>[n];
     int* parent = new int[n];
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