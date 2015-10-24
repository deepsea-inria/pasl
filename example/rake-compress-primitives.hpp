#include <iostream>
#include <new>
#include <set>
#include <vector>
#include <unordered_set>
#include "sequence.hpp"
#include "hash.hpp"

#ifndef _RAKE_COMPRESS_PRIMITIVES_
#define _RAKE_COMPRESS_PRIMITIVES_

const int MAX_DEGREE = 5;

int N;

void print_array(int* a, int n) {
  for (int i = 0; i < n; i++) {
    std::cerr << a[i] << " ";
  }
  std::cerr << std::endl;
}

struct Node;

#ifdef STANDART
struct State {
  int vertex;
  std::set<Node*> children;
  Node* parent;
  bool contracted;
  bool root;
  bool affected;
  bool frontier;

  State(int _vertex) : vertex(_vertex), children(), contracted(false), root(false), affected(false) {}

  State(const State &state) : children(state.children), contracted(false), root(false), affected(false) {
    vertex = state.vertex;
//    children = state.children;
    parent = state.parent;
  }

  void copy(const State &state) {
    vertex = state.vertex;
    parent = state.parent;
    children = state.children;
//    contracted = false;
    root = false;
    affected = false;
  }

  ~State() {}
};
#elif SPECIAL
struct State {
  int vertex;
  Node* children[MAX_DEGREE];
  Node* parent;
  bool contracted;
  bool root;
  bool affected;
  bool frontier;

  State(int _vertex) : vertex(_vertex), contracted(false), root(false), affected(false) {
    for (int i = 0; i < MAX_DEGREE; i++)
      children[i] = NULL;
  }

  State(const State &state) : contracted(false), root(false), affected(false) {
    vertex = state.vertex;
//    children = state.children;
    parent = state.parent;
//    children = new Node*[MAX_DEGREE];
    for (int i = 0; i < MAX_DEGREE; i++) {
      children[i] = state.children[i];
    }
  }

  void copy(const State &state) {
    vertex = state.vertex;
    parent = state.parent;
    for (int i = 0; i < MAX_DEGREE; i++) {
      children[i] = state.children[i];
    }
//    contracted = false;
    root = false;
    affected = false;
  }

  ~State() {
//    delete [] children;
  }
};
#endif

struct Node {
  Node* head;
  Node* next;
  Node* prev;
  State state;
  int proposals[MAX_DEGREE + 1];
  bool affected[MAX_DEGREE + 1];
  int cache[2];

  Node(int _vertex) : state(_vertex) {
    head = NULL;
    next = NULL;
    prev = NULL;
/*#ifdef STANDART
    proposals = NULL;
#elif SPECIAL*/
    for (int i = 0; i < MAX_DEGREE; i++) {
      proposals[i] = 0;
//      affected[i] = false;
    }
//#endif
  }

  Node(const Node &node) : state(node.state) {
    head = node.head;
    next = NULL;
    prev = NULL;
/*#ifdef STANDART
    proposals = NULL;
#elif SPECIAL*/
    for (int i = 0; i < MAX_DEGREE; i++) {
      proposals[i] = 0;
//      affected[i] = false;
    }
//#endif
  }

  void add_child(Node* child) {
#ifdef STANDART
    state.children.insert(child);
#elif SPECIAL
    for (int i = 0; i < MAX_DEGREE; i++) {
      if (state.children[i] == NULL) {
        state.children[i] = child;
        break;
      }
    }
#endif
  }    

  void remove_child(Node* child) {
#ifdef STANDART
    state.children.erase(child);
#elif SPECIAL
    for (int i = 0; i < MAX_DEGREE; i++) {
      if (state.children[i] == child) {
        state.children[i] = NULL;
        break;
      }
    }
#endif
  }

  void replace_child(Node* child1, Node* child2) {
    remove_child(child1);
    add_child(child2);
  }

  int degree() {
#ifdef STANDART
    return state.children.size();
#elif SPECIAL
    int ans = 0;
    for (int i = 0; i < MAX_DEGREE; i++) {
      if (state.children[i] != NULL) {
        ans++;
      }
    }
    return ans;
#endif
  }

