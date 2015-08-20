/*!
 * \file bench.cpp
 * \brief Benchmarking script for DAG machine
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include <random>
#include <map>
#include <set>
#include <deque>
#include <cmath>
#include <memory>

#include "pasl.hpp"
#include "tagged.hpp"

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Tagged-pointer routines */

template <class T>
T* tagged_pointer_of(T* n) {
  return pasl::data::tagged::extract_value<T*>(n);
}

template <class T>
int tagged_tag_of(T* n) {
  return (int)pasl::data::tagged::extract_tag<long>(n);
}

template <class T>
T* tagged_tag_with(T* n, int t) {
  return pasl::data::tagged::create<T*, T*>(n, (long)t);
}

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/* Random-number generator */

pasl::data::perworker::array<std::mt19937> generator;

// returns a random integer in the range [lo, hi)
int random_int(int lo, int hi) {
  std::uniform_int_distribution<int> distribution(lo, hi-1);
  return distribution(generator.mine());
}

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/* Global parameters */

int communication_delay = 100;

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/* The top-down algorithm */

namespace topdown {

class node;
class incounter;
class outset;

void add_node(node*);
void add_edge(node*, node*);
void add_edge(node*, outset*, node*, incounter*);
void prepare_node(node*);
void prepare_node(node*, incounter*);
void prepare_node(node*, outset*);
void prepare_node(node*, incounter*, outset*);
void join_with(node*, incounter*);
void continue_with(node*);
void decrement_incounter(node*);
incounter* incounter_ready();
incounter* incounter_unary();
incounter* incounter_fetch_add();
incounter* incounter_new(node*);
outset* outset_unary();
outset* outset_noop();
outset* outset_new();
  
class incounter : public pasl::sched::instrategy::common {
public:
  
  using status_type = enum {
    activated,
    not_activated
  };
  
  virtual bool is_activated() const = 0;
  
  virtual void increment(node* source) = 0;
  
  virtual status_type decrement(node* source) = 0;
  
  void check(pasl::sched::thread_p t) {
    if (is_activated()) {
      start(t);
    }
  }
  
  void delta(node* source, pasl::sched::thread_p target, int64_t d) {
    if (d == -1L) {
      if (decrement(source) == activated) {
        start(target);
      }
    } else if (d == +1L) {
      increment(source);
    } else {
      assert(false);
    }
  }
  
  void delta(pasl::sched::thread_p target, int64_t d) {
    delta(nullptr, target, d);
  }
  
};

class outset : public pasl::sched::outstrategy::common {
public:
  
  using insert_status_type = enum {
    insert_success,
    insert_fail
  };
  
  virtual insert_status_type insert(node*) = 0;
  
  virtual void finish() = 0;
  
  virtual void destroy() {
    delete this;
  }
  
  void add(pasl::sched::thread_p t) {
    insert((node*)t);
  }
  
  void finished() {
    finish();
  }
  
  bool should_deallocate_automatically = true;
  
  virtual void enable_future() {
    should_deallocate_automatically = false;
  }
  
};

using edge_algorithm_type = enum {
  edge_algorithm_simple,
  edge_algorithm_distributed,
  edge_algorithm_tree
};
  
edge_algorithm_type edge_algorithm;
  
namespace simple {
  
class simple_outset : public outset {
public:
  
  using concurrent_list_type = struct concurrent_list_struct {
    node* n;
    struct concurrent_list_struct* next;
  };
  
  std::atomic<concurrent_list_type*> head;
  
  const int finished_code = 1;
  
  simple_outset() {
    head.store(nullptr);
  }
  
  insert_status_type insert(node* n) {
    insert_status_type result = insert_success;
    concurrent_list_type* cell = new concurrent_list_type;
    cell->n = n;
    while (true) {
      concurrent_list_type* orig = head.load();
      if (tagged_tag_of(orig) == finished_code) {
        result = insert_fail;
        delete cell;
        break;
      } else {
        cell->next = orig;
        if (head.compare_exchange_strong(orig, cell)) {
          break;
        }
      }
    }
    return result;
  }
  
  void finish() {
    concurrent_list_type* todo = nullptr;
    while (true) {
      concurrent_list_type* orig = head.load();
      concurrent_list_type* v = (concurrent_list_type*)nullptr;
      concurrent_list_type* next = tagged_tag_with(v, finished_code);
      if (head.compare_exchange_strong(orig, next)) {
        todo = orig;
        break;
      }
    }
    while (todo != nullptr) {
      node* n = todo->n;
      concurrent_list_type* next = todo->next;
      delete todo;
      decrement_incounter(n);
      todo = next;
    }
    if (should_deallocate_automatically) {
      delete this;
    }
  }

  
};
  
} // end namespace
  
namespace distributed {
  
namespace snzi {

class node {
public:
  
  using contents_type = struct {
    int c; // counter value
    int v; // version number
  };
  
  static constexpr int one_half = -1;
  
  static constexpr int root_node_tag = 1;
  
  std::atomic<contents_type> X;
  
  node* parent;
  
  static bool is_root_node(const node* n) {
    return tagged_tag_of(n) == 1;
  }
  
  template <class Item>
  static node* create_root_node(Item x) {
    return (node*)tagged_tag_with(x, root_node_tag);
  }
  
  node(node* _parent = nullptr) {
    parent = (_parent == nullptr) ? create_root_node(_parent) : _parent;
    contents_type init;
    init.c = 0;
    init.v = 0;
    X.store(init);
  }
  
  void arrive() {
    bool succ = false;
    int undo_arr = 0;
    while (! succ) {
      contents_type x = X.load();
      if (x.c >= 1) {
        contents_type orig = x;
        contents_type next = x;
        next.c++;
        succ = X.compare_exchange_strong(orig, next);
      }
      if (x.c == 0) {
        contents_type orig = x;
        contents_type next = x;
        next.c = one_half;
        next.v++;
        if (X.compare_exchange_strong(orig, next)) {
          succ = true;
          x.c = one_half;
          x.v++;
        }
      }
      if (x.c == one_half) {
        if (! is_root_node(parent)) {
          parent->arrive();
        }
        contents_type orig = x;
        contents_type next = x;
        next.c = 1;
        if (! X.compare_exchange_strong(orig, next)) {
          undo_arr++;
        }
      }
    }
    if (is_root_node(parent)) {
      return;
    }
    while (undo_arr > 0) {
      parent->depart();
      undo_arr--;
    }
  }
  
  // returns true if this call to depart caused a zero count, false otherwise
  bool depart() {
    while (true) {
      contents_type x = X.load();
      assert(x.c >= 1);
      contents_type orig = x;
      contents_type next = x;
      next.c--;
      if (X.compare_exchange_strong(orig, next)) {
        bool s = (x.c == 1);
        if (is_root_node(parent)) {
          return s;
        } else if (s) {
          return parent->depart();
        } else {
          return false;
        }
      }
    }
  }
  
  bool is_nonzero() const {
    return (X.load().c > 0);
  }
  
  template <class Item>
  static void set_root_annotation(node* n, Item x) {
    node* m = n;
    assert(! is_root_node(m));
    while (! is_root_node(m->parent)) {
      m = m->parent;
    }
    assert(is_root_node(m->parent));
    m->parent = create_root_node(x);
  }
  
  template <class Item>
  static Item get_root_annotation(node* n) {
    node* m = n;
    while (! is_root_node(m)) {
      m = m->parent;
    }
    assert(is_root_node(m));
    return (Item)tagged_pointer_of(m);
  }
  
};
  
int default_branching_factor = 2;
int default_nb_levels = 3;

class tree {
private:
  
  int branching_factor;
  int nb_levels;
  
  std::vector<node*> nodes;
  
  void build() {
    nodes.push_back(new node);
    for (int i = 1; i <= nb_levels; i++) {
      int e = (int)nodes.size();
      int s = e - std::pow(branching_factor, i - 1);
      for (int j = s; j < e; j++) {
        for (int k = 0; k < branching_factor; k++) {
          nodes.push_back(new node(nodes[j]));
        }
      }
    }
  }
  
  int get_nb_leaf_nodes() const {
    return std::pow(branching_factor, nb_levels - 1);
  }
  
  node* ith_leaf_node(int i) const {
    assert((i >= 0) && (i < get_nb_leaf_nodes()));
    int n = (int)nodes.size();
    int j = n - (i + 1);
    return nodes[j];
  }
  
  static unsigned int hashu(unsigned int a) {
    a = (a+0x7ed55d16) + (a<<12);
    a = (a^0xc761c23c) ^ (a>>19);
    a = (a+0x165667b1) + (a<<5);
    a = (a+0xd3a2646c) ^ (a<<9);
    a = (a+0xfd7046c5) + (a<<3);
    a = (a^0xb55a4f09) ^ (a>>16);
    return a;
  }
  
public:
  
  tree(int branching_factor = default_branching_factor,
       int nb_levels = default_nb_levels)
  : branching_factor(branching_factor), nb_levels(nb_levels) {
    build();
  }
  
  ~tree() {
    for (node* n : nodes) {
      delete n;
    }
  }
  
  template <class Item>
  node* random_leaf_of(Item x) const {
    union {
      Item x;
      long b;
    } bits;
    bits.x = x;
    int h = std::abs((int)hashu((unsigned int)bits.b));
    int n = get_nb_leaf_nodes();
    int l = h % n;
    return ith_leaf_node(l);
  }
  
  bool is_nonzero() const {
    return nodes[0]->is_nonzero();
  }
  
  template <class Item>
  void set_root_annotation(Item x) {
    node::set_root_annotation(nodes[0], x);
  }
  
};

} // end namespace
  
class distributed_incounter : public incounter {
public:
      
  snzi::tree nzi;
  
  distributed_incounter(node* n) {
    nzi.set_root_annotation(n);
  }
  
  bool is_activated() const {
    return (! nzi.is_nonzero());
  }
  
  void increment(node* source) {
    nzi.random_leaf_of(source)->arrive();
  }
  
