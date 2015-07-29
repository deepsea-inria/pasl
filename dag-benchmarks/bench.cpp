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

int random_int(int lo, int hi) {
  std::uniform_int_distribution<int> distribution(lo, hi-1);
  return distribution(generator.mine());
}

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/* The top-down algorithm */

namespace topdown {
  
static constexpr int B = 2;

static constexpr int incounter_minus = 1;

class node;

class ictnode {
public:
  
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
  
  int nb_leaf_nodes() {
    if (is_leaf()) {
      return 1;
    }
    int cnt = 0;
    for (int i = 0; i < B; i++) {
      if (tagged_tag_of(children[i].load()) == incounter_minus) {
        continue;
      }
      ictnode* child = tagged_pointer_of(children[i].load());
      if (child != nullptr) {
        cnt += child->nb_leaf_nodes();
      }
    }
    return cnt;
  }
  
};
  
std::ostream& operator<<(std::ostream& out, ictnode* n) {
  out << n->nb_leaf_nodes();
  return out;
}

class incounter : public pasl::sched::instrategy::common {
public:
  
  ictnode* in;
  ictnode* out;
  std::atomic<bool> one_to_zero;
  
  ictnode* minus() const {
    return tagged_tag_with((ictnode*)nullptr, incounter_minus);
  }
  
  incounter() {
    in = new ictnode;
    out = new ictnode(minus());
    one_to_zero.store(false);
    out = tagged_tag_with(out, incounter_minus);
  }
  
  void destroy_icttree(ictnode* n) {
    for (int i = 0; i < B; i++) {
      ictnode* child = tagged_pointer_of(n->children[i].load());
      if (child == nullptr) {
        continue;
      }
      destroy_icttree(child);
      n->children[i].store(nullptr);
    }
    delete n;
  }
  
  ~incounter() {
    destroy_icttree(in);
    destroy_icttree(tagged_pointer_of(out));
    in = nullptr;
    out = nullptr;
  }
  
  bool is_zero() {
    if (in->is_leaf()) {
      while (true) {
        if (one_to_zero.load()) {
          return false;
        }
        bool orig = false;
        if (one_to_zero.compare_exchange_strong(orig, true)) {
          return true;
        }
      }
    }
    return false;
  }
  
  void increment() {
    ictnode* leaf = new ictnode;
    while (true) {
      ictnode* cur = in;
      while (true) {
        int i = random_int(0, B);
        std::atomic<ictnode*>& branch = cur->children[i];
        ictnode* next = branch.load();
        if (tagged_tag_of(next) == incounter_minus) {
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
        cur = next;
      }
    }
  }
  
  void decrement() {
    while (true) {
      ictnode* cur = in;
      while (true) {
        int i = random_int(0, B);
        std::atomic<ictnode*>& branch = cur->children[i];
        ictnode* next = branch.load();
        if (   (next == nullptr)
            || (tagged_tag_of(next) == incounter_minus) ) {
          break;
        }
        if (next->is_leaf()) {
          if (try_to_detatch(next)) {
            branch.store(nullptr);
            add_to_out(next);
            return;
          }
          break;
        }
        cur = next;
      }
    }
  }
  
  bool try_to_detatch(ictnode* n) {
    for (int i = 0; i < B; i++) {
      ictnode* orig = nullptr;
      if (! (n->children[i].compare_exchange_strong(orig, minus()))) {
        for (int j = i; j >= 0; j--) {
          n->children[j].store(nullptr);
        }
        return false;
      }
    }
    return true;
  }
  
  void add_to_out(ictnode* n) {
    while (true) {
      ictnode* cur = tagged_pointer_of(out);
      while (true) {
        int i = random_int(0, B);
        std::atomic<ictnode*>& branch = cur->children[i];
        ictnode* next = branch.load();
        if (tagged_pointer_of(next) == nullptr) {
          ictnode* orig = next;
          ictnode* tagged = tagged_tag_with(n, incounter_minus);
          if (branch.compare_exchange_strong(orig, tagged)) {
            return;
          }
          break;
        }
        cur = tagged_pointer_of(next);
      }
    }
  }
  
  void check(pasl::sched::thread_p t) {
    if (is_zero()) {
      start(t);
    }
  }
  
  void delta(pasl::sched::thread_p t, int64_t d) {
    assert(false);
  }
  
};
  
std::ostream& operator<<(std::ostream& out, incounter* n) {
  if (n->in->is_leaf()) {
    out << 0;
  } else {
    out << n->in;
  }
  return out;
}

using outset_add_status_type = enum {
  outset_add_success,
  outset_add_fail
};
  
class node;

void add_edge(node*, node*);

void join_with(node*, incounter*);

void continue_with(node*);

void add_node(node* n);

void decrement_incounter(node*);
  
class outset : public pasl::sched::outstrategy::common {
public:
  
  using ostnode_tag_type = enum {
    ostnode_empty=1,
    ostnode_leaf=2,
    ostnode_interior=3,
    ostnode_finished_empty=4,
    ostnode_finished_leaf=5,
    ostnode_finished_interior=6
  };
  
