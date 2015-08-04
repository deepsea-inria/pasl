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
/* The top-down algorithm */

namespace topdown {

class node;
class incounter;
class outset;

void add_node(node*);
void add_edge(node*, node*);
void add_edge(node*, node*, outset*, incounter*);
void prepare_node(node*);
void prepare_node(node*, incounter*);
void prepare_node(node*, outset*);
void prepare_node(node*, incounter*, outset*);
void join_with(node*, incounter*);
void continue_with(node*);
void decrement_incounter(node*);
incounter* incounter_ready();
incounter* incounter_unary();
incounter* incounter_new();
incounter* incounter_fetch_add();
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
  
  virtual void increment() = 0;
  
  virtual status_type decrement() = 0;
  
  void check(pasl::sched::thread_p t) {
    if (is_activated()) {
      start(t);
    }
  }
  
  void delta(pasl::sched::thread_p t, int64_t d) {
    if (d == -1L) {
      if (decrement() == activated) {
        start(t);
      }
    } else if (d == +1L) {
      increment();
    } else {
      assert(false);
    }
  }
  
};

class outset : public pasl::sched::outstrategy::common {
public:
  
  using insert_status_type = enum {
    insert_success,
    insert_fail
  };
  
  bool should_deallocate = true;
  
  virtual insert_status_type insert(node*) = 0;
  
  virtual void finish() = 0;
  
  void add(pasl::sched::thread_p t) {
    insert((node*)t);
  }
  
  void finished() {
    finish();
  }
  
};

using edge_algorithm_type = enum {
  edge_algorithm_simple,
  edge_algorithm_perprocessor,
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
        decrement_incounter(n);
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
  }

  
};
  
} // end namespace
  
namespace perprocessor {
  
class perprocessor_incounter : public incounter, public pasl::util::worker::periodic_t {
public:
      
  pasl::data::perworker::counter::carray<int64_t> counter;
      
  node* n;

  void init(pasl::sched::thread_p t) {
    n = (node*)t;
    counter.init(0);
    pasl::sched::scheduler::get_mine()->add_periodic(this);
  }
  
  bool is_activated() const {
    return (counter.sum() == 0);
  }
  
  void increment() {
    delta((pasl::sched::thread_p)n, +1L);
  }
  
  status_type decrement() {
    delta((pasl::sched::thread_p)n, -1L);
    return is_activated() ? activated: not_activated;
  }
  
  void check(node* _n) {
    assert(_n == n);
    if (counter.sum() == 0) {
      pasl::sched::scheduler::get_mine()->rem_periodic(this);
      start((pasl::sched::thread_p)n);
    }
  }
  
  void check() {
    check(n);
  }
  
  void delta(pasl::sched::thread_p t, int64_t d) {
    assert((node*)t == n);
    pasl::worker_id_t my_id = pasl::util::worker::get_my_id();
    counter.delta(my_id, d);
  }
  
};

class perprocessor_outset : public outset, public pasl::util::worker::periodic_t {
public:
  
  using node_buffer_type = std::vector<node*>;
  
  pasl::data::perworker::array<node_buffer_type> nodes;
  
  bool finished_indicator = false;
  
  insert_status_type insert(node* n) {
    if (finished_indicator) {
      return insert_fail;
    }
    pasl::sched::scheduler::get_mine()->add_periodic(this);
    node_buffer_type& buffer = nodes.mine();
    buffer.push_back(n);
    return insert_success;
  }
  
  void check() {
    if (finished_indicator) {
      node_buffer_type& buffer = nodes.mine();
      while (! buffer.empty()) {
        node* n = buffer.back();
        buffer.pop_back();
        decrement_incounter(n);
      }
      pasl::sched::scheduler::get_mine()->rem_periodic(this);
    }
  }
  
  void finish() {
    finished_indicator = true;
  }
  
};
  
} // end namespace
  
namespace tree {
  
const int B = 2;
int K = 100;
  
class tree_incounter;
class tree_outset;
class ictnode;
class ostnode;

void deallocate_incounter_tree(ictnode*);
void finish_outset(tree_outset*);
void deallocate_outset_tree(ostnode*);
  
class ictnode {
public:
  
  static constexpr int minus = 1;
  
  std::atomic<ictnode*> children[B];
  
  void init(ictnode* v) {
    for (int i = 0; i < B; i++) {
      children[i].store(v);
    }
  }
  