  status_type decrement(node* source) {
    bool b = nzi.random_leaf_of(source)->depart();
    return b ? incounter::activated : incounter::not_activated;
  }
  
};

class distributed_outset : public outset, public pasl::util::worker::periodic_t {
public:
  
  using node_buffer_type = std::vector<node*>;
  
  pasl::data::perworker::array<node_buffer_type> nodes;
  
  snzi::tree nzi;
  
  bool finished_indicator = false;
  
  distributed_outset() {
    add_calling_processor();
  }
  
  insert_status_type insert(node* n) {
    if (finished_indicator) {
      return insert_fail;
    }
    add_calling_processor();
    node_buffer_type& buffer = nodes.mine();
    buffer.push_back(n);
    return insert_success;
  }
  
  void finish() {
    finished_indicator = true;
  }
  
  void destroy() {
    assert(! should_deallocate_automatically);
    depart(0);
  }
  
  void enable_future() {
    should_deallocate_automatically = false;
    arrive(0);
  }
  
  void process_buffer() {
    node_buffer_type& buffer = nodes.mine();
    while (! buffer.empty()) {
      node* n = buffer.back();
      buffer.pop_back();
      decrement_incounter(n);
    }
  }
  
  void check() {
    if (finished_indicator) {
      remove_calling_processor();
    }
  }
  
  void add_calling_processor() {
    if (pasl::sched::scheduler::get_mine()->is_in_periodic(this)) {
      return;
    }
    arrive(pasl::util::worker::get_my_id());
    pasl::sched::scheduler::get_mine()->add_periodic(this);
  }
  
  void remove_calling_processor() {
    assert(finished_indicator);
    assert(pasl::sched::scheduler::get_mine()->is_in_periodic(this));
    pasl::sched::scheduler::get_mine()->rem_periodic(this);
    depart(pasl::util::worker::get_my_id());
  }
  
  void arrive(pasl::worker_id_t my_id) {
    nzi.random_leaf_of(my_id)->arrive();
  }
  
  void depart(pasl::worker_id_t my_id) {
    process_buffer();
    if (nzi.random_leaf_of(my_id)->depart()) {
      delete this;
    }
  }
  
};
  
void unary_finished(pasl::sched::thread_p t) {
  snzi::node* leaf = (snzi::node*)t;
  if (leaf->depart()) {
    node* n = snzi::node::get_root_annotation<node*>(leaf);
    pasl::sched::instrategy::schedule((pasl::sched::thread_p)n);
  }
}
  
} // end namespace
  
namespace dyntree {
  
int branching_factor = 2;
  
class dyntree_incounter;
class dyntree_outset;
class ictnode;
class ostnode;

void deallocate_incounter_tree(ictnode*);
void notify_outset_nodes(dyntree_outset*);
void deallocate_outset_tree(ostnode*);
  
class ictnode {
public:
  
  static constexpr int minus_tag = 1;
  
  std::atomic<ictnode*>* children;
  
  void init(ictnode* v) {
    children = new std::atomic<ictnode*>[branching_factor];
    for (int i = 0; i < branching_factor; i++) {
      children[i].store(v);
    }
  }
  
  ictnode() {
    init(nullptr);
  }
  
  ictnode(ictnode* i) {
    init(i);
  }
  
  ~ictnode() {
    delete [] children;
  }
  
  bool is_leaf() const {
    for (int i = 0; i < branching_factor; i++) {
      ictnode* child = tagged_pointer_of(children[i].load());
      if (child != nullptr) {
        return false;
      }
    }
    return true;
  }
  
};

class dyntree_incounter : public incounter {
public:
  
  ictnode* in;
  ictnode* out;
  
  ictnode* minus() const {
    return tagged_tag_with((ictnode*)nullptr, ictnode::minus_tag);
  }
  
  dyntree_incounter() {
    in = nullptr;
    out = new ictnode(minus());
    out = tagged_tag_with(out, ictnode::minus_tag);
  }
  
  ~dyntree_incounter() {
    assert(is_activated());
    deallocate_incounter_tree(tagged_pointer_of(out));
    out = nullptr;
  }
  
  bool is_activated() const {
    return in == nullptr;
  }
  
  void increment(node*) {
    ictnode* leaf = new ictnode;
    while (true) {
      if (in == nullptr) {
        in = leaf;
        return;
      }
      assert(in != nullptr);
      ictnode* current = in;
      while (true) {
        int i = random_int(0, branching_factor);
        std::atomic<ictnode*>& branch = current->children[i];
        ictnode* next = branch.load();
        if (tagged_tag_of(next) == ictnode::minus_tag) {
          break;
        }
        if (next == nullptr) {
          ictnode* orig = nullptr;
          if (branch.compare_exchange_strong(orig, leaf)) {
            return;
          } else {
            break;
          }
        }
        current = next;
      }
    }
  }
  
  status_type decrement(node*) {
    while (true) {
      ictnode* current = in;
      assert(current != nullptr);
      if (current->is_leaf()) {
        if (try_to_detatch(current)) {
          in = nullptr;
          add_to_out(current);
          return activated;
        }
      }
      while (true) {
        int i = random_int(0, branching_factor);
        std::atomic<ictnode*>& branch = current->children[i];
        ictnode* next = branch.load();
        if (   (next == nullptr)
            || (tagged_tag_of(next) == ictnode::minus_tag) ) {
          break;
        }
        if (next->is_leaf()) {
          if (try_to_detatch(next)) {
            branch.store(nullptr);
            add_to_out(next);
            return not_activated;
          }
          break;
        }
        current = next;
      }
    }
    return not_activated;
  }
  
  bool try_to_detatch(ictnode* n) {
    for (int i = 0; i < branching_factor; i++) {
      ictnode* orig = nullptr;
      if (! (n->children[i].compare_exchange_strong(orig, minus()))) {
        for (int j = i - 1; j >= 0; j--) {
          n->children[j].store(nullptr);
        }
        return false;
      }
    }
    return true;
  }
  
  void add_to_out(ictnode* n) {
    n = tagged_tag_with(n, ictnode::minus_tag);
    while (true) {
      ictnode* current = tagged_pointer_of(out);
      while (true) {
        int i = random_int(0, branching_factor);
        std::atomic<ictnode*>& branch = current->children[i];
        ictnode* next = branch.load();
        if (tagged_pointer_of(next) == nullptr) {
          ictnode* orig = next;
          if (branch.compare_exchange_strong(orig, n)) {
            return;
          }
          break;
        }
        current = tagged_pointer_of(next);
      }
    }
  }
  
};
  
class ostnode {
public:
  
  enum {
    empty=1,
    leaf=2,
    interior=3,
    finished_empty=4,
    finished_leaf=5,
    finished_interior=6
  };
  
  using tagged_pointer_type = union {
    ostnode* interior;
    node* leaf;
  };
  
  std::atomic<tagged_pointer_type>* children;
  
  void init() {
    children = new std::atomic<tagged_pointer_type>[branching_factor];
    for (int i = 0; i < branching_factor; i++) {
      tagged_pointer_type p;
      p.interior = tagged_tag_with((ostnode*)nullptr, empty);
      children[i].store(p);
    }
  }
  
  ostnode() {
    init();
  }
  
  ostnode(tagged_pointer_type child1, tagged_pointer_type child2) {
    init();
    children[0].store(child1);
    children[1].store(child2);
  }
  
  ~ostnode() {
    delete [] children;
  }
  
  static tagged_pointer_type make_finished(tagged_pointer_type p) {
    tagged_pointer_type result;
    int tag = tagged_tag_of(p.interior);
    if (tag == empty) {
      ostnode* n = tagged_pointer_of(p.interior);
      result.interior = tagged_tag_with(n, finished_empty);
    } else if (tag == leaf) {
      node* n = tagged_pointer_of(p.leaf);
      result.leaf = tagged_tag_with(n, finished_leaf);
    } else if (tag == interior) {
      ostnode* n = tagged_pointer_of(p.interior);
      result.interior = tagged_tag_with(n, finished_interior);
    } else {
      assert(false);
    }
    return result;
  }
  
};
  
class dyntree_outset : public outset {
public:
 
  ostnode* root;
  
  dyntree_outset() {
    root = new ostnode;
  }
  
  ~dyntree_outset() {
    deallocate_outset_tree(root);
  }
  
  insert_status_type insert(ostnode::tagged_pointer_type val) {
    ostnode* current = root;
    ostnode* next = nullptr;
    while (true) {
      ostnode::tagged_pointer_type n;
      while (true) {
        int i = random_int(0, branching_factor);
        n = current->children[i].load();
        int tag = tagged_tag_of(n.interior);
        if (   (tag == ostnode::finished_empty)
            || (tag == ostnode::finished_leaf)
            || (tag == ostnode::finished_interior) ) {
          return insert_fail;
        }
        if (tag == ostnode::empty) {
          ostnode::tagged_pointer_type orig = n;
          if (current->children[i].compare_exchange_strong(orig, val)) {
            return insert_success;
          }
          n = current->children[i].load();
          tag = tagged_tag_of(n.interior);
        }
        if (tag == ostnode::leaf) {
          ostnode::tagged_pointer_type orig = n;
          ostnode* tmp = new ostnode(val, n);
          ostnode::tagged_pointer_type next;
          next.interior = tagged_tag_with(tmp, ostnode::interior);
          if (current->children[i].compare_exchange_strong(orig, next)) {
            return insert_success;
          }
          delete tmp;
          n = current->children[i].load();
          tag = tagged_tag_of(n.interior);
        }
        if (tag == ostnode::interior) {
          next = tagged_pointer_of(n.interior);
          break;
        }
      }
      current = next;
    }
    assert(false); // control should never reach here
    return insert_fail;
  }
  
  insert_status_type insert(node* leaf) {
    ostnode::tagged_pointer_type val;
    val.leaf = tagged_tag_with(leaf, ostnode::leaf);
    return insert(val);
  }

  void add(pasl::sched::thread_p t) {
    assert(false);
  }
  
