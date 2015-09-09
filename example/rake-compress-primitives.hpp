#include <iostream>
#include <set>
#include <vector>
#include "sequence.hpp"
#include "hash.hpp"

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
  bool contracted;
  bool root;

  State(int _vertex) : vertex(_vertex), children(), contracted(false), root(false) {}

  State(const State &state) : children(state.children), contracted(false), root(false) {
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
  int* proposals;

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
    return state.root = (state.children.size() == 0 && get_parent()->get_vertex() == state.vertex);
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

  void set_contracted(bool value) {
    state.contracted = value;
  }

  bool is_contracted() {
    return state.contracted;
  }

  void set_root(bool value) {
    state.root = value;
  }

  bool is_known_root() {
    return state.root;
  }

  void prepare() {
    if (proposals != NULL)
      delete proposals;
    proposals = new int[state.children.size()];
  }

  ~Node() { }
};

Node** lists;
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
    lists[v]->set_contracted(true);
    return true;
  }
  if (lists[v]->degree() == 1) {
    Node* u = lists[v]->get_first_child();
    int p = lists[v]->get_parent()->get_vertex();
    if (v != p && u->degree() > 0 && flips(p, v, u->get_vertex(), round)) {
      lists[v]->set_contracted(true);
      return true;
    }
  }
  lists[v]->set_contracted(false);
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