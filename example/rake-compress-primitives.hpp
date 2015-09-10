#include <iostream>
#include <set>
#include <vector>
#include <unordered_set>
#include "sequence.hpp"
#include "hash.hpp"

#ifndef _RAKE_COMPRESS_PRIMITIVES_
#define _RAKE_COMPRESS_PRIMITIVES_

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

  Node(int _vertex) : state(_vertex) {
    head = NULL;
    next = NULL;
    proposals = NULL;
  }

  Node(const Node &node) : state(node.state) {
    head = node.head;
    next = NULL;
    proposals = NULL;
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

  void advance() {
    state.parent = state.parent->next;
    std::set<Node*> new_children;
    for (Node* c : state.children) {
      new_children.insert(c->next);
    }
    state.children.swap(new_children);
  }

  void set_children(std::set<Node*>& children) {
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
    if (proposals != NULL) {
      for (int i = 0; i < state.children.size(); i++) {
        proposals[i] = 0;
      }
    } else {
      proposals = new int[state.children.size()];
    }
  }

  void set_proposal(Node* v, int id) {
    int i = 0;
    for (Node* u : state.children) {
      if (u == v) {
        proposals[i] = id + 1;
      }
      i++;
    }
  }

  int get_proposal() {
    int id = 0;
    for (int i = 0; i < state.children.size(); i++) {
      id = std::max(id, proposals[i]);
    }
    return id - 1;
  }

  ~Node() {
    delete [] proposals;
  }
};

Node** lists;
int* live[2];
int len[2];

bool hash(int a, int b) {
  return (pbbs::utils::hash(a * 100000 + b)) % 2 == 0;
}

bool flips(int p, int v, int u, int round) {
  return (hash(p, round) && !hash(v, round) && hash(u, round));
}

bool is_contracted(Node* v, int round) {
  if (v->degree() == 0 && !v->is_root()) {
    v->set_contracted(true);
    return true;
  }
  if (v->degree() == 1) {
    Node* u = v->get_first_child();
    int p = v->get_parent()->get_vertex();
    if (v->get_vertex() != p && u->degree() > 0 && flips(p, v->get_vertex(), u->get_vertex(), round)) {
      v->set_contracted(true);
      return true;
    }
  }
  v->set_contracted(false);
  return false;
}

void copy_node(int v) {
  Node* new_node = new Node(*lists[v]);
  lists[v]->next = new_node;
  lists[v] = new_node;
}

void delete_node(Node* v) {
  Node* p = v->get_parent();
  p->next->remove_child(v);
  if (v->degree() == 1) {
    Node* c = v->get_first_child();
    p->next->add_child(c);
    c->next->set_parent(p);
  }             
}

void contract(Node* v, int round) {
  if (is_contracted(v, round)) {
    delete_node(v);
  }
}

std::unordered_set<Node*>* live_affected_sets;
std::unordered_set<Node*>* deleted_affected_sets;
int* vertex_thread;
int set_number;

void make_affected(Node* u, int id, bool to_copy) {
  if (vertex_thread[u->get_vertex()] != -1) {
    return;
  }
  lists[u->get_vertex()] = u;
  u->set_contracted(false);
  u->set_root(false);
  u->prepare();
  vertex_thread[u->get_vertex()] = id;
  Node* next = u->next;
  u->next = NULL;
  if (next != NULL)
    deleted_affected_sets[id].insert(next);
  if (to_copy) {
    copy_node(u->get_vertex());
    u = u->next;
  }
  live_affected_sets[id].insert(u);
  return;
}

int get_thread_id(Node* v) {
  if (vertex_thread[v->get_vertex()] == -1) {
    return v->get_proposal();
  } else {
    return vertex_thread[v->get_vertex()];
  }
}

bool on_frontier(Node* v) {
  if (get_thread_id(v->get_parent()) == -1) {
    return true;
  }

  for (Node* u : v->get_children()) {
    if (get_thread_id(v->get_parent()) == -1) {
      return true;
    }
  }
  return false;
}

void print_roots(int n) {
  int* roots = new int[n];
  for (int i = 0; i < n; i++) {
    roots[i] = i;
  }

  int* result = new int[n];
  int roots_number = pbbs::sequence::filter(roots, result, n, [&] (int v) {
    return lists[v]->is_known_root();
  });

  std::cout << "number of roots: " << roots_number << std::endl;
  for (int i = 0; i < roots_number; i++)
    std::cout << result[i] << " ";
  std::cout << std::endl;
  delete [] roots;
  delete [] result;
}

#endif