  void finish() {
    notify_outset_nodes(this);
  }

};
  
} // end namespace
  
class node : public pasl::sched::thread {
public:
  
  using incounter_type = incounter;
  using outset_type = outset;
  
  const int uninitialized_block_id = -1;
  const int entry_block_id = 0;
  
  int current_block_id;
  
private:
  
  int continuation_block_id;
  
public:
  
  node()
  : current_block_id(uninitialized_block_id),
    continuation_block_id(entry_block_id) { }
  
  virtual void body() = 0;
  
  void run() {
    current_block_id = continuation_block_id;
    continuation_block_id = uninitialized_block_id;
    assert(current_block_id != uninitialized_block_id);
    body();
  }
  
  void prepare_for_transfer(int target) {
    pasl::sched::threaddag::reuse_calling_thread();
    continuation_block_id = target;
  }
  
  void jump_to(int continuation_block_id) {
    prepare_for_transfer(continuation_block_id);
    continue_with(this);
  }
  
  void async(node* producer, node* consumer, int continuation_block_id) {
    prepare_node(producer, incounter_ready(), outset_unary());
    add_edge(producer, consumer);
    jump_to(continuation_block_id);
    add_node(producer);
  }
  
  void finish(node* producer, int continuation_block_id) {
    node* consumer = this;
    prepare_node(producer, incounter_ready(), outset_unary());
    prepare_for_transfer(continuation_block_id);
    join_with(consumer, incounter_new(this));
    add_edge(producer, consumer);
    add_node(producer);
  }
  
  outset* future(node* producer, int continuation_block_id) {
    node* consumer = this;
    prepare_node(producer, incounter_ready());
    outset* producer_out = (outset*)producer->out;
    producer_out->enable_future();
    consumer->jump_to(continuation_block_id);
    add_node(producer);
    return producer_out;
  }
  
  void force(outset* producer_out, int continuation_block_id) {
    node* consumer = this;
    prepare_for_transfer(continuation_block_id);
    incounter* consumer_in = incounter_unary();
    join_with(consumer, consumer_in);
    node* producer = nullptr; // dummy value
    add_edge(producer, producer_out, consumer, consumer_in);
  }
  
  void call(node* target, int continuation_block_id) {
    finish(target, continuation_block_id);
  }
  
  void deallocate_future(outset* future) const {
    assert(! future->should_deallocate_automatically);
    future->destroy();
  }
  
  THREAD_COST_UNKNOWN
  
};

incounter* incounter_ready() {
  return (incounter*)pasl::sched::instrategy::ready_new();
}

incounter* incounter_unary() {
  return (incounter*)pasl::sched::instrategy::unary_new();
}
  
incounter* incounter_fetch_add() {
  return (incounter*)pasl::sched::instrategy::fetch_add_new();
}
  
incounter* incounter_new(node* n) {
  if (edge_algorithm == edge_algorithm_simple) {
    return incounter_fetch_add();
  } else if (edge_algorithm == edge_algorithm_distributed) {
    return new distributed::distributed_incounter(n);
  } else if (edge_algorithm == edge_algorithm_tree) {
    return new dyntree::dyntree_incounter;
  } else {
    assert(false);
    return nullptr;
  }
}
  
const bool enable_distributed = true;

outset* outset_unary() {
  if (enable_distributed && edge_algorithm == edge_algorithm_distributed) {
    return (outset*)pasl::sched::outstrategy::topdown_distributed_unary_new(nullptr);
  } else {
    return (outset*)pasl::sched::outstrategy::unary_new();
  }
}
  
outset* outset_noop() {
  return (outset*)pasl::sched::outstrategy::noop_new();
}

outset* outset_new() {
  if (edge_algorithm == edge_algorithm_simple) {
    return new simple::simple_outset;
  } else if (edge_algorithm == edge_algorithm_distributed) {
    return new distributed::distributed_outset;
  } else if (edge_algorithm == edge_algorithm_tree) {
    return new dyntree::dyntree_outset;
  } else {
    assert(false);
    return nullptr;
  }
}
  
void increment_incounter(node* source, node* target, incounter* target_in) {
  long tag = pasl::sched::instrategy::extract_tag(target_in);
  assert(tag != pasl::sched::instrategy::READY_TAG);
  if (tag == pasl::sched::instrategy::UNARY_TAG) {
    // nothing to do
  } else if (tag == pasl::sched::instrategy::FETCH_ADD_TAG) {
    pasl::data::tagged::atomic_fetch_and_add<pasl::sched::instrategy_p>(&(target->in), 1l);
  } else {
    assert(tag == 0);
    source = enable_distributed ? source : nullptr;
    target_in->delta(source, target, +1L);
  }
}
  
void increment_incounter(node* source, node* target) {
  increment_incounter(source, target, (incounter*)target->in);
}
  
void decrement_incounter(node* source, node* target, incounter* target_in) {
  long tag = pasl::sched::instrategy::extract_tag(target_in);
  assert(tag != pasl::sched::instrategy::READY_TAG);
  if (tag == pasl::sched::instrategy::UNARY_TAG) {
    pasl::sched::instrategy::schedule(target);
  } else if (tag == pasl::sched::instrategy::FETCH_ADD_TAG) {
    int64_t old = pasl::data::tagged::atomic_fetch_and_add<pasl::sched::instrategy_p>(&(target->in), -1l);
    if (old == 1) {
      pasl::sched::instrategy::schedule(target);
    }
  } else {
    assert(tag == 0);
    source = enable_distributed ? source : nullptr;
    target_in->delta(source, target, -1L);
  }
}

void decrement_incounter(node* source, node* target) {
  decrement_incounter(source, target, (incounter*)target->in);
}
  
void decrement_incounter(node* target) {
  decrement_incounter((node*)nullptr, target);
}
  
void add_node(node* n) {
  pasl::sched::threaddag::add_thread(n);
}
  
outset::insert_status_type outset_insert(node* source, outset* source_out, node* target) {
  long tag = pasl::sched::outstrategy::extract_tag(source_out);
  assert(tag != pasl::sched::outstrategy::NOOP_TAG);
  if (tag == pasl::sched::outstrategy::UNARY_TAG) {
    source->out = pasl::data::tagged::create<pasl::sched::thread_p, pasl::sched::outstrategy_p>(target, tag);
    return outset::insert_success;
  } else if (tag == pasl::sched::outstrategy::TOPDOWN_DISTRIBUTED_UNARY_TAG) {
    auto target_in = (distributed::distributed_incounter*)target->in;
    long tg = pasl::sched::instrategy::extract_tag(target_in);
    if ((tg == 0) && (edge_algorithm == edge_algorithm_distributed)) {
      auto leaf = target_in->nzi.random_leaf_of(source);
      auto t = (pasl::sched::thread_p)leaf;
      source->out = pasl::sched::outstrategy::topdown_distributed_unary_new(t);
    } else {
      tag = pasl::sched::outstrategy::UNARY_TAG;
      source->out = pasl::data::tagged::create<pasl::sched::thread_p, pasl::sched::outstrategy_p>(target, tag);
    }
    return outset::insert_success;
  } else {
    assert(tag == 0);
    return source_out->insert(target);
  }
}

void add_edge(node* source, outset* source_out, node* target, incounter* target_in) {
  increment_incounter(source, target, target_in);
  if (outset_insert(source, source_out, target) == outset::insert_fail) {
    decrement_incounter(source, target, target_in);
  }
}
  
void add_edge(node* source, node* target) {
  add_edge(source, (outset*)source->out, target, (incounter*)target->in);
}
  
void prepare_node(node* n) {
  prepare_node(n, incounter_new(n), outset_new());
}

void prepare_node(node* n, incounter* in) {
  prepare_node(n, in, outset_new());
}

void prepare_node(node* n, outset* out) {
  prepare_node(n, incounter_new(n), out);
}
  
void prepare_node(node* n, incounter* in, outset* out) {
  n->set_instrategy(in);
  n->set_outstrategy(out);
}

outset* capture_outset() {
  auto sched = pasl::sched::threaddag::my_sched();
  outset* out = (outset*)sched->get_outstrategy();
  assert(out != nullptr);
  sched->set_outstrategy(outset_noop());
  return out;
}

void join_with(node* n, incounter* in) {
  prepare_node(n, in, capture_outset());
}

void continue_with(node* n) {
  join_with(n, incounter_ready());
  add_node(n);
}
  
namespace dyntree {
  
void deallocate_incounter_tree_partial(std::deque<ictnode*>& todo) {
  int k = 0;
  while ( (k < communication_delay) && (! todo.empty()) ) {
    ictnode* current = todo.back();
    todo.pop_back();
    for (int i = 0; i < branching_factor; i++) {
      ictnode* child = tagged_pointer_of(current->children[i].load());
      if (child == nullptr) {
        continue;
      }
      todo.push_back(child);
    }
    delete current;
    k++;
  }
}
  
class deallocate_incounter_tree_par : public node {
public:
  
  using self_type = deallocate_incounter_tree_par;
  
  enum {
    process_block=0,
    repeat_block
  };
  
  std::deque<ictnode*> todo;
  
  void body() {
    switch (current_block_id) {
      case process_block: {
        deallocate_incounter_tree_partial(todo);
        jump_to(repeat_block);
        break;
      }
      case repeat_block: {
        if (! todo.empty()) {
          jump_to(process_block);
        }
        break;
      }
      default:
        break;
    }
  }
  
  size_t size() const {
    return todo.size();
  }
  