  bool is_root() {
    return state.root = (degree() == 0 && get_parent()->get_vertex() == state.vertex);
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

#ifdef STANDART
  std::set<Node*>& get_children() {
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
#elif SPECIAL
  Node** get_children() {
    return state.children;
  }

  void advance() {state.parent = state.parent->next;
    for (int i = 0; i < MAX_DEGREE; i++) {
      if (state.children[i] != NULL) {
        state.children[i] = state.children[i]->next;
      }
    }
  }

  void set_children(Node** children) {
//    state.children = children;
    for (int i = 0; i < MAX_DEGREE; i++) {
      state.children[i] = children[i];
    }
  }

  Node* get_first_child() {
    for (int i = 0; i < MAX_DEGREE; i++) {
      if (state.children[i] != NULL) {
        return state.children[i];
      }
    }
    return NULL; 
  }
#endif

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

#ifdef STANDART
  bool is_affected() {
    if (state.affected)
      return true;
    for (int i = 0; i < state.children.size() + 1; i++) {
      if (affected[i]) return state.affected = true;
    }
    return false;
  }

  void set_affected(Node* v) {
    if (state.parent == v) {
      affected[0] = true;
      return;
    }
    int i = 1;
    for (Node* u : state.children) {
      if (u == v) {
        affected[i] = true;
        return;
      }
      i++;
    }
  }

  void prepare() {state.affected = false;
//#ifdef STANDART    
//    if (proposals != NULL) {
///*      for (int i = 0; i < state.children.size() + 1; i++) {
//        proposals[i] = 0;
//      }*/
//      delete [] affected;
//      delete [] proposals;
//    }
//    affected = new bool[state.children.size() + 1];
//    proposals = new int[state.children.size() + 1];
//#endif
//    for (int i = 0; i < state.children.size() + 1; i++)
//      affected[i] = false;

    for (int i = 0; i < state.children.size() + 1; i++)
      proposals[i] = 0;
  }

  void set_proposal(Node* v, int id) {
    if (state.parent == v) {
      proposals[0] = id + 1;
      return;
    }
    int i = 1;
    for (Node* u : state.children) {
      if (u == v) {
        proposals[i] = id + 1;
        return;
      }
      i++;
    }
  }

  int get_proposal() {
    int id = 0;
    for (int i = 0; i < state.children.size() + 1; i++) {
      id = std::max(id, proposals[i]);
    }
    return id - 1;
  }
#elif SPECIAL
  bool is_affected() {
    if (state.affected)
      return true;
    for (int i = 0; i < MAX_DEGREE + 1; i++) {
      if (affected[i]) return state.affected = true;
    }
    return false;
  }

  void set_affected(Node* v) {
    if (state.parent == v) {
      affected[0] = true;
      return;
    }
    int i = 1;
    for (int i = 0; i < MAX_DEGREE; i++) {
      if (state.children[i] == v) {
        affected[i] = true;
        return;
      }
      i++;
    }
  }

  void prepare() {state.affected = false;
/*    for (int i = 0; i < MAX_DEGREE + 1; i++)
      affected[i] = false;*/

    for (int i = 0; i < MAX_DEGREE + 1; i++)
      proposals[i] = 0;
  }


  void set_proposal(Node* v, int id) {
    if (state.parent == v) {
      proposals[0] = id + 1;
      return;
    }
    for (int i = 0; i < MAX_DEGREE; i++) {
      if (state.children[i] == v) {
        proposals[i + 1] = id + 1;
        return;
      }
    }
  }

  int get_proposal() {
    int id = 0;
    for (int i = 0; i < MAX_DEGREE + 1; i++) {
      id = std::max(id, proposals[i]);
    }
    return id - 1;
  }

#endif

  void copy_state(Node* u) {
    state.copy(u->state);
    head = u->head;
  }

  ~Node() {
//    delete [] proposals;
//    delete [] affected;
  }
};

Node** lists;
int* live[2];
int len[2];
int* tmp;

#ifdef SPECIAL
Node* memory;
#endif

bool hash(int a, int b) {
  return (pbbs::utils::hash(a * 100000 + b)) % 2 == 0;
}

bool flips(int p, int v, int u, int round) {
  return (hash(p, round) && !hash(v, round) && hash(u, round));
}

bool is_contracted(Node* v, int round) {
  if (v->degree() == 0 && !v->is_root()) {
//    v->set_contracted(true);
    return true;
  }
  if (v->degree() == 1) {
    Node* u = v->get_first_child();
    int p = v->get_parent()->get_vertex();
    if (v->get_vertex() != p && u->degree() > 0 && flips(p, v->get_vertex(), u->get_vertex(), round)) {
//      v->set_contracted(true);
      return true;
    }
  }
//  v->set_contracted(false);
  return false;
}

void copy_node(Node* v) {
  if (v->next == NULL) {
#ifdef STANDART
    Node* new_node = new Node(*v);
#elif SPECIAL
    Node* new_node = new (v + N) Node(*v);
//    Node* new_node = new (v + 1) Node(*v);
#endif
    v->next = new_node;
    lists[v->get_vertex()] = new_node;
  } else {
    v->next->copy_state(v);
    lists[v->get_vertex()] = v->next;
  }
  v->next->prev = v;
  lists[v->get_vertex()]->prepare();
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

void delete_node_for(Node* v, Node* u) {
  Node* p = v->get_parent();
  if (p->next == u)
    p->next->remove_child(v);
  if (v->degree() == 1) {
    Node* c = v->get_first_child();
    if (p->next == u)
      p->next->add_child(c);
    if (c->next == u)
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
  //need to affect vertex, which will not be contracted later
//  if (lists[u->get_vertex()]->is_contracted()) {
//    Node* v = lists[u->get_vertex()];
//    Node* p = v->get_parent();
////    std::cerr << "set proposal from " << v->get_vertex() << "(" << v << ")" << " to " << p->get_vertex() << "(" << p << ")" ;
////    if (vertex_thread[p->get_vertex()] == -1) {
//      p->set_affected(v);
////      p->set_affected(true);
///*&    if (v == u) {
//      make_affected(p, id, to_copy);
//    }*/
////    }
//#ifdef STANDART
//    for (Node* c : v->get_children()) {
////      if (vertex_thread[c->get_vertex()] == -1) {
//        c->set_affected(v);
////        c->set_affected(true);
///*        if (v == u) {
//          make_affected(c, id, to_copy);
//        } */
////      }
//    }
//#elif SPECIAL
//    Node* child = v->get_first_child();
//    if (child != NULL) {
//      child->set_affected(v);
//    }
//#endif
//  }
//  std::cerr << "\nMADE AFFECTED: " << u->get_vertex() << "\n";

  lists[u->get_vertex()] = u;
  if (to_copy)
    u->set_contracted(false);
  u->set_root(false);
  u->prepare();
  vertex_thread[u->get_vertex()] = id;
  if (to_copy) {
    copy_node(u);
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
  if (vertex_thread[v->get_parent()->get_vertex()] == -1) {
    return true;
  }

#ifdef STANDART
  for (Node* u : v->get_children()) {
    if (vertex_thread[u->get_vertex()] == -1) {
      return true;
    }
  }
#elif SPECIAL
  Node** children = v->get_children();
  for (int i = 0; i < MAX_DEGREE; i++) {
    if (children[i] != NULL && vertex_thread[children[i]->get_vertex()] == -1) {
      return true;
    }
  }
#endif

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

  std::cerr << "number of roots: " << roots_number << std::endl;
  for (int i = 0; i < roots_number; i++)
    std::cerr << result[i] << " ";
  std::cerr << std::endl;
  delete [] roots;
  delete [] result;
}

void print_graph(int n) {
  for (int i = 0; i < n; i++) {
    if (!lists[i]->is_contracted() && !lists[i]->is_root()) {
      std::cout << lists[i]->get_vertex() << " (" << lists[i]->get_parent()->get_vertex() << "): ";
#ifdef STANDART
      for (Node* child : lists[i]->get_children()) {
        std::cout << child->get_vertex() << " ";
      }
#elif SPECIAL
      Node** children = lists[i]->get_children();
      for (int i = 0; i < MAX_DEGREE; i++) {
        if (children[i] != NULL)
          std::cout << children[i]->get_vertex() << " ";
      }
#endif
      std::cout << std::endl;
    }
  }
}

#endif