  using tagged_pointer_type = union {
    outset* interior;
    node* leaf;
  };
  
  std::atomic<tagged_pointer_type> items[B];
  
  void init() {
    for (int i = 0; i < B; i++) {
      tagged_pointer_type p;
      p.interior = tagged_tag_with((outset*)nullptr, ostnode_empty);
      items[i].store(p);
    }
  }
  
  outset() {
    init();
  }
  
  outset(tagged_pointer_type pointer) {
    init();
    items[0].store(pointer);
  }
  
  ~outset() {
    destroy();
  }
  
  void destroy() {
    for (int i = 0; i < B; i++) {
      tagged_pointer_type p = items[i].load();
      if (tagged_tag_of(p.interior) == ostnode_finished_empty)  {
        // nothing to do
      } else if (tagged_tag_of(p.interior) == ostnode_finished_interior) {
        outset* out = tagged_pointer_of(p.interior);
        delete out;
      } else if (tagged_tag_of(p.interior) == ostnode_finished_leaf) {
        // nothing to do
      } else {
        assert(false);
      }
    }
  }
  
  outset_add_status_type add(tagged_pointer_type pointer) {
    int i = random_int(0, B);
    tagged_pointer_type cur;
    outset_add_status_type result = outset_add_success;
    while (true) {
      cur = items[i].load();
      if (   (tagged_tag_of(cur.interior) == ostnode_finished_empty)
          || (tagged_tag_of(cur.interior) == ostnode_finished_leaf)
          || (tagged_tag_of(cur.interior) == ostnode_finished_interior) ){
        result = outset_add_fail;
        break;
      }
      if (tagged_tag_of(cur.interior) == ostnode_empty) {
        tagged_pointer_type orig = cur;
        if (items[i].compare_exchange_strong(orig, pointer)) {
          break;
        }
        cur = items[i].load();
      }
      if (tagged_tag_of(cur.interior) == ostnode_leaf) {
        tagged_pointer_type orig = cur;
        outset* nextp = new outset(pointer);
        tagged_pointer_type next;
        next.interior = tagged_tag_with(nextp, ostnode_interior);
        if (items[i].compare_exchange_strong(orig, next)) {
          break;
        }
        delete nextp;
        cur = items[i].load();
      }
      if (tagged_tag_of(cur.interior) == ostnode_interior) {
        outset* p = tagged_pointer_of(cur.interior);
        p->add(pointer);
        break;
      }
    }
    return result;
  }
  
  void add(pasl::sched::thread_p t) {
    assert(false);
  }
  
  outset_add_status_type my_add(node* leaf) {
    tagged_pointer_type p;
    p.leaf = tagged_tag_with(leaf, ostnode_leaf);
    return add(p);
  }
  
  tagged_pointer_type make_finished(tagged_pointer_type p) const {
    tagged_pointer_type result;
    int t = tagged_tag_of(p.interior);
    if (t == ostnode_empty) {
      outset* out = tagged_pointer_of(p.interior);
      result.interior = tagged_tag_with(out, ostnode_finished_empty);
    } else if (t == ostnode_leaf) {
      node* n = tagged_pointer_of(p.leaf);
      result.leaf = tagged_tag_with(n, ostnode_finished_leaf);
    } else if (t == ostnode_interior) {
      outset* out = tagged_pointer_of(p.interior);
      result.interior = tagged_tag_with(out, ostnode_finished_interior);
    } else {
      assert(false);
    }
    return result;
  }
  
  void finish(std::function<void (node*)> f) {
    for (int i = 0; i < B; i++) {
      tagged_pointer_type cur = items[i].load();
      while (true) {
        tagged_pointer_type orig = cur;
        if (items[i].compare_exchange_strong(orig, make_finished(cur))) {
          break;
        }
        cur = items[i].load();
      }
      if (tagged_tag_of(cur.leaf) == ostnode_leaf) {
        f(tagged_pointer_of(cur.leaf));
      }
      if (tagged_tag_of(cur.interior) == ostnode_interior) {
        outset* interior = tagged_pointer_of(cur.interior);
        interior->finish(f);
      }
    }
  }
  
  void finished() {
    finish([&] (node* n) {
      decrement_incounter(n);
    });
    common::finished();
  }
  
  void visit(std::function<void (node*)> f) {
    for (int i = 0; i < B; i++) {
      tagged_pointer_type cur = items[i].load();
      int t = tagged_tag_of(cur.interior);
      if (   (t == ostnode_leaf)
          || (t == ostnode_finished_leaf) ) {
        f(tagged_pointer_of(cur.leaf));
      } else if (   (t == ostnode_interior)
                 || (t == ostnode_finished_interior) ) {
        outset* out = tagged_pointer_of(cur.interior);
        out->visit(f);
      }
    }
  }
  
};
  
template <class Item>
std::ostream& operator<<(std::ostream& out, const std::vector<Item>& xs) {
  out << "{ ";
  size_t sz = xs.size();
  for (long i = 0; i < sz; i++) {
    out << xs[i];
    if (i+1 < sz)
      out << ", ";
  }
  out << " }";
  return out;
}
  
std::ostream& operator<<(std::ostream& out, outset* o) {
  std::vector<node*> nodes;
  o->visit([&] (node* n) {
    nodes.push_back(n);
  });
  out << nodes;
  return out;
}
  
outset* capture_outset();
  
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
  