  pasl::sched::thread_p split() {
    assert(size() >= 2);
    auto n = todo.front();
    todo.pop_front();
    auto t = new self_type();
    t->todo.push_back(n);
    return t;
  }
  
};
  
void deallocate_incounter_tree(ictnode* root) {
  deallocate_incounter_tree_par d;
  d.todo.push_back(root);
  deallocate_incounter_tree_partial(d.todo);
  if (! d.todo.empty()) {
    node* n = new deallocate_incounter_tree_par(d);
    prepare_node(n);
    add_node(n);
  }
}
  
void notify_outset_tree_nodes_partial(std::deque<ostnode*>& todo) {
  int k = 0;
  while ( (k < communication_delay) && (! todo.empty()) ) {
    ostnode* current = todo.back();
    todo.pop_back();
    for (int i = 0; i < branching_factor; i++) {
      ostnode::tagged_pointer_type n = current->children[i].load();
      while (true) {
        ostnode::tagged_pointer_type orig = n;
        ostnode::tagged_pointer_type next = ostnode::make_finished(n);
        if (current->children[i].compare_exchange_strong(orig, next)) {
          break;
        }
      }
      int tag = tagged_tag_of(n.leaf);
      if (tag == ostnode::leaf) {
        decrement_incounter(tagged_pointer_of(n.leaf));
      } else if (tag == ostnode::interior) {
        todo.push_back(tagged_pointer_of(n.interior));
      }
    }
    k++;
  }
}
  
class notify_outset_tree_nodes_par_rec : public node {
public:
  
  using self_type = notify_outset_tree_nodes_par_rec;
  
  enum {
    process_block=0,
    repeat_block,
    exit_block
  };

  node* join;
  std::deque<ostnode*> todo;
  
  notify_outset_tree_nodes_par_rec(node* join, ostnode* n)
  : join(join) {
    todo.push_back(n);
  }
  
  notify_outset_tree_nodes_par_rec(node* join, std::deque<ostnode*>& _todo)
  : join(join) {
    _todo.swap(todo);
  }
  
  void body() {
    switch (current_block_id) {
      case process_block: {
        notify_outset_tree_nodes_partial(todo);
        jump_to(repeat_block);
        break;
      }
      case repeat_block: {
        if (! todo.empty()) {
          jump_to(process_block);
        }
        break;
      }
      default:
        break;
    }
  }
  
  size_t size() const {
    return todo.size();
  }
  
  pasl::sched::thread_p split() {
    assert(size() >= 2);
    auto n = todo.front();
    todo.pop_front();
    auto t = new self_type(join, n);
    add_edge(t, join);
    return t;
  }
  
};
  
class notify_outset_tree_nodes_par : public node {
public:
  
  using self_type = notify_outset_tree_nodes_par;
  
  enum {
    entry_block=0,
    exit_block
  };
  
  dyntree_outset* out;
  std::deque<ostnode*> todo;
  
  notify_outset_tree_nodes_par(dyntree_outset* out, std::deque<ostnode*>& _todo)
  : out(out) {
    todo.swap(_todo);
  }
  
  void body() {
    switch (current_block_id) {
      case entry_block: {
        finish(new notify_outset_tree_nodes_par_rec(this, todo),
               exit_block);
        break;
      }
      case exit_block: {
        if (out->should_deallocate_automatically) {
          delete out;
        }
        break;
      }
      default:
        break;
    }
  }
  
};
  
void notify_outset_nodes(dyntree_outset* out) {
  std::deque<ostnode*> todo;
  todo.push_back(out->root);
  notify_outset_tree_nodes_partial(todo);
  if (! todo.empty()) {
    auto n = new notify_outset_tree_nodes_par(out, todo);
    prepare_node(n);
    add_node(n);
  } else {
    if (out->should_deallocate_automatically) {
      delete out;
    }
  }
}
  
void deallocate_outset_tree_partial(std::deque<ostnode*>& todo) {
  int k = 0;
  while ( (k < communication_delay) && (! todo.empty()) ) {
    ostnode* n = todo.back();
    todo.pop_back();
    for (int i = 0; i < branching_factor; i++) {
      ostnode::tagged_pointer_type c = n->children[i].load();
      int tag = tagged_tag_of(c.interior);
      if (   (tag == ostnode::finished_empty)
          || (tag == ostnode::finished_leaf) ) {
        // nothing to do
      } else if (tag == ostnode::finished_interior) {
        todo.push_back(tagged_pointer_of(c.interior));
      } else {
        // should not occur, given that finished() has been called
        assert(false);
      }
    }
    delete n;
    k++;
  }
}
  
class deallocate_outset_tree_par : public node {
public:
  
  using self_type = deallocate_outset_tree_par;
  
  enum {
    process_block=0,
    repeat_block
  };
  
  std::deque<ostnode*> todo;
  
  void body() {
    switch (current_block_id) {
      case process_block: {
        deallocate_outset_tree_partial(todo);
        jump_to(repeat_block);
        break;
      }
      case repeat_block: {
        if (! todo.empty()) {
          jump_to(process_block);
        }
        break;
      }
      default:
        break;
    }
  }
  
  size_t size() const {
    return todo.size();
  }
  
  pasl::sched::thread_p split() {
    assert(size() >= 2);
    auto n = todo.front();
    todo.pop_front();
    auto t = new self_type();
    t->todo.push_back(n);
    return t;
  }
  
};
  
void deallocate_outset_tree(ostnode* root) {
  deallocate_outset_tree_par d;
  d.todo.push_back(root);
  deallocate_outset_tree_partial(d.todo);
  if (! d.todo.empty()) {
    node* n = new deallocate_outset_tree_par(d);
    prepare_node(n);
    add_node(n);
  }
}
  
} // end namespace
  
} // end namespace

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/* The bottom-up algorithm */

namespace bottomup {
  
class node;
class incounter;
class outset;
class ictnode;
class ostnode;
  
using inport_map_type = std::map<incounter*, ictnode*>;
using outport_map_type = std::map<outset*, ostnode*>;
  
void prepare_node(node*);
void prepare_node(node*, incounter*);
void prepare_node(node*, outset*);
void prepare_node(node*, incounter*, outset*);
incounter* incounter_ready();
incounter* incounter_unary();
incounter* incounter_fetch_add();
incounter* incounter_new(node*);
outset* outset_unary(node*);
outset* outset_noop();
outset* outset_new(node*);
void add_node(node* n);
void create_fresh_ports(node*, node*);
outset* capture_outset();
void join_with(node*, incounter*);
void continue_with(node*);
ictnode* increment_incounter(node*);
std::pair<ictnode*, ictnode*> increment_incounter(node*, ictnode*);
std::pair<ictnode*, ictnode*> increment_incounter(node*, node*);
void decrement_incounter(node*, ictnode*);
void decrement_incounter(node*, incounter*, ictnode*);
void decrement_inports(node*);
void insert_inport(node*, incounter*, ictnode*);
void insert_inport(node*, node*, ictnode*);
void insert_outport(node*, outset*, ostnode*);
void insert_outport(node*, node*, ostnode*);
void deallocate_future(node*, outset*);
void notify_outset_tree_nodes(outset*);
void deallocate_outset_tree(ostnode*);
  
class ictnode {
public:
  
  ictnode* parent;
  std::atomic<int> nb_removed_children;
  
  ictnode() {
    parent = nullptr;
    nb_removed_children.store(0);
  }
  
};

class ostnode {
public:
  
  node* target;
  ictnode* port;
  std::atomic<ostnode*> children[2];
  
  ostnode() {
    target = nullptr;
    port = nullptr;
    for (int i = 0; i < 2; i++) {
      children[i].store(nullptr);
    }
  }
  
};

class incounter : public pasl::sched::instrategy::common {
public:
  
  using status_type = enum {
    activated,
    not_activated
  };
  
  node* n;
  
  incounter(node* n)
  : n(n) {
    assert(n != nullptr);
  }
  
  bool is_activated(ictnode* port) const {
    return port->parent == nullptr;
  }
  
  std::pair<ictnode*, ictnode*> increment(ictnode* port) const {
    ictnode* branch1;
    ictnode* branch2;
    if (port == nullptr) {
      branch1 = new ictnode;
      branch2 = nullptr;
    } else {
      branch1 = new ictnode;
      branch2 = new ictnode;
      branch1->parent = port;
      branch2->parent = port;
    }
    return std::make_pair(branch1, branch2);
  }
  
  ictnode* increment() const {
    return increment((ictnode*)nullptr).first;
  }
  
  status_type decrement(ictnode* port) const {
    assert(port != nullptr);
    ictnode* current = port;
    ictnode* next = current->parent;
    while (next != nullptr) {
      delete current;
      while (true) {
        if (next->nb_removed_children.load() != 0) {
          break;
        }
        int orig = 0;
        if (next->nb_removed_children.compare_exchange_strong(orig, 1)) {
          return not_activated;
        }
      }
      current = next;
      next = next->parent;
    }
    assert(current != nullptr);
    assert(next == nullptr);
    delete current;
    return activated;
  }
  
  void check(pasl::sched::thread_p t) {
    assert(false);
  }
  
  void delta(pasl::sched::thread_p t, int64_t d) {
    assert(false);
  }
  
};

class outset : public pasl::sched::outstrategy::common {
public:
  
  using insert_status_type = enum {
    insert_success,
    insert_fail
  };
  
  using insert_result_type = std::pair<insert_status_type, ostnode*>;
  
  static constexpr int frozen_tag = 1;
  
  ostnode* root;
  
  node* n;
  
  outset(node* n)
  : n(n) {
    root = new ostnode;
  }
  
  ~outset() {
    deallocate_outset_tree(root);
  }
  
  bool is_finished() const {
    int tag = tagged_tag_of(root->children[0].load());
    return tag == frozen_tag;
  }
  
  insert_result_type insert(ostnode* outport, node* target, ictnode* inport) {
    if (is_finished()) {
      return std::make_pair(insert_fail, nullptr);
    }
    ostnode* next = new ostnode;
    next->target = target;
    next->port = inport;
    ostnode* orig = nullptr;
    if (! (outport->children[0].compare_exchange_strong(orig, next))) {
      delete next;
      return std::make_pair(insert_fail, nullptr);
    }
    return std::make_pair(insert_success, next);
  }
  