  ictnode() {
    init(nullptr);
  }
  
  ictnode(ictnode* i) {
    init(i);
  }
  
  bool is_leaf() const {
    for (int i = 0; i < B; i++) {
      ictnode* child = tagged_pointer_of(children[i].load());
      if (child != nullptr) {
        return false;
      }
    }
    return true;
  }
  
};

class tree_incounter : public incounter {
public:
  
  ictnode* in;
  ictnode* out;
  
  ictnode* minus() const {
    return tagged_tag_with((ictnode*)nullptr, ictnode::minus);
  }
  
  tree_incounter() {
    in = nullptr;
    out = new ictnode(minus());
    out = tagged_tag_with(out, ictnode::minus);
  }
  
  ~tree_incounter() {
    assert(is_activated());
    deallocate_incounter_tree(tagged_pointer_of(out));
    out = nullptr;
  }
  
  bool is_activated() const {
    return in == nullptr;
  }
  
  void increment() {
    ictnode* leaf = new ictnode;
    while (true) {
      if (in == nullptr) {
        in = leaf;
        return;
      }
      assert(in != nullptr);
      ictnode* current = in;
      while (true) {
        int i = random_int(0, B);
        std::atomic<ictnode*>& branch = current->children[i];
        ictnode* next = branch.load();
        if (tagged_tag_of(next) == ictnode::minus) {
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
  
  status_type decrement() {
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
        int i = random_int(0, B);
        std::atomic<ictnode*>& branch = current->children[i];
        ictnode* next = branch.load();
        if (   (next == nullptr)
            || (tagged_tag_of(next) == ictnode::minus) ) {
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
    for (int i = 0; i < B; i++) {
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
    n = tagged_tag_with(n, ictnode::minus);
    while (true) {
      ictnode* current = tagged_pointer_of(out);
      while (true) {
        int i = random_int(0, B);
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
  
  std::atomic<tagged_pointer_type> children[B];
  
  void init() {
    for (int i = 0; i < B; i++) {
      tagged_pointer_type p;
      p.interior = tagged_tag_with((ostnode*)nullptr, empty);
      children[i].store(p);
    }
  }
  
  ostnode() {
    init();
  }
  
  ostnode(tagged_pointer_type pointer) {
    init();
    children[0].store(pointer);
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
  
class tree_outset : public outset {
public:
 
  ostnode* root;
  
  tree_outset() {
    root = new ostnode;
  }
  
  ~tree_outset() {
    deallocate_outset_tree(root);
  }
  
  insert_status_type insert(ostnode::tagged_pointer_type val) {
    ostnode* current = root;
    ostnode* next = nullptr;
    while (true) {
      ostnode::tagged_pointer_type n;
      while (true) {
        int i = random_int(0, B);
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
          ostnode* tmp = new ostnode(val);
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
    finish_outset(this);
  }

};
  
} // end namespace
  
class node : public pasl::sched::thread {
public:
  
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
    join_with(consumer, incounter_new());
    add_edge(producer, consumer);
    add_node(producer);
  }
  
  outset* future(node* producer, int continuation_block_id) {
    node* consumer = this;
    prepare_node(producer, incounter_ready());
    outset* producer_out = (outset*)producer->out;
    producer_out->should_deallocate = false;
    consumer->jump_to(continuation_block_id);
    add_node(producer);
    return producer_out;
  }
  
  void force(outset* producer_out, int continuation_block_id) {
    node* consumer = this;
    prepare_for_transfer(continuation_block_id);
    incounter* consumer_in = incounter_unary();
    join_with(consumer, consumer_in);
    add_edge(consumer, nullptr, producer_out, consumer_in);
  }
  
  void call(node* target, int continuation_block_id) {
    finish(target, continuation_block_id);
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
  
incounter* incounter_new() {
  if (edge_algorithm == edge_algorithm_simple) {
    return incounter_fetch_add();
  } else if (edge_algorithm == edge_algorithm_perprocessor) {
    return new perprocessor::perprocessor_incounter;
  } else if (edge_algorithm == edge_algorithm_tree) {
    return new tree::tree_incounter;
  } else {
    assert(false);
    return nullptr;
  }
}

outset* outset_unary() {
  return (outset*)pasl::sched::outstrategy::unary_new();
}
  
outset* outset_noop() {
  return (outset*)pasl::sched::outstrategy::noop_new();
}

outset* outset_new() {
  if (edge_algorithm == edge_algorithm_simple) {
    return new simple::simple_outset;
  } else if (edge_algorithm == edge_algorithm_perprocessor) {
    return new perprocessor::perprocessor_outset;
  } else if (edge_algorithm == edge_algorithm_tree) {
    return new tree::tree_outset;
  } else {
    assert(false);
    return nullptr;
  }
}
  
void increment_incounter(node* n, incounter* n_in) {
  long tag = pasl::sched::instrategy::extract_tag(n_in);
  assert(tag != pasl::sched::instrategy::READY_TAG);
  if (tag == pasl::sched::instrategy::UNARY_TAG) {
    // nothing to do
  } else if (tag == pasl::sched::instrategy::FETCH_ADD_TAG) {
    pasl::data::tagged::atomic_fetch_and_add<pasl::sched::instrategy_p>(&(n->in), 1l);
  } else {
    assert(tag == 0);
    n_in->delta(n, +1L);
  }
}
  
void increment_incounter(node* n) {
  increment_incounter(n, (incounter*)n->in);
}
  
void decrement_incounter(node* n, incounter* n_in) {
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
    assert(tag == 0);
    n->in->delta(n, -1L);
  }
}

void decrement_incounter(node* n) {
  decrement_incounter(n, (incounter*)n->in);
}
  
void add_node(node* n) {
  pasl::sched::threaddag::add_thread(n);
}
  
outset::insert_status_type add_to_outset(node* n, outset* out, node* m) {
  long tag = pasl::sched::outstrategy::extract_tag(out);
  assert(tag != pasl::sched::outstrategy::NOOP_TAG);
  if (tag == pasl::sched::outstrategy::UNARY_TAG) {
    n->out = pasl::data::tagged::create<pasl::sched::thread_p, pasl::sched::outstrategy_p>(m, tag);
    return outset::insert_success;
  } else {
    assert(tag == 0);
    return out->insert(m);
  }
}

void add_edge(node* target, node* source, outset* source_out, incounter* target_in) {
  increment_incounter(target, target_in);
  if (add_to_outset(source, source_out, target) == outset::insert_fail) {
    decrement_incounter(target, target_in);
  }
}
  
void add_edge(node* source, node* target) {
  add_edge(target, source, (outset*)source->out, (incounter*)target->in);
}
  
void prepare_node(node* n, incounter* in, outset* out) {
  n->set_instrategy(in);
  n->set_outstrategy(out);
}
  
void prepare_node(node* n) {
  prepare_node(n, incounter_new(), outset_new());
}

void prepare_node(node* n, incounter* in) {
  prepare_node(n, in, outset_new());
}

void prepare_node(node* n, outset* out) {
  prepare_node(n, incounter_new(), out);
}
  
outset* capture_outset() {
  auto sched = pasl::sched::threaddag::my_sched();
  outset* out = (outset*)sched->get_outstrategy();
  assert(out != nullptr);
  sched->set_outstrategy(outset_new());
  return out;
}

void join_with(node* n, incounter* in) {
  prepare_node(n, in, capture_outset());
}

void continue_with(node* n) {
  join_with(n, incounter_ready());
  add_node(n);
}
  
void deallocate_future(outset* out) {
  assert(! out->should_deallocate);
  delete out;
}
  
namespace tree {
  
void deallocate_incounter_tree_partial(std::deque<ictnode*>& todo) {
  int k = 0;
  while ( (k < K) && (! todo.empty()) ) {
    ictnode* current = todo.back();
    todo.pop_back();
    for (int i = 0; i < B; i++) {
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
  
void finish_outset_tree_partial(std::deque<ostnode*>& todo) {
  int k = 0;
  while ( (k < K) && (! todo.empty()) ) {
    ostnode* current = todo.back();
    todo.pop_back();
    for (int i = 0; i < B; i++) {
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
      }
      if (tag == ostnode::interior) {
        todo.push_back(tagged_pointer_of(n.interior));
      }
    }
    k++;
  }
}
  
class finish_outset_tree_par_rec : public node {
public:
  
  using self_type = finish_outset_tree_par_rec;
  
  enum {
    process_block=0,
    repeat_block,
    exit_block
  };

  node* join;
  std::deque<ostnode*> todo;
  
  finish_outset_tree_par_rec(node* join, ostnode* n)
  : join(join) {
    todo.push_back(n);
  }
  
  finish_outset_tree_par_rec(node* join, std::deque<ostnode*>& _todo)
  : join(join) {
    _todo.swap(todo);
  }
  
  void body() {
    switch (current_block_id) {
      case process_block: {
        finish_outset_tree_partial(todo);
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
  
class finish_outset_tree_par : public node {
public:
  
  using self_type = finish_outset_tree_par;
  
  enum {
    entry_block=0,
    exit_block
  };
  
  tree_outset* out;
  std::deque<ostnode*> todo;
  
  finish_outset_tree_par(tree_outset* out, std::deque<ostnode*>& _todo)
  : out(out) {
    todo.swap(_todo);
  }
  
  void body() {
    switch (current_block_id) {
      case entry_block: {
        finish(new finish_outset_tree_par_rec(this, todo),
               exit_block);
        break;
      }
      case exit_block: {
        if (out->should_deallocate) {
          delete out;
        }
        break;
      }
      default:
        break;
    }
  }
  
};
  
void finish_outset(tree_outset* out) {
  std::deque<ostnode*> todo;
  todo.push_back(out->root);
  finish_outset_tree_partial(todo);
  if (! todo.empty()) {
    auto n = new finish_outset_tree_par(out, todo);
    prepare_node(n);
    add_node(n);
  } else {
    if (out->should_deallocate) {
      delete out;
    }
  }
}
  
void deallocate_outset_tree_partial(std::deque<ostnode*>& todo) {
  int k = 0;
  while ( (k < K) && (! todo.empty()) ) {
    ostnode* n = todo.back();
    todo.pop_back();
    for (int i = 0; i < B; i++) {
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
/* Test functions for the top-down algorithm */

namespace topdown {
  
std::atomic<int> async_leaf_counter;
std::atomic<int> async_interior_counter;
  
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
    switch (current_block_id) {
      case async_loop_rec_entry: {
        int n = hi - lo;
        if (n == 0) {
          return;
        } else if (n == 1) {
          async_leaf_counter.fetch_add(1);
        } else {
          async_interior_counter.fetch_add(1);
          mid = (lo + hi) / 2;
          async(new async_loop_rec(lo, mid, consumer), consumer,
                async_loop_rec_mid);
        }
        break;
      }
      case async_loop_rec_mid: {
        async(new async_loop_rec(mid, hi, consumer), consumer,
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
    switch (current_block_id) {
      case async_loop_entry: {
        async_leaf_counter.store(0);
        async_interior_counter.store(0);
        finish(new async_loop_rec(0, n, this),
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
  
  outset* branch1_out;
  outset* branch2_out;
  
  int mid;
  
  future_loop_rec(int lo, int hi)
  : lo(lo), hi(hi) { }
  
  void body() {
    switch (current_block_id) {
      case future_loop_entry: {
        int n = hi - lo;
        if (n == 0) {
          return;
        } else if (n == 1) {
          future_leaf_counter.fetch_add(1);
        } else {
          mid = (lo + hi) / 2;
          node* branch1 = new future_loop_rec(lo, mid);
          branch1_out = future(branch1,
                               future_loop_branch2);
        }
        break;
      }
      case future_loop_branch2: {
        node* branch2 = new future_loop_rec(mid, hi);
        branch2_out = future(branch2,
                             future_loop_force1);
        break;
      }
      case future_loop_force1: {
        force(branch1_out,
              future_loop_force2);
        break;
      }
      case future_loop_force2: {
        force(branch2_out,
              future_loop_exit);
        break;
      }
      case future_loop_exit: {
        future_interior_counter.fetch_add(1);
        deallocate_future(branch1_out);
        deallocate_future(branch2_out);
        break;
      }
      default:
        break;
    }
  }
  
};
  
class future_loop : public node {
public:
  
  enum {
    future_loop_entry,
    future_loop_force,
    future_loop_exit
  };
  
  int n;
  
  outset* root_out;
  
  future_loop(int n)
  : n(n) { }
  
  void body() {
    switch (current_block_id) {
      case future_loop_entry: {
        future_leaf_counter.store(0);
        future_interior_counter.store(0);
        node* root = new future_loop_rec(0, n);
        root_out = future(root,
                          future_loop_force);
        break;
      }
      case future_loop_force: {
        force(root_out,
              future_loop_exit);
        break;
      }
      case future_loop_exit: {
        deallocate_future(root_out);
        assert(future_leaf_counter.load() == n);
        assert(future_interior_counter.load() + 1 == n);
        break;
      }
      default:
        break;
    }
  }
  
};

template <class Body_gen>
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
    switch (current_block_id) {
      case parallel_for_rec_entry: {
        int n = hi - lo;
        if (n == 0) {
          // nothing to do
        } else if (n == 1) {
          call(body_gen(lo),
               parallel_for_rec_exit);
        } else {
          mid = (hi + lo) / 2;
          async(new parallel_for_rec(lo, mid, body_gen, join), join,
                parallel_for_rec_branch2);
        }
        break;
      }
      case parallel_for_rec_branch2: {
        async(new parallel_for_rec(mid, hi, body_gen, join), join,
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
  
template <class Body_gen>
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
    switch (current_block_id) {
      case parallel_for_entry: {
        finish(new parallel_for_rec<Body_gen>(lo, hi, body_gen, this),
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
  
int fib_input = 20;
  
long fib_result;
  
class future_body : public node {
public:
  
  enum {
    future_body_entry,
    future_body_exit
  };
  
  
  void body() {
    switch (current_block_id) {
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

class future_reader : public node {
public:
  
  enum {
    future_reader_entry,
    future_reader_exit
  };
  
  outset* f;
  
  int i;
  
  future_reader(outset* f, int i)
  : f(f), i(i) { }
  
  void body() {
    switch (current_block_id) {
      case future_reader_entry: {
        force(f,
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
  
template <class Body_gen>
node* mk_parallel_for(int lo, int hi, Body_gen body_gen) {
  return new parallel_for<Body_gen>(lo, hi, body_gen);
}
  
class future_pool : public node {
public:
  
  enum {
    future_pool_entry,
    future_pool_call,
    future_pool_exit
  };
  
  int n;
  
  outset* f;
  
  future_pool(int n)
  : n(n) { }

  void body() {
    switch (current_block_id) {
      case future_pool_entry: {
        node* fut = new future_body();
        f = future(fut,
                   future_pool_call);
        break;
      }
      case future_pool_call: {
        node* b = mk_parallel_for(0, n, [=] (int i) {
          return new future_reader(f, i);
        });
        call(b,
             future_pool_exit);
        break;
      }
      case future_pool_exit: {
        deallocate_future(f);
        assert(future_pool_counter.load() == n);
        break;
      }
      default:
        break;
    }
  }
  
};

} // end namespace

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/* The bottom-up algorithm */

namespace bottomup {
  
class node;

class ictnode {
public:
  
  ictnode* parent;
  std::atomic<int> counter;
  
  ictnode() {
    parent = nullptr;
    counter.store(0);
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
  
  bool is_activated(ictnode* port) const {
    return port->parent == nullptr;
  }
  
  std::pair<ictnode*, ictnode*> increment(ictnode* port) {
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
  
  ictnode* decrement(ictnode* port) {
    ictnode* cur = port;
    while (cur->parent != nullptr) {
      ictnode* tmp2 = cur;
      cur = cur->parent;
      int count = cur->counter.load();
      delete tmp2;
      int orig = 0;
      if ( (count == 0) && cur->counter.compare_exchange_strong(orig, 1)) {
        return cur;
      }
    }
    return cur;
  }
  
  void check(pasl::sched::thread_p t) {

  }
  
  void delta(pasl::sched::thread_p t, int64_t d) {
    assert(false);
  }
  
};

std::pair<ictnode*, ictnode*> incr_incounter(ictnode* port, node* n);
void decr_incounter(ictnode* port, node* n);
  
void decr_inports(node* n);

using outset_insert_status_type = enum {
  outset_insert_success,
  outset_insert_fail
};

class outset : public pasl::sched::outstrategy::common {
public:
  
  static constexpr int ostnode_frozen = 1;
  
  ostnode* root;
  
  pasl::sched::thread_p t;
  
  outset() : t(nullptr) { }
  
  outset(pasl::sched::thread_p t)
  : t(t) { }
  
  std::pair<outset_insert_status_type, ostnode*> add(ostnode* outport, node* target, ictnode* inport) {
    ostnode* next = new ostnode;
    next->target = target;
    next->port = inport;
    ostnode* orig = nullptr;
    if (! (outport->children[0].compare_exchange_strong(orig, next))) {
      delete next;
      return std::make_pair(outset_insert_fail, nullptr);
    }
    return std::make_pair(outset_insert_success, next);
  }
  
  void add(pasl::sched::thread_p t) {
    assert(false);
  }
  
  void finish_rec(ostnode* outport, std::function<void (ictnode*, node*)> f) {
    std::set<ostnode*> branches;
    if (outport->target != nullptr) {
      f(outport->port, outport->target);
    }
    for (int i = 0; i < 2; i++) {
      ostnode* child = outport->children[i].load();
      while (true) {
        ostnode* orig = child;
        ostnode* next = tagged_tag_with((ostnode*)nullptr, ostnode_frozen);
        if (outport->children[i].compare_exchange_strong(orig, next)) {
          if (child != nullptr) {
            branches.insert(child);
          }
          break;
        }
      }
    }
    for (auto it = branches.cbegin(); it != branches.cend(); it++) {
      finish_rec(*it, f);
    }
  }
  
  void finished() {
    finish_rec(root, [&] (ictnode* port, node* n) {
      decr_incounter(port, n);
    });
    if (t != nullptr) {
      decr_inports((node*)t);
    }
    common::finished();
  }
  
  std::pair<ostnode*, ostnode*> fork2(ostnode* port) {
    ostnode* branches[2];
    branches[0] = new ostnode;
    branches[1] = new ostnode;
    for (int i = 0; i < 2; i++) {
      ostnode* orig = nullptr;
      if (! port->children[i].compare_exchange_strong(orig, branches[i])) {
        for (int j = 0; j < 2; j++) {
          delete branches[j];
          branches[j] = nullptr;
        }
      }
    }
    return std::make_pair(branches[0], branches[1]);
  }
  
};
  
using inports_type = std::map<node*, ictnode*>;
using outports_type = std::map<node*, ostnode*>;

void add_node(node* n);
  
void create_fresh_ports(node*, node*);
outset* capture_outset();
  
void join_with(node*, incounter*);
void continue_with(node*);
  
class node : public pasl::sched::thread {
public:
  
  const int uninitialized_block_id = -1;
  const int entry_block_id = 0;
  
  int current_block_id;
  
private:
  int continuation_block_id;
  
public:
  
  inports_type inports;
  outports_type outports;
  
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
  
  void init() {
    set_instrategy(new incounter);
    set_outstrategy(new outset);
  }
  
  void decr_inports() {
    for (auto it = inports.cbegin(); it != inports.cend(); it++) {
      decr_incounter(it->second, it->first);
    }
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
    producer->init();
    node* caller = this;
    prepare_for_transfer(continuation_block_id);
    create_fresh_ports(caller, producer);
    auto p = caller->inports.find(consumer);
    assert(p != caller->inports.end());
    ictnode* port = p->second;
    auto ports = incr_incounter(port, consumer);
    caller->inports.insert(std::make_pair(consumer, ports.first));
    producer->inports.insert(std::make_pair(consumer, ports.second));
    add_node(consumer);
    add_node(producer);
  }
  
  void finish(node* producer, int continuation_block_id) {
    producer->init();
    node* consumer = this;
    prepare_for_transfer(continuation_block_id);
    create_fresh_ports(consumer, producer);
    auto ports = incr_incounter(nullptr, consumer);
    producer->inports.insert(std::make_pair(consumer, ports.first));
    add_node(producer);
  }
  
  void future(node* producer, int continuation_block_id) {
    producer->init();
    producer->should_not_deallocate = true;
    node* caller = this;
    prepare_for_transfer(continuation_block_id);
    create_fresh_ports(caller, producer);
    outset* producer_out = (outset*)producer->out;
    caller->outports.insert(std::make_pair(producer, producer_out->root));
    add_node(caller);
    add_node(producer);
  }
  
  void force(node* producer, int continuation_block_id) {
    node* consumer = this;
    outset* producer_out = (outset*)producer->out;
    auto p = consumer->outports.find(producer);
    assert(p != consumer->outports.end());
    ostnode* outport = p->second;
    auto inport = incr_incounter(nullptr, consumer);
    auto res = producer_out->add(outport, consumer, inport.first);
    if (res.first == outset_insert_success) {
      consumer->outports.insert(std::make_pair(producer, res.second));
    } else {
      add_node(consumer);
    }
  }
  
  THREAD_COST_UNKNOWN
  
};

void create_fresh_inports(node* source, node* target) {
  inports_type source_ports = source->inports;
  inports_type target_ports;
  for (auto it = source->inports.cbegin(); it != source->inports.cend(); it++) {
    std::pair<node*, ictnode*> p = *it;
    if (target->inports.find(p.first) != target->inports.cend()) {
      incounter* in = (incounter*)p.first->in;
      auto ports = in->increment(p.second);
      source_ports.insert(std::make_pair(p.first, ports.first));
      target_ports.insert(std::make_pair(p.first, ports.second));
    }
  }
  source->inports.swap(source_ports);
  target->inports.swap(target_ports);
}

void create_fresh_outports(node* source, node* target) {
  outports_type source_ports = source->outports;
  outports_type target_ports;
  for (auto it = source->outports.cbegin(); it != source->outports.cend(); it++) {
    std::pair<node*, ostnode*> p = *it;
    if (target->outports.find(p.first) != target->outports.cend()) {
      outset* out = (outset*)p.first->out;
      auto ports = out->fork2(p.second);
      source_ports.insert(std::make_pair(p.first, ports.first));
      target_ports.insert(std::make_pair(p.first, ports.second));
    }
  }
  source->outports.swap(source_ports);
  target->outports.swap(target_ports);
}
  
void create_fresh_ports(node* source, node* target) {
  create_fresh_inports(source, target);
  create_fresh_outports(source, target);
}
  
std::pair<ictnode*, ictnode*> incr_incounter(ictnode* port, node* n) {
  incounter* in = (incounter*)n->in;
  return in->increment(port);
}

void decr_incounter(ictnode* port, node* n) {
  incounter* in = (incounter*)n->in;
  port = in->decrement(port);
  if (in->is_activated(port)) {
    delete port;
    in->start(n);
  }
}
  
void decr_inports(node* n) {
  n->decr_inports();
}
  
void add_node(node* n) {
  assert(false); // no good because check() is being called by add_thread()
  // instead, find a way to push n directly on the deque
  pasl::sched::threaddag::add_thread(n);
}
  
outset* capture_outset() {
  outset* out = (outset*)pasl::sched::threaddag::my_sched()->get_outstrategy();
  assert(out != nullptr);
  pasl::sched::threaddag::my_sched()->set_outstrategy(new outset);
  return out;
}
  
void join_with(node* n, incounter* in) {
  n->set_instrategy(in);
  n->set_outstrategy(capture_outset());
}

void continue_with(node* n) {
  join_with(n, new incounter);
  add_node(n);
}
  
} // end namespace

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/

void choose_edge_algorithm() {
  pasl::util::cmdline::argmap_dispatch c;
  c.add("simple", [&] {
    topdown::edge_algorithm = topdown::edge_algorithm_simple;
  });
  c.add("perprocessor", [&] {
    topdown::edge_algorithm = topdown::edge_algorithm_perprocessor;
  });
  c.add("tree", [&] {
    topdown::edge_algorithm = topdown::edge_algorithm_tree;
  });
  c.find("edge_algo", "tree")();
}

int main(int argc, char** argv) {
  pasl::util::cmdline::set(argc, argv);
  pasl::sched::threaddag::init();
  pasl::util::cmdline::argmap_dispatch c;
  pasl::sched::thread_p t;
  c.add("topdown", [&] {
    choose_edge_algorithm();
    pasl::util::cmdline::argmap_dispatch c;
    c.add("async_loop", [&] {
      int n = pasl::util::cmdline::parse_or_default_int("n", 1);
      t = new topdown::async_loop(n);
    });
    c.add("future_loop", [&] {
      int n = pasl::util::cmdline::parse_or_default_int("n", 1);
      t = new topdown::future_loop(n);
    });
    c.add("future_pool", [&] {
      int n = pasl::util::cmdline::parse_or_default_int("n", 1);
      topdown::fib_input = pasl::util::cmdline::parse_or_default_int("fib_input", topdown::fib_input);
      t = new topdown::future_pool(n);
    });
    c.find_by_arg("cmd")();
  });
  c.add("bottomup", [&] {
    assert(false);
  });
  c.find_by_arg("algo")();
  pasl::sched::threaddag::launch(t);
  pasl::sched::threaddag::destroy();
  return 0;
}

/***********************************************************************/