  void init() {
    set_instrategy(new incounter);
    set_outstrategy(new outset);
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
    add_edge(producer, consumer);
    jump_to(continuation_block_id);
    add_node(producer);
  }
  
  void finish(node* producer, int continuation_block_id) {
    node* consumer = this;
    producer->init();
    prepare_for_transfer(continuation_block_id);
    join_with(consumer, new incounter);
    add_edge(producer, consumer);
    add_node(producer);
  }
  
  void future(node* producer, int continuation_block_id) {
    node* consumer = this;
    producer->init();
    producer->should_not_deallocate = true;
    consumer->jump_to(continuation_block_id);
    add_node(producer);
  }
  
  void force(node* producer, int continuation_block_id) {
    node* consumer = this;
    prepare_for_transfer(continuation_block_id);
    join_with(consumer, new incounter);
    add_edge(producer, consumer);
  }
  
  THREAD_COST_UNKNOWN
  
};
  
void increment_incounter(node* n) {
  incounter* in = (incounter*)n->in;
  in->increment();
}

void decrement_incounter(node* n) {
  incounter* in = (incounter*)n->in;
  in->decrement();
  if (in->is_zero()) {
    in->start(n);
  }
}
  
void add_node(node* n) {
  pasl::sched::threaddag::add_thread(n);
}
  
void add_edge(node* source, node* target) {
  increment_incounter(target);
  outset* out = (outset*)source->out;
  if (out->my_add(target) == outset_add_fail) {
    decrement_incounter(target);
  }
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
          async(new async_loop_rec(lo, mid, consumer), consumer, async_loop_rec_mid);
        }
        break;
      }
      case async_loop_rec_mid: {
        async(new async_loop_rec(mid, hi, consumer), consumer, async_loop_rec_exit);
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
        finish(new async_loop_rec(0, n, this), async_loop_exit);
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
  
  future_loop_rec* branch1;
  future_loop_rec* branch2;
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
          branch1 = new future_loop_rec(lo, mid);
          future(branch1, future_loop_branch2);
        }
        break;
      }
      case future_loop_branch2: {
        branch2 = new future_loop_rec(mid, hi);
        future(branch2, future_loop_force1);
        break;
      }
      case future_loop_force1: {
        force(branch1, future_loop_branch2);
        break;
      }
      case future_loop_force2: {
        force(branch2, future_loop_exit);
        break;
      }
      case future_loop_exit: {
        future_interior_counter.fetch_add(1);
        delete branch1;
        delete branch2;
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
  
  future_loop_rec* root;
  
  future_loop(int n)
  : n(n) { }
  
  void body() {
    switch (current_block_id) {
      case future_loop_entry: {
        future_leaf_counter.store(0);
        future_interior_counter.store(0);
        root = new future_loop_rec(0, n);
        future(root, future_loop_force);
        break;
      }
      case future_loop_force: {
        force(root, future_loop_exit);
        break;
      }
      case future_loop_exit: {
        delete root;
        assert(future_leaf_counter.load() == n);
        assert(future_interior_counter.load() == n);
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
  
  bool is_zero(ictnode* port) const {
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

using outset_add_status_type = enum {
  outset_add_success,
  outset_add_fail
};

class outset : public pasl::sched::outstrategy::common {
public:
  
  static constexpr int ostnode_frozen = 1;
  
  ostnode* root;
  
  pasl::sched::thread_p t;
  
  outset() : t(nullptr) { }
  
  outset(pasl::sched::thread_p t)
  : t(t) { }
  
  std::pair<outset_add_status_type, ostnode*> add(ostnode* outport, node* target, ictnode* inport) {
    ostnode* next = new ostnode;
    next->target = target;
    next->port = inport;
    ostnode* orig = nullptr;
    if (! (outport->children[0].compare_exchange_strong(orig, next))) {
      delete next;
      return std::make_pair(outset_add_fail, nullptr);
    }
    return std::make_pair(outset_add_success, next);
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
    if (res.first == outset_add_success) {
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
  if (in->is_zero(port)) {
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

int main(int argc, char** argv) {
  pasl::util::cmdline::set(argc, argv);
  pasl::sched::threaddag::init();
  pasl::util::cmdline::argmap_dispatch c;
  pasl::sched::thread_p t;
  c.add("topdown", [&] {
    pasl::util::cmdline::argmap_dispatch c;
    c.add("async_loop", [&] {
      int n = pasl::util::cmdline::parse_or_default_int("n", 1);
      t = new topdown::async_loop(n);
    });
    c.add("future_loop", [&] {
      int n = pasl::util::cmdline::parse_or_default_int("n", 1);
      t = new topdown::future_loop(n);
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