  std::pair<ostnode*, ostnode*> fork2(ostnode* port) const {
    assert(port != nullptr);
    ostnode* branches[2];
    branches[0] = new ostnode;
    branches[1] = new ostnode;
    for (int i = 1; i >= 0; i--) {
      ostnode* orig = nullptr;
      if (! port->children[i].compare_exchange_strong(orig, branches[i])) {
        for (int j = i; j >= 0; j--) {
          delete branches[j];
          branches[j] = port;
        }
        break;
      }
    }
    return std::make_pair(branches[0], branches[1]);
  }
  
  void add(pasl::sched::thread_p t) {
    assert(false);
  }
  
  void finished() {
    if (n != nullptr) {
      decrement_inports(n);
    }
    notify_outset_tree_nodes(this);
  }
  
  bool should_deallocate_automatically = true;
  
  void enable_future() {
    should_deallocate_automatically = false;
  }
  
};
  
outset::insert_result_type insert_outedge(node*,
                                          outset*,
                                          node*, ictnode*);
  
class node : public pasl::sched::thread {
public:
  
  using incounter_type = incounter;
  using outset_type = outset;
  
  const int uninitialized_block_id = -1;
  const int entry_block_id = 0;
  
  int current_block_id;
  
private:
  
  int continuation_block_id;
  
public:
  
  inport_map_type inports;
  outport_map_type outports;
  
  node()
  : current_block_id(uninitialized_block_id),
    continuation_block_id(entry_block_id) {
  }
  
  virtual void body() = 0;
  
  void run() {
    current_block_id = continuation_block_id;
    continuation_block_id = uninitialized_block_id;
    assert(current_block_id != uninitialized_block_id);
    body();
  }
  
  void decrement_inports() {
    for (auto p : inports) {
      incounter* in = p.first;
      ictnode* in_port = p.second;
      node* n_in = in->n;
      decrement_incounter(n_in, in, in_port);
    }
    inports.clear();
  }
  
  void prepare_for_transfer(int target) {
    pasl::sched::threaddag::reuse_calling_thread();
    continuation_block_id = target;
  }
  
  void jump_to(int continuation_block_id) {
    prepare_for_transfer(continuation_block_id);
    continue_with(this);
  }
  
  void async(node* producer, node* consumer, int continuation_block_id) {
    prepare_node(producer, incounter_ready(), outset_unary(producer));
    node* caller = this;
    insert_inport(producer, (incounter*)consumer->in, (ictnode*)nullptr);
    create_fresh_ports(caller, producer);
    caller->jump_to(continuation_block_id);
    add_node(producer);
  }
  
  void finish(node* producer, int continuation_block_id) {
    prepare_node(producer, incounter_ready(), outset_unary(producer));
    node* consumer = this;
    join_with(consumer, new incounter(consumer));
    create_fresh_ports(consumer, producer);
    ictnode* consumer_inport = increment_incounter(consumer);
    insert_inport(producer, consumer, consumer_inport);
    consumer->prepare_for_transfer(continuation_block_id);
    add_node(producer);
  }
  
  outset* future(node* producer, int continuation_block_id) {
    prepare_node(producer, incounter_ready());
    outset* producer_out = (outset*)producer->out;
    producer_out->enable_future();
    node* caller = this;
    create_fresh_ports(caller, producer);
    insert_outport(caller, producer, producer_out->root);
    caller->jump_to(continuation_block_id);
    add_node(producer);
    return producer_out;
  }
  
  void force(outset* producer_out, int continuation_block_id) {
    node* consumer = this;
    prepare_for_transfer(continuation_block_id);
    join_with(consumer, incounter_unary());
    auto insert_result = insert_outedge(consumer, producer_out, consumer, (ictnode*)nullptr);
    outset::insert_status_type insert_status = insert_result.first;
    if (insert_status == outset::insert_success) {
      ostnode* producer_outport = insert_result.second;
      insert_outport(consumer, producer_out, producer_outport);
    } else if (insert_status == outset::insert_fail) {
      add_node(consumer);
    } else {
      assert(false);
    }
  }
  
  void deallocate_future(outset* future) {
    bottomup::deallocate_future(this, future);
  }
  
  void call(node* target, int continuation_block_id) {
    finish(target, continuation_block_id);
  }
  
  THREAD_COST_UNKNOWN
  
};
  
void prepare_node(node* n) {
  prepare_node(n, incounter_new(n), outset_new(n));
}

void prepare_node(node* n, incounter* in) {
  prepare_node(n, in, outset_new(n));
}

void prepare_node(node* n, outset* out) {
  prepare_node(n, incounter_new(n), out);
}

void prepare_node(node* n, incounter* in, outset* out) {
  n->set_instrategy(in);
  n->set_outstrategy(out);
}
  
incounter* incounter_ready() {
  return (incounter*)pasl::sched::instrategy::ready_new();
}

incounter* incounter_unary() {
  return (incounter*)pasl::sched::instrategy::unary_new();
}

incounter* incounter_fetch_add() {
  return (incounter*)pasl::sched::instrategy::fetch_add_new();
}
  
incounter* incounter_new(node* n) {
  return new incounter(n);
}

outset* outset_unary(node* n) {
  return (outset*)pasl::sched::outstrategy::bottomup_unary_new((pasl::sched::thread_p)n);
}

outset* outset_noop() {
  return (outset*)pasl::sched::outstrategy::noop_new();
}
  
outset* outset_new(node* n) {
  return new outset(n);
}
  
void insert_inport(node* caller, incounter* target_in, ictnode* target_inport) {
  caller->inports.insert(std::make_pair(target_in, target_inport));
}
  
void insert_inport(node* caller, node* target, ictnode* target_inport) {
  insert_inport(caller, (incounter*)target->in, target_inport);
}

void insert_outport(node* caller, outset* target_out, ostnode* target_outport) {
  assert(target_outport != nullptr);
  caller->outports.insert(std::make_pair(target_out, target_outport));
}
  
void insert_outport(node* caller, node* target, ostnode* target_outport) {
  insert_outport(caller, (outset*)target->out, target_outport);
}

ictnode* find_inport(node* caller, incounter* target_in) {
  auto target_inport_result = caller->inports.find(target_in);
  assert(target_inport_result != caller->inports.end());
  return target_inport_result->second;
}

ostnode* find_outport(node* caller, outset* target_out) {
  auto target_outport_result = caller->outports.find(target_out);
  assert(target_outport_result != caller->outports.end());
  return target_outport_result->second;
}

void create_fresh_inports(node* source, node* target) {
  inport_map_type source_ports = source->inports;
  inport_map_type target_ports;
  for (auto p : source->inports) {
    if (target->inports.find(p.first) != target->inports.cend()) {
      source_ports.erase(p.first);
      incounter* in = p.first;
      auto ports = in->increment(p.second);
      source_ports.insert(std::make_pair(p.first, ports.first));
      target_ports.insert(std::make_pair(p.first, ports.second));
    }
  }
  source->inports.swap(source_ports);
  target->inports.swap(target_ports);
}

void create_fresh_outports(node* source, node* target) {
  outport_map_type source_ports = source->outports;
  outport_map_type target_ports;
  for (auto p : source->outports) {
    source_ports.erase(p.first);
    outset* out = p.first;
    auto ports = out->fork2(p.second);
    source_ports.insert(std::make_pair(p.first, ports.first));
    target_ports.insert(std::make_pair(p.first, ports.second));
  }
  source->outports.swap(source_ports);
  target->outports.swap(target_ports);
}
  
void create_fresh_ports(node* source, node* target) {
  create_fresh_inports(source, target);
  create_fresh_outports(source, target);
}
  
ictnode* increment_incounter(node* n) {
  incounter* in = (incounter*)n->in;
  return in->increment();
}
  
std::pair<ictnode*, ictnode*> increment_incounter(node* n, ictnode* n_port) {
  incounter* n_in = (incounter*)n->in;
  long tag = pasl::sched::instrategy::extract_tag(n_in);
  assert(tag != pasl::sched::instrategy::READY_TAG);
  if (tag == pasl::sched::instrategy::UNARY_TAG) {
    // nothing to do
    return std::make_pair(nullptr, nullptr);
  } else if (tag == pasl::sched::instrategy::FETCH_ADD_TAG) {
    pasl::data::tagged::atomic_fetch_and_add<pasl::sched::instrategy_p>(&(n->in), 1l);
    return std::make_pair(nullptr, nullptr);
  } else {
    incounter* in = (incounter*)n->in;
    return in->increment(n_port);
  }
}
  
std::pair<ictnode*, ictnode*> increment_incounter(node* caller, node* target) {
  ictnode* target_inport = find_inport(caller, (incounter*)target->in);
  return increment_incounter(target, target_inport);
}

void decrement_incounter(node* n, incounter* n_in, ictnode* n_port) {
  long tag = pasl::sched::instrategy::extract_tag(n_in);
  assert(tag != pasl::sched::instrategy::READY_TAG);
  if (tag == pasl::sched::instrategy::UNARY_TAG) {
    pasl::sched::instrategy::schedule(n);
  } else if (tag == pasl::sched::instrategy::FETCH_ADD_TAG) {
    int64_t old = pasl::data::tagged::atomic_fetch_and_add<pasl::sched::instrategy_p>(&(n->in), -1l);
    if (old == 1) {
      pasl::sched::instrategy::schedule(n);
    }
  } else {
    incounter::status_type status = n_in->decrement(n_port);
    if (status == incounter::activated) {
      n_in->start(n);
    }
  }
}
  
void decrement_incounter(node* n, ictnode* n_port) {
  decrement_incounter(n, (incounter*)n->in, n_port);
}
  
void decrement_inports(node* n) {
  n->decrement_inports();
}
  
outset::insert_result_type insert_outedge(node* caller,
                                          outset* source_out,
                                          node* target, ictnode* target_inport) {
  auto source_outport = find_outport(caller, source_out);
  return source_out->insert(source_outport, target, target_inport);
}
  
void add_node(node* n) {
  incounter* n_in = (incounter*)n->in;
  long tag = pasl::sched::instrategy::extract_tag(n_in);
  if (   (tag == pasl::sched::instrategy::UNARY_TAG)
      || (tag == pasl::sched::instrategy::READY_TAG)) {
    // nothing to do
  } else if (tag == pasl::sched::instrategy::FETCH_ADD_TAG) {
    // nothing to do
  } else {
    delete n->in;
  }
  pasl::sched::instrategy::schedule(n);
}
  
outset* capture_outset() {
  auto sched = pasl::sched::threaddag::my_sched();
  outset* out = (outset*)sched->get_outstrategy();
  assert(out != nullptr);
  sched->set_outstrategy(outset_noop());
  return out;
}
  
void join_with(node* n, incounter* in) {
  prepare_node(n, in, capture_outset());
}

void continue_with(node* n) {
  join_with(n, incounter_ready());
  add_node(n);
}
  
void bottomup_finished(pasl::sched::thread_p t) {
  node* n = tagged_pointer_of((node*)t);
  n->decrement_inports();
}
  
void deallocate_future(node* caller, outset* future) {
  assert(! future->should_deallocate_automatically);
  assert(caller->outports.find(future) != caller->outports.end());
  caller->outports.erase(future);
  delete future;
}
  
void notify_outset_tree_nodes_partial(std::deque<ostnode*>& todo) {
  int k = 0;
  while ( (k < communication_delay) && (! todo.empty()) ) {
    ostnode* n = todo.back();
    todo.pop_back();
    if (n->target != nullptr) {
      decrement_incounter(n->target, n->port);
    }
    for (int i = 0; i < 2; i++) {
      ostnode* orig;
      while (true) {
        orig = n->children[i].load();
        ostnode* next = tagged_tag_with((ostnode*)nullptr, outset::frozen_tag);
        if (n->children[i].compare_exchange_strong(orig, next)) {
          break;
        }
      }
      if (orig != nullptr) {
        todo.push_back(orig);
      }
    }
    k++;
  }
}
  
class notify_outset_tree_nodes_par_rec : public node {
public:
  
  using self_type = notify_outset_tree_nodes_par_rec;
  
  enum {
    process_block=0,
    repeat_block,
    exit_block
  };
  
  node* join;
  std::deque<ostnode*> todo;
  
  notify_outset_tree_nodes_par_rec(node* join, ostnode* n)
  : join(join) {
    todo.push_back(n);
  }
  
  notify_outset_tree_nodes_par_rec(node* join, std::deque<ostnode*>& _todo)
  : join(join) {
    _todo.swap(todo);
  }
  
  void body() {
    switch (current_block_id) {
      case process_block: {
        notify_outset_tree_nodes_partial(todo);
        jump_to(repeat_block);
        break;
      }
      case repeat_block: {
        if (! todo.empty()) {
          jump_to(process_block);
        }
        break;
      }
      default:
        break;
    }
  }
  
  size_t size() const {
    return todo.size();
  }
  
  pasl::sched::thread_p split() {
    assert(size() >= 2);
    auto n = todo.front();
    todo.pop_front();
    node* consumer = join;
    node* caller = this;
    auto producer = new self_type(join, n);
    prepare_node(producer);
    insert_inport(producer, (incounter*)consumer->in, (ictnode*)nullptr);
    create_fresh_ports(caller, producer);
    return producer;
  }
  
};
  
class notify_outset_tree_nodes_par : public node {
public:
  
  using self_type = notify_outset_tree_nodes_par;
  
  enum {
    entry_block=0,
    exit_block
  };
  
  outset* out;
  std::deque<ostnode*> todo;
  
  notify_outset_tree_nodes_par(outset* out, std::deque<ostnode*>& _todo)
  : out(out) {
    todo.swap(_todo);
  }
  
  void body() {
    switch (current_block_id) {
      case entry_block: {
        finish(new notify_outset_tree_nodes_par_rec(this, todo),
               exit_block);
        break;
      }
      case exit_block: {
        if (out->should_deallocate_automatically) {
          delete out;
        }
        break;
      }
      default:
        break;
    }
  }
  
};
  
void notify_outset_tree_nodes(outset* out) {
  std::deque<ostnode*> todo;
  todo.push_back(out->root);
  notify_outset_tree_nodes_partial(todo);
  if (! todo.empty()) {
    auto n = new notify_outset_tree_nodes_par(out, todo);
    prepare_node(n);
    add_node(n);
  } else {
    if (out->should_deallocate_automatically) {
      delete out;
    }
  }
}
  
void deallocate_outset_tree_partial(std::deque<ostnode*>& todo) {
  int k = 0;
  while ( (k < communication_delay) && (! todo.empty()) ) {
    ostnode* n = todo.back();
    todo.pop_back();
    for (int i = 0; i < 2; i++) {
      ostnode* child = tagged_pointer_of(n->children[i].load());
      if (child != nullptr) {
        todo.push_back(child);
      }
    }
    delete n;
    k++;
  }
}

class deallocate_outset_tree_par : public node {
public:
  
  using self_type = deallocate_outset_tree_par;
  
  enum {
    process_block=0,
    repeat_block
  };
  
  std::deque<ostnode*> todo;
  
  void body() {
    switch (current_block_id) {
      case process_block: {
        deallocate_outset_tree_partial(todo);
        jump_to(repeat_block);
        break;
      }
      case repeat_block: {
        if (! todo.empty()) {
          jump_to(process_block);
        }
        break;
      }
      default:
        break;
    }
  }
  
  size_t size() const {
    return todo.size();
  }
  
  pasl::sched::thread_p split() {
    assert(size() >= 2);
    auto n = todo.front();
    todo.pop_front();
    auto t = new self_type();
    prepare_node(t);
    t->todo.push_back(n);
    return t;
  }
  
};
  
void deallocate_outset_tree(ostnode* root) {
  deallocate_outset_tree_par d;
  d.todo.push_back(root);
  deallocate_outset_tree_partial(d.todo);
  if (! d.todo.empty()) {
    node* n = new deallocate_outset_tree_par(d);
    prepare_node(n);
    add_node(n);
  }
}
  
} // end namespace

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/* Test functions */

namespace tests {
  
template <class node>
using outset_of = typename node::outset_type;
  
std::atomic<int> async_leaf_counter;
std::atomic<int> async_interior_counter;

template <class node>
class async_loop_rec : public node {
public:
  
  enum {
    async_loop_rec_entry,
    async_loop_rec_mid,
    async_loop_rec_exit
  };
  
  int lo;
  int hi;
  node* consumer;
  
  int mid;
  
  async_loop_rec(int lo, int hi, node* consumer)
  : lo(lo), hi(hi), consumer(consumer) { }
  
  void body() {
    switch (node::current_block_id) {
      case async_loop_rec_entry: {
        int n = hi - lo;
        if (n == 0) {
          return;
        } else if (n == 1) {
          async_leaf_counter.fetch_add(1);
        } else {
          async_interior_counter.fetch_add(1);
          mid = (lo + hi) / 2;
          node::async(new async_loop_rec(lo, mid, consumer), consumer,
                      async_loop_rec_mid);
        }
        break;
      }
      case async_loop_rec_mid: {
        node::async(new async_loop_rec(mid, hi, consumer), consumer,
                    async_loop_rec_exit);
        break;
      }
      case async_loop_rec_exit: {
        break;
      }
      default:
        break;
    }
  }
  
};

template <class node>
class async_loop : public node {
public:
  
  enum {
    async_loop_entry,
    async_loop_exit
  };
  
  int n;
  
  async_loop(int n)
  : n(n) { }
  
  void body() {
    switch (node::current_block_id) {
      case async_loop_entry: {
        async_leaf_counter.store(0);
        async_interior_counter.store(0);
        node::finish(new async_loop_rec<node>(0, n, this),
                     async_loop_exit);
        break;
      }
      case async_loop_exit: {
        assert(async_leaf_counter.load() == n);
        assert(async_interior_counter.load() + 1 == n);
        break;
      }
      default:
        break;
    }
  }
  
};

std::atomic<int> future_leaf_counter;
std::atomic<int> future_interior_counter;

template <class node>
class future_loop_rec : public node {
public:
  
  enum {
    future_loop_entry,
    future_loop_branch2,
    future_loop_force1,
    future_loop_force2,
    future_loop_exit
  };
  
  int lo;
  int hi;
  
  outset_of<node>* branch1_out;
  outset_of<node>* branch2_out;
  
  int mid;
  
  future_loop_rec(int lo, int hi)
  : lo(lo), hi(hi) { }
  
  void body() {
    switch (node::current_block_id) {
      case future_loop_entry: {
        int n = hi - lo;
        if (n == 0) {
          return;
        } else if (n == 1) {
          future_leaf_counter.fetch_add(1);
        } else {
          mid = (lo + hi) / 2;
          node* branch1 = new future_loop_rec<node>(lo, mid);
          branch1_out = node::future(branch1,
                                     future_loop_branch2);
        }
        break;
      }
      case future_loop_branch2: {
        node* branch2 = new future_loop_rec<node>(mid, hi);
        branch2_out = node::future(branch2,
                                   future_loop_force1);
        break;
      }
      case future_loop_force1: {
        node::force(branch1_out,
                    future_loop_force2);
        break;
      }
      case future_loop_force2: {
        node::force(branch2_out,
                    future_loop_exit);
        break;
      }
      case future_loop_exit: {
        future_interior_counter.fetch_add(1);
        node::deallocate_future(branch1_out);
        node::deallocate_future(branch2_out);
        break;
      }
      default:
        break;
    }
  }
  
};

template <class node>
class future_loop : public node {
public:
  
  enum {
    future_loop_entry,
    future_loop_force,
    future_loop_exit
  };
  
  int n;
  
  outset_of<node>* root_out;
  
  future_loop(int n)
  : n(n) { }
  
  void body() {
    switch (node::current_block_id) {
      case future_loop_entry: {
        future_leaf_counter.store(0);
        future_interior_counter.store(0);
        node* root = new future_loop_rec<node>(0, n);
        root_out = node::future(root,
                                future_loop_force);
        break;
      }
      case future_loop_force: {
        node::force(root_out,
                    future_loop_exit);
        break;
      }
      case future_loop_exit: {
        node::deallocate_future(root_out);
        assert(future_leaf_counter.load() == n);
        assert(future_interior_counter.load() + 1 == n);
        break;
      }
      default:
        break;
    }
  }
  
};

template <class Body_gen, class node>
class parallel_for_rec : public node {
public:
  
  enum {
    parallel_for_rec_entry,
    parallel_for_rec_branch2,
    parallel_for_rec_exit
  };
  
  int lo;
  int hi;
  Body_gen body_gen;
  node* join;
  
  int mid;
  
  parallel_for_rec(int lo, int hi, Body_gen body_gen, node* join)
  : lo(lo), hi(hi), body_gen(body_gen), join(join) { }
  
  void body() {
    switch (node::current_block_id) {
      case parallel_for_rec_entry: {
        int n = hi - lo;
        if (n == 0) {
          // nothing to do
        } else if (n == 1) {
          node::call(body_gen(lo),
                     parallel_for_rec_exit);
        } else {
          mid = (hi + lo) / 2;
          node::async(new parallel_for_rec(lo, mid, body_gen, join), join,
                      parallel_for_rec_branch2);
        }
        break;
      }
      case parallel_for_rec_branch2: {
        node::async(new parallel_for_rec(mid, hi, body_gen, join), join,
                    parallel_for_rec_exit);
        break;
      }
      case parallel_for_rec_exit: {
        // nothing to do
        break;
      }
      default:
        break;
    }
  }
  
};

template <class Body_gen, class node>
class parallel_for : public node {
public:
  
  enum {
    parallel_for_entry,
    parallel_for_exit
  };
  
  int lo;
  int hi;
  Body_gen body_gen;
  
  parallel_for(int lo, int hi, Body_gen body_gen)
  : lo(lo), hi(hi), body_gen(body_gen) { }
  
  void body() {
    switch (node::current_block_id) {
      case parallel_for_entry: {
        node::finish(new parallel_for_rec<Body_gen, node>(lo, hi, body_gen, this),
                     parallel_for_exit);
        break;
      }
      case parallel_for_exit: {
        // nothing to do
        break;
      }
      default:
        break;
    }
  }
  
};

std::atomic<int> future_pool_leaf_counter;
std::atomic<int> future_pool_interior_counter;

static long fib (long n){
  if (n < 2)
    return n;
  else
    return fib (n - 1) + fib (n - 2);
}

int fib_input = 22;

long fib_result;

template <class node>
class future_body : public node {
public:
  
  enum {
    future_body_entry,
    future_body_exit
  };
  
  void body() {
    switch (node::current_block_id) {
      case future_body_entry: {
        fib_result = fib(fib_input);
        break;
      }
      case future_body_exit: {
        break;
      }
      default:
        break;
    }
  }
  
};

std::atomic<int> future_pool_counter;

template <class node>
class future_reader : public node {
public:
  
  enum {
    future_reader_entry,
    future_reader_exit
  };
  
  outset_of<node>* f;
  
  int i;
  
  future_reader(outset_of<node>* f, int i)
  : f(f), i(i) { }
  
  void body() {
    switch (node::current_block_id) {
      case future_reader_entry: {
        node::force(f,
                    future_reader_exit);
        break;
      }
      case future_reader_exit: {
        future_pool_counter.fetch_add(1);
        assert(fib_result == fib(fib_input));
        break;
      }
      default:
        break;
    }
  }
  
};

template <class node>
class future_pool : public node {
public:
  
  enum {
    future_pool_entry,
    future_pool_call,
    future_pool_exit
  };
  
  int n;
  
  outset_of<node>* f;
  
  future_pool(int n)
  : n(n) { }
  
  void body() {
    switch (node::current_block_id) {
      case future_pool_entry: {
        node* fut = new future_body<node>;
        f = node::future(fut,
                         future_pool_call);
        break;
      }
      case future_pool_call: {
        auto loop_body = [=] (int i) {
          return new future_reader<node>(f, i);
        };
        node::call(new parallel_for<decltype(loop_body), node>(0, n, loop_body),
                   future_pool_exit);
        break;
      }
      case future_pool_exit: {
        node::deallocate_future(f);
        assert(future_pool_counter.load() == n);
        break;
      }
      default:
        break;
    }
  }
  
};

int nb_levels(int n) {
  assert(n >= 1);
  return 2 * (n - 1) + 1;
}

int nb_cells_in_level(int n, int l) {
  assert((l >= 1) && (l <= nb_levels(n)));
  return (l <= n) ? l : (nb_levels(n) + 1) - l;
}

std::pair<int, int> index_of_cell_at_pos(int n, int l, int pos) {
  assert((pos >= 0) && (pos < nb_cells_in_level(n, l)));
  int i;
  int j;
  if (l <= n) { // either on or above the diagonal
    i = pos;
    j = l - (pos + 1);
  } else {      // below the diagonal
    i = (l - n) + pos;
    j = n - (pos + 1);
  }
  return std::make_pair(i, j);
}

int row_major_index_of(int n, int i, int j) {
  return i * n + j;
}

template <class Item>
Item& row_major_address_of(Item* items, int n, int i, int j) {
  assert(i >= 0 && i < n);
  assert(j >= 0 && j < n);
  return items[row_major_index_of(n, i, j)];
}

template <class Item>
class matrix_type {
public:
  
  using value_type = Item;
  
  class Deleter {
  public:
    void operator()(value_type* ptr) {
      free(ptr);
    }
  };
  
  std::unique_ptr<value_type[], Deleter> items;
  int n;
  
  void fill(value_type val) {
    for (int i = 0; i < n*n; i++) {
      items[i] = val;
    }
  }
  
  void init() {
    value_type* ptr = (value_type*)malloc(sizeof(value_type) * (n*n));
    assert(ptr != nullptr);
    items.reset(ptr);
  }
  
  matrix_type(int n, value_type val)
  : n(n) {
    init();
    fill(val);
  }
  
  matrix_type(int n)
  : n(n) {
    init();
  }
  
  value_type& subscript(std::pair<int, int> pos) const {
    return subscript(pos.first, pos.second);
  }
  
  value_type& subscript(int i, int j) const {
    return row_major_address_of(&items[0], n, i, j);
  }
  
};

template <class Item>
std::ostream& operator<<(std::ostream& out, const matrix_type<Item>& xs) {
  out << "{\n";
  for (int i = 0; i < xs.n; i++) {
    out << "{ ";
    for (int j = 0; j < xs.n; j++) {
      if (j+1 < xs.n) {
        out << xs.subscript(i, j) << ",\t";
      } else {
        out << xs.subscript(i, j);
      }
    }
    out << " }\n";
  }
  out << "}\n";
  return out;
}

template <class Item, class Body>
void matrix_apply_to_each_cell(matrix_type<Item>& mtx, Body body) {
  int n = mtx.n;
  int xx = nb_levels(n);
  for (int l = 1; l <= xx; l++) {
    int yy = nb_cells_in_level(n, l);
    for (int i = 0; i < yy; i++) {
      auto idx = index_of_cell_at_pos(n, l, i);
      body(idx.first, idx.second, mtx.subscript(idx));
    }
  }
}

bool check(int n) {
  int val = 123;
  matrix_type<int> orig(n, val);
  matrix_type<int> test(n, 0);
  for (int i = 0; i < n*n; i++) {
    orig.items[i] = i;
  }
  matrix_apply_to_each_cell(test, [&] (int i,int j,int& x) {
    x = i*n+j;
  });
  std::cout << orig << std::endl;
  std::cout << test << std::endl;
  bool result = true;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      if (orig.subscript(i, j) != test.subscript(i, j)) {
        result = false;
      }
    }
  }
  return result;
}

void gauss_seidel_block(int N, double* a, int block_size) {
  for (int i = 1; i <= block_size; i++) {
    for (int j = 1; j <= block_size; j++) {
      a[i*N+j] = 0.2 * (a[i*N+j] + a[(i-1)*N+j] + a[(i+1)*N+j] + a[i*N+j-1] + a[i*N+j+1]);
    }
  }
}

void gauss_seidel_sequential(int numiters, int N, int M, int block_size, double* data) {
  for (int iter = 0; iter < numiters; iter++) {
    for (int i = 0; i < N; i += block_size) {
      for (int j = 0; j < N; j += block_size) {
        gauss_seidel_block(M, &data[M * i + j], block_size);
      }
    }
  }
}
  
template <class node>
class gauss_seidel_sequential_node : public node {
public:
  
  enum {
    gauss_seidel_sequential_node_entry,
    gauss_seidel_sequential_node_exit
  };
  
  int numiters; int N; int M; int block_size; double* data;

  gauss_seidel_sequential_node(int numiters, int N, int M, int block_size, double* data)
  : numiters(numiters), N(N), M(M), block_size(block_size), data(data) { }
  
  void body() {
    switch (node::current_block_id) {
      case gauss_seidel_sequential_node_entry: {
        gauss_seidel_sequential(numiters, N, M, block_size, data);
        break;
      }
      case gauss_seidel_sequential_node_exit: {
        break;
      }
    }
  }
  
};
  
template <class node>
using futures_matrix_type = matrix_type<outset_of<node>*>;

template <class node>
class gauss_seidel_loop_future_body : public node {
public:
  
  enum {
    gauss_seidel_loop_future_body_entry,
    gauss_seidel_loop_future_body_force,
    gauss_seidel_loop_future_body_exit
  };
  
  futures_matrix_type<node>* futures;
  int i, j;
  int M; int block_size; double* data;
  
  gauss_seidel_loop_future_body(futures_matrix_type<node>* futures, int i, int j,
                                int M, int block_size, double* data)
  : futures(futures), i(i), j(j),
  M(M), block_size(block_size), data(data) { }
  
  void body() {
    switch (node::current_block_id) {
      case gauss_seidel_loop_future_body_entry: {
        node::force(futures->subscript(i, j - 1),
                    gauss_seidel_loop_future_body_force);
        break;
      }
      case gauss_seidel_loop_future_body_force: {
        node::force(futures->subscript(i - 1, j),
                    gauss_seidel_loop_future_body_exit);
        break;
      }
      case gauss_seidel_loop_future_body_exit: {
        int ii = i * block_size;
        int jj = j * block_size;
        gauss_seidel_block(M, &data[M * ii + jj], block_size);
        break;
      }
    }
  }
  
};

template <class node>
class gauss_seidel_loop_body : public node {
public:
  
  enum {
    gauss_seidel_loop_body_entry,
    gauss_seidel_loop_body_exit
  };
  
  futures_matrix_type<node>* futures;
  int i, j;
  int M; int block_size; double* data;
  
  gauss_seidel_loop_body(futures_matrix_type<node>* futures, int i, int j,
                         int M, int block_size, double* data)
  : futures(futures), i(i), j(j),
  M(M), block_size(block_size), data(data) { }
  
  void body() {
    switch (node::current_block_id) {
      case gauss_seidel_loop_body_entry: {
        node* f = new gauss_seidel_loop_future_body<node>(futures, i, j, M, block_size, data);
        outset_of<node>*& cell = futures->subscript(i, j);
        cell = node::future(f,
                            gauss_seidel_loop_body_entry);
        break;
      }
      case gauss_seidel_loop_body_exit: {
        break;
      }
    }
  }
  
};

template <class node>
class gauss_seidel_parallel : public node {
public:
  
  enum {
    gauss_seidel_parallel_entry,
    gauss_seidel_parallel_iter_loop_body,
    gauss_seidel_parallel_iter_loop_test,
    gauss_seidel_parallel_level_loop_body,
    gauss_seidel_parallel_level_loop_test,
    gauss_seidel_parallel_exit
  };
  
  futures_matrix_type<node>* futures;
  int numiters; int N; int M; int block_size; double* data;
  
  int n; int l; int iter;
  
  gauss_seidel_parallel(int numiters, int N, int M, int block_size, double* data)
  : numiters(numiters), N(N), M(M), block_size(block_size), data(data) { }
  
  void body() {
    switch (node::current_block_id) {
      case gauss_seidel_parallel_entry: {
        n = N / block_size;
        futures = new futures_matrix_type<node>(n);
        iter = 0;
        if (iter < numiters) {
          node::jump_to(gauss_seidel_parallel_iter_loop_body);
        } else {
          node::jump_to(gauss_seidel_parallel_exit);
        }
        break;
      }
      case gauss_seidel_parallel_iter_loop_body: {
        l = 1;
        if (l <= nb_levels(n)) {
          node::jump_to(gauss_seidel_parallel_level_loop_body);
        } else {
          node::jump_to(gauss_seidel_parallel_iter_loop_test);
        }
        break;
      }
      case gauss_seidel_parallel_iter_loop_test: {
        if (iter < numiters) {
          iter++;
          node::jump_to(gauss_seidel_parallel_iter_loop_body);
        } else {
          node::jump_to(gauss_seidel_parallel_exit);
        }
        break;
      }
      case gauss_seidel_parallel_level_loop_body: {
        auto loop_body = [=] (int c) {
          auto idx = index_of_cell_at_pos(n, l, c);
          return new gauss_seidel_loop_body<node>(futures, idx.first, idx.second,
                                                  M, block_size, data);
        };
        node::call(new parallel_for<decltype(loop_body), node>(0, n, loop_body),
                   gauss_seidel_parallel_exit);
        break;
      }
      case gauss_seidel_parallel_level_loop_test: {
        if (l <= nb_levels(n)) {
          l++;
          node::jump_to(gauss_seidel_parallel_level_loop_body);
        } else {
          node::jump_to(gauss_seidel_parallel_iter_loop_test);
        }
        break;
      }
      case gauss_seidel_parallel_exit: {
        // later: parallelize
        int nn = futures->n * futures->n;
        for (int i = 0; i < nn; i++) {
          node::deallocate_future(futures->items[i]);
        }
        delete futures;
        break;
      }
    }
  }
  
};

  // for reference
void _gauss_seidel_parallel(int numiters, int N, int M, int block_size, double* data) {
  assert((N % block_size) == 0);
  int n = N / block_size;
  for (int iter = 0; iter < numiters; iter++) {
    for (int l = 1; l <= nb_levels(n); l++) {
      for (int c = 0; c < nb_cells_in_level(n, l); c++) {
        auto idx = index_of_cell_at_pos(n, l, c);
        int i = idx.first * block_size;
        int j = idx.second * block_size;
        gauss_seidel_block(M, &data[M * i + j], block_size);
      }
    }
  }
}

void gauss_seidel_initialize(matrix_type<double>& mtx) {
  int N = mtx.n;
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      mtx.subscript(i, j) = (double) ((i == 25 && j == 25) || (i == N-25 && j == N-25)) ? 500 : 0;
    }
  }
}

double epsilon = 0.001;

bool same_contents(const matrix_type<double>& lhs,
                   const matrix_type<double>& rhs,
                   int& nb) {
  if (lhs.n != rhs.n) {
    return false;
  }
  int n = lhs.n;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      double diff = std::abs(lhs.subscript(i, j) - rhs.subscript(i, j));
      if (diff > epsilon) {
        nb++;
        return false;
      }
    }
  }
  return true;
}

} // end namespace

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/

namespace cmdline = pasl::util::cmdline;

void choose_edge_algorithm() {
  cmdline::argmap_dispatch c;
  c.add("simple", [&] {
    topdown::edge_algorithm = topdown::edge_algorithm_simple;
  });
  c.add("distributed", [&] {
    topdown::distributed::snzi::default_branching_factor =
    cmdline::parse_or_default_int("branching_factor",
                                  topdown::distributed::snzi::default_branching_factor);
    topdown::distributed::snzi::default_nb_levels =
    cmdline::parse_or_default_int("nb_levels",
                                  topdown::distributed::snzi::default_nb_levels);
    topdown::edge_algorithm = topdown::edge_algorithm_distributed;
  });
  c.add("dyntree", [&] {
    topdown::edge_algorithm = topdown::edge_algorithm_tree;
    topdown::dyntree::branching_factor =
    cmdline::parse_or_default_int("branching_factor",
                                  topdown::dyntree::branching_factor);
  });
  c.find_by_arg_or_default_key("edge_algo", "tree")();
}

void read_gauss_seidel_params(int& numiters, int& N, int& block_size) {
  numiters = cmdline::parse_or_default_int("numiters", 3);
  N = cmdline::parse_or_default_int("N", 128);
  block_size = cmdline::parse_or_default_int("block_size", 2);
}

std::deque<pasl::sched::thread_p> todo;

void add_todo(pasl::sched::thread_p t) {
  todo.push_back(t);
}

template <class Body>
class todo_function : public pasl::sched::thread {
public:
  
  Body body;
  
  todo_function(Body body) : body(body) { }
  
  void run() {
    body();
  }
  
  THREAD_COST_UNKNOWN
  
};

void add_todo(std::function<void()> f) {
  add_todo(new todo_function<decltype(f)>(f));
}

bool do_consistency_check;

template <class node>
void choose_command() {
  cmdline::argmap_dispatch c;
  c.add("async_loop", [&] {
    int n = cmdline::parse_or_default_int("n", 1);
    add_todo(new tests::async_loop<node>(n));
  });
  c.add("future_loop", [&] {
    int n = cmdline::parse_or_default_int("n", 1);
    add_todo(new tests::future_loop<node>(n));
  });
  c.add("future_pool", [&] {
    int n = cmdline::parse_or_default_int("n", 1);
    tests::fib_input = cmdline::parse_or_default_int("fib_input", tests::fib_input);
    add_todo(new tests::future_pool<node>(n));
  });
  c.add("seidel_parallel", [&] {
    int numiters;
    int N;
    int block_size;
    read_gauss_seidel_params(numiters, N, block_size);
    int M = N + 2;
    tests::matrix_type<double>* test_mtx = new tests::matrix_type<double>(M, 0.0);
    add_todo(new tests::gauss_seidel_parallel<node>(numiters, N, M, block_size, &(test_mtx->items[0])));
    add_todo([&] {
      if (do_consistency_check) {
        tests::matrix_type<double> reference_mtx(M, 0.0);
        tests::gauss_seidel_initialize(reference_mtx);
        tests::gauss_seidel_sequential(numiters, N, M, block_size, &reference_mtx.items[0]);
        int nb_diffs = 0;
        bool success = tests::same_contents(reference_mtx, *test_mtx, nb_diffs);
        assert(success);
      }
      delete test_mtx;
    });
  });
  c.add("seidel_sequential", [&] {
    int numiters;
    int N;
    int block_size;
    read_gauss_seidel_params(numiters, N, block_size);
    int M = N + 2;
    tests::matrix_type<double>* test_mtx = new tests::matrix_type<double>(M, 0.0);
    add_todo(new tests::gauss_seidel_sequential_node<node>(numiters, N, M, block_size, &test_mtx->items[0]));
    add_todo([&] {
      delete test_mtx;
    });
  });
  c.find_by_arg("cmd")();
}

void launch() {
  do_consistency_check = cmdline::parse_or_default_bool("consistency_check", false);
  communication_delay = cmdline::parse_or_default_int("communication_delay",
                                                      communication_delay);
  cmdline::argmap_dispatch c;
  c.add("topdown", [&] {
    choose_edge_algorithm();
    choose_command<topdown::node>();
  });
  c.add("bottomup", [&] {
    choose_command<bottomup::node>();
  });
  c.find_by_arg("algo")();
  while (! todo.empty()) {
    pasl::sched::thread_p t = todo.front();
    todo.pop_front();
    pasl::sched::threaddag::launch(t);
  }
}

int main(int argc, char** argv) {
  cmdline::set(argc, argv);
  pasl::sched::threaddag::init();
  launch();
  pasl::sched::threaddag::destroy();
  return 0;
}

/***********************************************************************/

