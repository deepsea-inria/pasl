
/*!
 * \file bench.cpp
 * \brief Benchmarking program for DAG-machine experiments
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include <random>
#include <unordered_map>
#include <set>
#include <deque>
#include <cmath>
#include <memory>
#include <chrono>
#include <thread>
#include <climits>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>

#ifdef HAVE_HWLOC
#include <hwloc.h>
#endif

#include "pasl.hpp"
#include "tagged.hpp"
#include "snzi.hpp"
#include "gsnzi.hpp"
#include "microtime.hpp"
#include "chunkedseq.hpp"
#include "chunkedbag.hpp"
#include "sequence.hpp"
#include "outset.hpp"

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

template <class T>
T* tagged_tag_with(int t) {
  return tagged_tag_with((T*)nullptr, t);
}

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/* Random-number generator */

#ifndef USE_STL_RANDGEN

static inline
unsigned int hashu(unsigned int a) {
  a = (a+0x7ed55d16) + (a<<12);
  a = (a^0xc761c23c) ^ (a>>19);
  a = (a+0x165667b1) + (a<<5);
  a = (a+0xd3a2646c) ^ (a<<9);
  a = (a+0xfd7046c5) + (a<<3);
  a = (a^0xb55a4f09) ^ (a>>16);
  return a;
}

// generate a random number in the range (lo, hi], assuming non-negative lo
static inline
int random_int_in_range(unsigned int& rng, int lo, int hi) {
  unsigned int r = hashu(rng);
  rng = r;
  assert((hi - lo) > 0);
  r = (r % (hi - lo)) + lo;
  assert(r >= lo);
  assert(r < hi);
  return r;
}

pasl::data::perworker::array<unsigned int> generator;

static inline
int random_int(int lo, int hi) {
  unsigned int& rng = generator.mine();
  return random_int_in_range(rng, lo, hi);
}

#else

pasl::data::perworker::array<std::mt19937> generator;

// generate a random number in the range (lo, hi], assuming non-negative lo
int random_int(int lo, int hi) {
  std::uniform_int_distribution<int> distribution(lo, hi-1);
  return distribution(generator.mine());
}

#endif

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/* Globals */

int communication_delay = 512;

using port_passing_mode = enum {
  port_passing_default,
  port_passing_intersection,
  port_passing_difference
};

bool should_communicate() {
  return pasl::sched::threaddag::my_sched()->should_call_communicate();
}

template <class T>
T* malloc_array(size_t n) {
  return (T*)malloc(sizeof(T) * n);
}

namespace {
static constexpr int backoff_nb_cycles = 1l << 17;
template <class T>
bool compare_exchange(std::atomic<T>& cell, T& expected, T desired) {
  if (cell.compare_exchange_strong(expected, desired)) {
    return true;
  }
  pasl::util::microtime::wait_for(backoff_nb_cycles);
  return false;
}
} // end namespace


/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/* The top-down algorithm */

namespace direct {

class node;
class incounter;
class incounter_node;
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
template <class Body>
node* new_parallel_for(long, long, node*, int, const Body&);
template <class Body>
node* new_parallel_for(long, long, node*, const Body&);

  
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
  
  virtual bool is_growabletree() const {
    return false;
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
  edge_algorithm_statreeopt,
  edge_algorithm_growabletree
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
  
  const int finished_tag = 1;
  
  simple_outset() {
    head.store(nullptr);
  }
  
  insert_status_type insert(node* n) {
    insert_status_type result = insert_success;
    concurrent_list_type* cell = new concurrent_list_type;
    cell->n = n;
    while (true) {
      concurrent_list_type* orig = head.load();
      if (tagged_tag_of(orig) == finished_tag) {
        result = insert_fail;
        delete cell;
        break;
      } else {
        cell->next = orig;
        if (compare_exchange(head, orig, cell)) {
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
      concurrent_list_type* next = tagged_tag_with(v, finished_tag);
      if (compare_exchange(head, orig, next)) {
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
  
namespace statreeopt {
  
#ifndef SNZI_TREE_HEIGHT
#define SNZI_TREE_HEIGHT 6
#endif
  
constexpr int snzi_tree_height = SNZI_TREE_HEIGHT;
  
class statreeopt_incounter : public incounter {
public:
      
  pasl::data::snzi::tree<snzi_tree_height> nzi;
  
  statreeopt_incounter(node* n) {
    nzi.set_root_annotation(n);
  }
  
  bool is_activated() const {
    return (! nzi.is_nonzero());
  }
  
  void increment(node* source) {
    nzi.get_target_of_value(source)->increment();
  }
  
  status_type decrement(node* source) {
    bool b = nzi.get_target_of_value(source)->decrement();
    return b ? incounter::activated : incounter::not_activated;
  }
  
};
  
void unary_finished(pasl::sched::thread_p t) {
  pasl::data::snzi::node* leaf = (pasl::data::snzi::node*)t;
  if (leaf->decrement()) {
    node* n = pasl::data::snzi::node::get_root_annotation<node*>(leaf);
    pasl::sched::instrategy::schedule((pasl::sched::thread_p)n);
  }
}
  
} // end namespace
  
namespace growabletree {
  
  
class growabletree_outset;

void outset_tree_deallocate(growabletree_outset*);
void outset_finish(growabletree_outset*);
  
bool should_deallocate_sequentially = false;
  
class growabletree_incounter : public incounter {
public:
  
  using nzi_type = pasl::data::gsnzi::tree<>;
  using nzi_node_type = typename nzi_type::node_type;
  
  nzi_type nzi;
  
  growabletree_incounter(node* n) {
    nzi.set_root_annotation(n);
  }
  
  bool is_activated() const {
    return (! nzi.is_nonzero());
  }
  
  void increment(node* source) {
    assert(false);
//    nzi.get_target_of_value(source)->increment();
  }
  
  status_type decrement(node* source) {
    assert(false);
    return incounter::not_activated;
/*    bool b = nzi.get_target_of_value(source)->decrement();
    return b ? incounter::activated : incounter::not_activated; */
  }
  
  bool is_growabletree() const {
    return true;
  }
  
};
  
void unary_finished(pasl::sched::thread_p t) {
  using nzi_type = typename growabletree_incounter::nzi_type;
  using nzi_node_type = typename growabletree_incounter::nzi_node_type;
  nzi_node_type* target = (nzi_node_type*)t;
  if (target->decrement()) {
    node* n = nzi_node_type::get_root_annotation<node*>(target);
    pasl::sched::instrategy::schedule((pasl::sched::thread_p)n);
  }
}

using set_type = pasl::data::outset::outset<node*, 4, 4096>;
using node_type = typename set_type::node_type;

class growabletree_outset : public outset {
public:
  
  set_type set;
  
  ~growabletree_outset() {
    outset_tree_deallocate(this);
  }
  
  insert_status_type insert(node* n) {
    int my_id = pasl::sched::threaddag::get_my_id();
    bool success = set.insert(n, my_id, [&] (int lo, int hi) {
      return random_int(lo, hi);
    });
    if (success) {
      return insert_success;
    } else {
      return insert_fail;
    }
  }
  
  void finish() {
    outset_finish(this);
  }
  
  void add(pasl::sched::thread_p t) {
    assert(false);
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
  
  static outset* allocate_future() {
    outset* out = outset_new();
    out->enable_future();
    return out;
  }
  
  void listen_on(outset*) {
    // nothing to do
  }
  
  void spawn(node* n, outset* out) {
    prepare_node(n, incounter_ready(), out);
    add_node(n);
  }
  
  void spawn(node* n) {
    spawn(n, outset_noop());
  }
  
  void future(node* producer, outset* producer_out, int continuation_block_id) {
    node* consumer = this;
    consumer->jump_to(continuation_block_id);
    spawn(producer, producer_out);
  }
  
  outset* future(node* producer, int continuation_block_id) {
    outset* producer_out = allocate_future();
    future(producer, producer_out, continuation_block_id);
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
  
  void deallocate_future(outset* future) const {
    assert(! future->should_deallocate_automatically);
    future->destroy();
  }
  
  template <class Body>
  void parallel_for_nested(long lo, long hi, const Body& body, int continuation_block_id) {
    node* consumer = this;
    node* producer = new_parallel_for(lo, hi, consumer, body);
    prepare_node(producer, incounter_ready(), outset_unary());
    prepare_for_transfer(continuation_block_id);
    join_with(consumer, incounter_new(this));
    add_edge(producer, consumer);
    add_node(producer);
  }
  
  template <class Body>
  void parallel_for_rng(long lo, long hi, int cutoff, const Body& body, int continuation_block_id) {
    node* consumer = this;
    node* producer = new_parallel_for(lo, hi, consumer, cutoff, body);
    prepare_node(producer, incounter_ready(), outset_unary());
    prepare_for_transfer(continuation_block_id);
    join_with(consumer, incounter_new(this));
    add_edge(producer, consumer);
    add_node(producer);
  }
  
  template <class Body>
  void parallel_for(long lo, long hi, int cutoff, const Body& body, int continuation_block_id) {
    parallel_for_rng(lo, hi, cutoff, [&] (long lo, long hi) {
      for (long i = lo; i < hi; i++) {
        body(i);
      }
    }, continuation_block_id);
  }
  
  template <class Body>
  void parallel_for(long lo, long hi, const Body& body, int continuation_block_id) {
    parallel_for(lo, hi, communication_delay, body, continuation_block_id);
  }
  
  void split_with(node* n) {
    prepare_node(n, incounter_ready(), outset_noop());
  }
  
  void split_with(node* n, node* join) {
    prepare_node(n, incounter_ready(), outset_unary());
    add_edge(n, join);
  }
  
  outset_type* split_and_join_with(node* n) {
    outset_type* result = allocate_future();
    prepare_node(n, incounter_ready(), result);
    return result;
  }
  
  void call(node* target, int continuation_block_id) {
    finish(target, continuation_block_id);
  }
  
  void fork2(node* producer1, node* producer2, int continuation_block_id) {
    node* consumer = this;
    prepare_node(producer1, incounter_ready(), outset_unary());
    prepare_node(producer2, incounter_ready(), outset_unary());
    join_with(consumer, incounter_new(this));
    add_edge(producer1, consumer);
    add_edge(producer2, consumer);
    add_node(producer1);
    add_node(producer2);
    prepare_for_transfer(continuation_block_id);
  }
  
  void detach(int continuation_block_id) {
    prepare_for_transfer(continuation_block_id);
    join_with(this, incounter_ready());
  }
  
  void set_inport_mode(port_passing_mode mode) {
    // nothing to do
  }
  
  void set_outport_mode(port_passing_mode mode) {
    // nothing to do
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
  } else if (edge_algorithm == edge_algorithm_statreeopt) {
    return new statreeopt::statreeopt_incounter(n);
  } else if (edge_algorithm == edge_algorithm_growabletree) {
    return new growabletree::growabletree_incounter(n);
  } else {
    assert(false);
    return nullptr;
  }
}
  
const bool enable_statreeopt = true;

outset* outset_unary() {
  if (enable_statreeopt && edge_algorithm == edge_algorithm_statreeopt) {
    return (outset*)pasl::sched::outstrategy::direct_statreeopt_unary_new(nullptr);
  } else if (edge_algorithm == edge_algorithm_growabletree) {
    return (outset*)pasl::sched::outstrategy::direct_growabletree_unary_new(nullptr);
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
  } else if (edge_algorithm == edge_algorithm_statreeopt) {
    return new growabletree::growabletree_outset;
  } else if (edge_algorithm == edge_algorithm_growabletree) {
    return new growabletree::growabletree_outset;
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
    source = enable_statreeopt ? source : nullptr;
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
    source = enable_statreeopt ? source : nullptr;
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
  assert(tag != pasl::sched::outstrategy::DIRECT_GROWABLETREE_UNARY_TAG);
  if (tag == pasl::sched::outstrategy::UNARY_TAG) {
    source->out = pasl::data::tagged::create<pasl::sched::thread_p, pasl::sched::outstrategy_p>(target, tag);
    return outset::insert_success;
  } else if (tag == pasl::sched::outstrategy::DIRECT_STATREEOPT_UNARY_TAG) {
    auto target_in = (statreeopt::statreeopt_incounter*)target->in;
    long tg = pasl::sched::instrategy::extract_tag(target_in);
    if ((tg == 0) && (edge_algorithm == edge_algorithm_statreeopt)) {
      auto t = (pasl::sched::thread_p)target_in->nzi.get_target_of_value(source);
      source->out = pasl::sched::outstrategy::direct_statreeopt_unary_new(t);
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
  
void add_edge_growabletree(node* source, outset* source_out,
                           node* target, growabletree::growabletree_incounter* target_in) {
  auto target_nzi_node = target_in->nzi.get_target_of_value(source);
  target_nzi_node->increment();
  long tag = pasl::sched::outstrategy::extract_tag(source_out);
  assert(tag == pasl::sched::outstrategy::DIRECT_GROWABLETREE_UNARY_TAG);
  auto t = (pasl::sched::thread_p)target_nzi_node;
  source->out = pasl::sched::outstrategy::direct_growabletree_unary_new(t);
}

void add_edge(node* source, outset* source_out, node* target, incounter* target_in) {
  if (pasl::sched::instrategy::extract_tag(target_in) == 0 && target_in->is_growabletree()) {
    auto t = (growabletree::growabletree_incounter*)target_in;
    add_edge_growabletree(source, source_out, target, t);
    return;
  }
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
  
template <class Body>
class lazy_parallel_for_0_rec : public node {
public:
  
  long lo;
  long hi;
  node* join;
  int cutoff;
  Body _body;
  
  lazy_parallel_for_0_rec(long lo, long hi, node* join, int cutoff, const Body& _body)
  : lo(lo), hi(hi), join(join), cutoff(cutoff), _body(_body) { }
  
  enum {
    loop_header,
    loop_body,
    exit
  };
  
  void body() {
    switch (node::current_block_id) {
      case loop_header: {
        if (lo < hi) {
          node::jump_to(loop_body);
        }
        break;
      }
      case loop_body: {
        assert(lo < hi);
        long n = std::min(hi, lo + (long)cutoff);
        _body(lo, n);
        lo += n - lo;
        node::jump_to(loop_header);
        break;
      }
    }
  }
  
  size_t size() {
    return hi - lo;
  }
  
  pasl::sched::thread_p split(size_t) {
    long mid = (hi + lo) / 2;
    auto n = new lazy_parallel_for_0_rec(mid, hi, join, cutoff, _body);
    node::split_with(n, join);
    hi = mid;
    return n;
  }
  
};

template <class Body>
class lazy_parallel_for_n_rec : public node {
public:
  
  long lo;
  long hi;
  node* join;
  Body _body;
  
  lazy_parallel_for_n_rec(long lo, long hi, node* join, const Body& _body)
  : lo(lo), hi(hi), join(join), _body(_body) { }
  
  enum {
    loop_header,
    loop_body,
    exit
  };
  
  void body() {
    switch (node::current_block_id) {
      case loop_header: {
        if (lo < hi) {
          node::jump_to(loop_body);
        }
        break;
      }
      case loop_body: {
        assert(lo < hi);
        node::async(_body(lo), join,
                    loop_header);
        lo++;
        break;
      }
    }
  }
  
  size_t size() {
    return hi - lo;
  }
  
  pasl::sched::thread_p split(size_t) {
    long mid = (hi + lo) / 2;
    auto n = new lazy_parallel_for_n_rec(mid, hi, join, _body);
    node::split_with(n, join);
    hi = mid;
    return n;
  }
  
};
  
template <class Body>
node* new_parallel_for(long lo, long hi, node* join, const Body& body) {
  return new lazy_parallel_for_n_rec<Body>(lo, hi, join, body);
}
  
template <class Body>
node* new_parallel_for(long lo, long hi, node* join, int cutoff, const Body& body) {
  return new lazy_parallel_for_0_rec<Body>(lo, hi, join, cutoff, body);
}
  
namespace growabletree {

class outset_finish_parallel_rec : public node {
public:
  
  enum {
    process_block=0,
    repeat_block,
    exit_block
  };
  
  set_type* set;
  std::deque<node_type*> todo;
  
  outset_finish_parallel_rec(set_type* set, node_type* n)
  : set(set) {
    todo.push_back(n);
  }
  
  outset_finish_parallel_rec(set_type* set, std::deque<node_type*>& _todo)
  : set(set) {
    _todo.swap(todo);
  }
  
  void body() {
    switch (current_block_id) {
      case process_block: {
        jump_to(repeat_block);
        set->finish_partial(communication_delay, todo, [&] (node* n) {
          decrement_incounter(n);
        });
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
  
  size_t size() {
    return todo.size();
  }
  
  pasl::sched::thread_p split(size_t) {
    assert(size() >= 2);
    auto n = todo.front();
    todo.pop_front();
    auto t = new outset_finish_parallel_rec(set, n);
    prepare_node(t, incounter_ready(), outset_noop());
    return t;
  }
  
};

class outset_finish_parallel : public node {
public:
  
  enum {
    entry_block=0,
    exit_block
  };
  
  growabletree_outset* out;
  std::deque<node_type*> todo;
  
  outset_finish_parallel(growabletree_outset* out, std::deque<node_type*>& _todo)
  : out(out) {
    todo.swap(_todo);
  }
  
  void body() {
    switch (current_block_id) {
      case entry_block: {
        call(new outset_finish_parallel_rec(&(out->set), todo),
             exit_block);
        break;
      }
      case exit_block: {
        assert(! out->should_deallocate_automatically);
        break;
      }
      default:
        break;
    }
  }
  
};

void outset_finish(growabletree_outset* out) {
  assert(! out->should_deallocate_automatically);
  std::deque<node_type*> todo;
  node_type* n = out->set.finish_init([&] (node* n) {
    decrement_incounter(n);
  });
  if (n != nullptr) {
    todo.push_back(n);
  }
  if (! todo.empty()) {
    node* n;
    n = new outset_finish_parallel(out, todo);
    prepare_node(n, incounter_ready(), outset_noop());
    add_node(n);
  } else {
    if (out->should_deallocate_automatically) {
      delete out;
    }
  }
}
  
class outset_tree_deallocate_parallel : public node {
public:
  
  enum {
    process_block=0,
    repeat_block
  };
  
  std::deque<node_type*> todo;
  
  void body() {
    switch (current_block_id) {
      case process_block: {
        jump_to(repeat_block);
        set_type::deallocate_partial(communication_delay, todo);
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
  
  size_t size() {
    return todo.size();
  }
  
  pasl::sched::thread_p split(size_t) {
    assert(size() >= 2);
    auto n = todo.front();
    todo.pop_front();
    auto t = new outset_tree_deallocate_parallel;
    prepare_node(t, incounter_ready(), outset_noop());
    t->todo.push_back(n);
    return t;
  }
  
};
  
void outset_tree_deallocate_sequential(node_type* root) {
  outset_tree_deallocate_parallel d;
  d.todo.push_back(root);
  while (! d.todo.empty()) {
    set_type::deallocate_partial(communication_delay, d.todo);
  }
}

void outset_tree_deallocate(growabletree_outset* out) {
  assert(! out->should_deallocate_automatically);
  node_type* root = out->set.get_root();
  if (root == nullptr) {
    return;
  }
  if (should_deallocate_sequentially) {
    outset_tree_deallocate_sequential(root);
    return;
  }
  outset_tree_deallocate_parallel d;
  d.todo.push_back(root);
  set_type::deallocate_partial(communication_delay, d.todo);
  if (! d.todo.empty()) {
    node* n = new outset_tree_deallocate_parallel(d);
    prepare_node(n, incounter_ready(), outset_noop());
    add_node(n);
  }
}

} // end namespace
  
} // end namespace

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/* The bottom-up algorithm */

namespace portpassing {
  
class node;
class incounter;
class outset;
class incounter_node;
class outset_node;
  
using inport_map_type = std::unordered_map<incounter*, incounter_node*>;
using outport_map_type = std::unordered_map<outset*, outset_node*>;
  
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
void propagate_ports_for(node*, node*);
outset_node* find_outport(node*, outset*);
outset* capture_outset();
void join_with(node*, incounter*);
void continue_with(node*);
incounter_node* increment_incounter(node*);
std::pair<incounter_node*, incounter_node*> increment_incounter(node*, incounter_node*);
std::pair<incounter_node*, incounter_node*> increment_incounter(node*, node*);
void decrement_incounter(node*, incounter_node*);
void decrement_incounter(node*, incounter*, incounter_node*);
void decrement_inports(node*);
void insert_inport(node*, incounter*, incounter_node*);
void insert_inport(node*, node*, incounter_node*);
void insert_outport(node*, outset*, outset_node*);
void insert_outport(node*, node*, outset_node*);
void deallocate_future(node*, outset*);
void outset_finish(outset*);
void outset_tree_deallocate(outset_node*);
template <class Body>
node* new_parallel_for(long, long, node*, int, const Body&);
  
class incounter_node {
public:
  
  incounter_node* parent;
  std::atomic<int> nb_removed_children;
  
  incounter_node() {
    parent = nullptr;
    nb_removed_children.store(0);
  }
  
};

class outset_node {
public:
  
  node* target;
  incounter_node* port;
  std::atomic<outset_node*> children[2];
  
  outset_node() {
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
  
  bool is_activated(incounter_node* port) const {
    return port->parent == nullptr;
  }
  
  std::pair<incounter_node*, incounter_node*> increment(incounter_node* port) const {
    incounter_node* branch1;
    incounter_node* branch2;
    if (port == nullptr) {
      branch1 = new incounter_node;
      branch2 = nullptr;
    } else {
      branch1 = new incounter_node;
      branch2 = new incounter_node;
      branch1->parent = port;
      branch2->parent = port;
    }
    return std::make_pair(branch1, branch2);
  }
  
  incounter_node* increment() const {
    return increment((incounter_node*)nullptr).first;
  }
  
  status_type decrement(incounter_node* port) const {
    assert(port != nullptr);
    incounter_node* current = port;
    incounter_node* next = current->parent;
    while (next != nullptr) {
      delete current;
      while (true) {
        if (next->nb_removed_children.load() != 0) {
          break;
        }
        int orig = 0;
        if (compare_exchange(next->nb_removed_children, orig, 1)) {
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
  
  using insert_result_type = std::pair<insert_status_type, outset_node*>;
  
  static constexpr int frozen_tag = 1;
  
  outset_node* root;
  
  node* n;
  
  outset(node* n)
  : n(n) {
    root = new outset_node;
  }
  
  ~outset() {
    outset_tree_deallocate(root);
  }
  
  outset_node* find_leaf() const {
    outset_node* current = root;
    while (true) {
      int i;
      for (i = 0; i < 2; i++) {
        if (current->children[i].load() != nullptr) {
          break;
        }
      }
      if (i == 2) {
        return current;
      } else {
        current = current->children[i].load();
      }
    }
  }
  
  bool is_finished() const {
    int tag = tagged_tag_of(root->children[0].load());
    return tag == frozen_tag;
  }
  
  insert_result_type insert(outset_node* outport, node* target, incounter_node* inport) {
    if (is_finished()) {
      return std::make_pair(insert_fail, nullptr);
    }
    outset_node* next = new outset_node;
    next->target = target;
    next->port = inport;
    outset_node* orig = nullptr;
    if (! (compare_exchange(outport->children[0], orig, next))) {
      delete next;
      return std::make_pair(insert_fail, nullptr);
    }
    return std::make_pair(insert_success, next);
  }
  
  std::pair<outset_node*, outset_node*> fork2(outset_node* port) const {
    assert(port != nullptr);
    outset_node* branches[2];
    for (int i = 1; i >= 0; i--) {
      branches[i] = new outset_node;
      outset_node* orig = nullptr;
      if (! compare_exchange(port->children[i], orig, branches[i])) {
        delete branches[i];
        return std::make_pair(nullptr, nullptr);
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
    outset_finish(this);
  }
  
  bool should_deallocate_automatically = true;
  
  void enable_future() {
    should_deallocate_automatically = false;
  }
  
  void set_node(node* _n) {
    assert(n == nullptr);
    assert(_n != nullptr);
    n = _n;
  }
  
};
  
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
  
  port_passing_mode inport_mode = port_passing_default;
  port_passing_mode outport_mode = port_passing_default;
  
  void set_inport_mode(port_passing_mode mode) {
    inport_mode = mode;
  }
  
  void set_outport_mode(port_passing_mode mode) {
    outport_mode = mode;
  }
  
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
      decrement_incounter(p.first->n, p.first, p.second);
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
  
  void spawn(node* n) {
    assert(false);
  }
  
  void async(node* producer, node* consumer, int continuation_block_id) {
    prepare_node(producer, incounter_ready(), outset_unary(producer));
    node* caller = this;
    insert_inport(producer, (incounter*)consumer->in, (incounter_node*)nullptr);
    propagate_ports_for(caller, producer);
    caller->jump_to(continuation_block_id);
    add_node(producer);
  }
  
  void finish(node* producer, int continuation_block_id) {
    prepare_node(producer, incounter_ready(), outset_unary(producer));
    node* consumer = this;
    join_with(consumer, new incounter(consumer));
    propagate_ports_for(consumer, producer);
    incounter_node* consumer_inport = increment_incounter(consumer);
    insert_inport(producer, consumer, consumer_inport);
    consumer->prepare_for_transfer(continuation_block_id);
    add_node(producer);
  }
  
  static outset* allocate_future() {
    outset* out = outset_new(nullptr);
    out->enable_future();
    return out;
  }
  
  void listen_on(outset* out) {
    node* caller = this;
    insert_outport(caller, out, out->find_leaf());
  }
  
  void future(node* producer, outset* producer_out, int continuation_block_id) {
    prepare_node(producer, incounter_ready(), producer_out);
    producer_out->set_node(producer);
    node* caller = this;
    propagate_ports_for(caller, producer);
    listen_on(producer_out);
    caller->jump_to(continuation_block_id);
    add_node(producer);
  }
  
  outset* future(node* producer, int continuation_block_id) {
    outset* producer_out = allocate_future();
    future(producer, producer_out, continuation_block_id);
    return producer_out;
  }
  
  void force(outset* producer_out, int continuation_block_id) {
    node* consumer = this;
    prepare_for_transfer(continuation_block_id);
    join_with(consumer, incounter_unary());
    outset::insert_result_type insert_result;
    if (producer_out->is_finished()) {
      insert_result.first = outset::insert_fail;
    } else {
      outset_node* source_outport = find_outport(consumer, producer_out);
      insert_result = producer_out->insert(source_outport, consumer, (incounter_node*)nullptr);
    }
    outset::insert_status_type insert_status = insert_result.first;
    if (insert_status == outset::insert_success) {
      outset_node* producer_outport = insert_result.second;
      insert_outport(consumer, producer_out, producer_outport);
    } else if (insert_status == outset::insert_fail) {
      add_node(consumer);
    } else {
      assert(false);
    }
    consumer->outports.erase(producer_out);
  }
  
  void deallocate_future(outset* future) {
    portpassing::deallocate_future(this, future);
  }
  
  template <class Body>
  void parallel_for_nested(long lo, long hi, const Body& body, int continuation_block_id) {
    assert(false);
  }
  
  template <class Body>
  void parallel_for_rng(long lo, long hi, int cutoff, const Body& body, int continuation_block_id) {
    assert(false);
  }
  
  template <class Body>
  void parallel_for(long lo, long hi, int cutoff, const Body& body, int continuation_block_id) {
    node* consumer = this;
    node* producer = new_parallel_for(lo, hi, consumer, cutoff, body);
    prepare_node(producer, incounter_ready(), outset_unary(producer));
    join_with(consumer, new incounter(consumer));
    propagate_ports_for(consumer, producer);
    incounter_node* consumer_inport = increment_incounter(consumer);
    insert_inport(producer, consumer, consumer_inport);
    consumer->prepare_for_transfer(continuation_block_id);
    add_node(producer);
  }
  
  template <class Body>
  void parallel_for(long lo, long hi, const Body& body, int continuation_block_id) {
    parallel_for(lo, hi, communication_delay, body, continuation_block_id);
  }
  
  void split_with(node* new_sibling) {
    assert(false);
  }
  
  void split_with(node* new_sibling, node* join) {
    node* caller = this;
    prepare_node(new_sibling);
    propagate_ports_for(caller, new_sibling);
    assert(false);
  }
  
  outset_type* split_and_join_with(node* n) {
    outset_type* result = nullptr;
    assert(false);
    return result;
  }
  
  void call(node* target, int continuation_block_id) {
    finish(target, continuation_block_id);
  }
  
  void fork2(node* producer1, node* producer2, int continuation_block_id) {
    assert(false); // todo
  }
  
  void detach(int continuation_block_id) {
    prepare_for_transfer(continuation_block_id);
    join_with(this, incounter_ready());
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
  return (outset*)pasl::sched::outstrategy::portpassing_unary_new((pasl::sched::thread_p)n);
}

outset* outset_noop() {
  return (outset*)pasl::sched::outstrategy::noop_new();
}
  
outset* outset_new(node* n) {
  return new outset(n);
}
  
void insert_inport(node* caller, incounter* target_in, incounter_node* target_inport) {
  caller->inports.insert(std::make_pair(target_in, target_inport));
}
  
void insert_inport(node* caller, node* target, incounter_node* target_inport) {
  insert_inport(caller, (incounter*)target->in, target_inport);
}

void insert_outport(node* caller, outset* target_out, outset_node* target_outport) {
  assert(target_outport != nullptr);
  caller->outports.insert(std::make_pair(target_out, target_outport));
}
  
void insert_outport(node* caller, node* target, outset_node* target_outport) {
  insert_outport(caller, (outset*)target->out, target_outport);
}

incounter_node* find_inport(node* caller, incounter* target_in) {
  auto target_inport_result = caller->inports.find(target_in);
  assert(target_inport_result != caller->inports.end());
  return target_inport_result->second;
}

outset_node* find_outport(node* caller, outset* target_out) {
  auto target_outport_result = caller->outports.find(target_out);
  assert(target_outport_result != caller->outports.end());
  return target_outport_result->second;
}
  
template <class Map>
void intersect_with(const Map& source, Map& destination) {
  Map result;
  for (auto item : source) {
    if (destination.find(item.first) != destination.cend()) {
      result.insert(item);
    }
  }
  result.swap(destination);
}
  
template <class Map>
void difference_with(const Map& source, Map& destination) {
  Map result;
  for (auto item : source) {
    if (destination.find(item.first) == destination.cend()) {
      result.insert(item);
    }
  }
  result.swap(destination);
}
  
template <class Map>
void propagate_ports_for(port_passing_mode mode, const Map& parent_ports, Map& child_ports) {
  switch (mode) {
    case port_passing_default: {
      child_ports = parent_ports;
      break;
    }
    case port_passing_intersection: {
      intersect_with(parent_ports, child_ports);
      break;
    }
    case port_passing_difference: {
      difference_with(parent_ports, child_ports);
      break;
    }
  }
}
  
void fork_in_ports_for(inport_map_type& parent_ports, inport_map_type& child_ports) {
  for (auto item : parent_ports) {
    if (child_ports.find(item.first) != child_ports.cend()) {
      incounter* in = item.first;
      incounter_node* port = item.second;
      auto new_ports = in->increment(port);
      parent_ports[in] = new_ports.first;
      child_ports[in] = new_ports.second;
    }
  }
}

void fork_out_ports_for(outport_map_type& parent_ports, outport_map_type& child_ports) {
  std::vector<outset*> to_erase;
  for (auto item : parent_ports) {
    if (child_ports.find(item.first) != child_ports.cend()) {
      outset* out = item.first;
      outset_node* port = item.second;
      auto new_ports = out->fork2(port);
      if (new_ports.first == nullptr) {
        to_erase.push_back(out);
      } else {
        parent_ports[out] = new_ports.first;
        child_ports[out] = new_ports.second;
      }
    }
  }
  for (outset* out : to_erase) {
    parent_ports.erase(out);
    child_ports.erase(out);
  }
}
  
void propagate_ports_for(node* parent, node* child) {
  port_passing_mode in_port_mode = child->inport_mode;
  port_passing_mode out_port_mode = child->outport_mode;
  propagate_ports_for(in_port_mode, parent->inports, child->inports);
  fork_in_ports_for(parent->inports, child->inports);
  propagate_ports_for(out_port_mode, parent->outports, child->outports);
  fork_out_ports_for(parent->outports, child->outports);
}

incounter_node* increment_incounter(node* n) {
  incounter* in = (incounter*)n->in;
  return in->increment();
}
  
std::pair<incounter_node*, incounter_node*> increment_incounter(node* n, incounter_node* n_port) {
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
  
std::pair<incounter_node*, incounter_node*> increment_incounter(node* caller, node* target) {
  incounter_node* target_inport = find_inport(caller, (incounter*)target->in);
  return increment_incounter(target, target_inport);
}

void decrement_incounter(node* n, incounter* n_in, incounter_node* n_port) {
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
  
void decrement_incounter(node* n, incounter_node* n_port) {
  decrement_incounter(n, (incounter*)n->in, n_port);
}
  
void decrement_inports(node* n) {
  n->decrement_inports();
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
  
void portpassing_finished(pasl::sched::thread_p t) {
  node* n = tagged_pointer_of((node*)t);
  n->decrement_inports();
}
  
void deallocate_future(node* caller, outset* future) {
  assert(! future->should_deallocate_automatically);
  caller->outports.erase(future);
  delete future;
}
  
template <class Body>
class lazy_parallel_for_rec : public node {
public:
  
  long lo;
  long hi;
  node* join;
  int cutoff;
  Body _body;
  
  lazy_parallel_for_rec(long lo, long hi, node* join, int cutoff, const Body& _body)
  : lo(lo), hi(hi), join(join), cutoff(cutoff), _body(_body) { }
  
  enum {
    process_block,
    repeat_block,
    exit
  };
  
  void body() {
    switch (node::current_block_id) {
      case process_block: {
        long n = std::min(hi, lo + (long)cutoff);
        long i;
        for (i = lo; i < n; i++) {
          _body(i);
        }
        lo = i;
        node::jump_to(repeat_block);
        break;
      }
      case repeat_block: {
        if (lo < hi) {
          node::jump_to(process_block);
        }
        break;
      }
    }
  }
  
  size_t size() {
    return hi - lo;
  }
  
  pasl::sched::thread_p split(size_t) {
    node* consumer = join;
    node* caller = this;
    long mid = (hi + lo) / 2;
    auto producer = new lazy_parallel_for_rec(mid, hi, join, cutoff, _body);
    hi = mid;
    prepare_node(producer);
    insert_inport(producer, (incounter*)consumer->in, (incounter_node*)nullptr);
    propagate_ports_for(caller, producer);
    return producer;
  }
  
};

template <class Body>
node* new_parallel_for(long lo, long hi, node* join, int cutoff, const Body& body) {
  return new lazy_parallel_for_rec<Body>(lo, hi, join, cutoff, body);
}
  
void outset_finish_partial(std::deque<outset_node*>& todo) {
  int k = 0;
  while ( (k < communication_delay) && (! todo.empty()) ) {
    outset_node* n = todo.back();
    todo.pop_back();
    if (n->target != nullptr) {
      decrement_incounter(n->target, n->port);
    }
    for (int i = 0; i < 2; i++) {
      outset_node* orig;
      while (true) {
        orig = n->children[i].load();
        outset_node* next = tagged_tag_with(orig, outset::frozen_tag);
        if (compare_exchange(n->children[i], orig, next)) {
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
  
class outset_finish_and_deallocate_parallel_rec : public node {
public:
  
  enum {
    process_block=0,
    repeat_block,
    exit_block
  };
  
  node* join;
  std::deque<outset_node*> todo;
  
  outset_finish_and_deallocate_parallel_rec(node* join, outset_node* n)
  : join(join) {
    todo.push_back(n);
  }
  
  outset_finish_and_deallocate_parallel_rec(node* join, std::deque<outset_node*>& _todo)
  : join(join) {
    _todo.swap(todo);
  }
  
  void body() {
    switch (current_block_id) {
      case process_block: {
        jump_to(repeat_block);
        outset_finish_partial(todo);
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
  
  size_t size() {
    return todo.size();
  }
  
  pasl::sched::thread_p split(size_t) {
    assert(size() >= 2);
    auto n = todo.front();
    todo.pop_front();
    node* consumer = join;
    node* caller = this;
    auto producer = new outset_finish_and_deallocate_parallel_rec(join, n);
    prepare_node(producer);
    insert_inport(producer, (incounter*)consumer->in, (incounter_node*)nullptr);
    propagate_ports_for(caller, producer);
    return producer;
  }
  
};
  
class outset_finish_and_deallocate_parallel : public node {
public:
  
  enum {
    entry_block=0,
    exit_block
  };
  
  outset* out;
  std::deque<outset_node*> todo;
  
  outset_finish_and_deallocate_parallel(outset* out, std::deque<outset_node*>& _todo)
  : out(out) {
    todo.swap(_todo);
  }
  
  void body() {
    switch (current_block_id) {
      case entry_block: {
        finish(new outset_finish_and_deallocate_parallel_rec(this, todo),
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

class outset_finish_parallel_rec : public node {
public:
  
  enum {
    process_block=0,
    repeat_block,
    exit_block
  };
  
  std::deque<outset_node*> todo;
  
  outset_finish_parallel_rec(outset_node* n) {
    todo.push_back(n);
  }
  
  outset_finish_parallel_rec(std::deque<outset_node*>& _todo) {
    _todo.swap(todo);
  }
  
  void body() {
    switch (current_block_id) {
      case process_block: {
        jump_to(repeat_block);
        outset_finish_partial(todo);
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
  
  size_t size() {
    return todo.size();
  }
  
  pasl::sched::thread_p split(size_t) {
    assert(size() >= 2);
    auto n = todo.front();
    todo.pop_front();
    auto t = new outset_finish_parallel_rec(n);
    prepare_node(t);
    return t;
  }
  
};

class outset_finish_parallel : public node {
public:
  
  enum {
    entry_block=0,
    exit_block
  };
  
  outset* out;
  std::deque<outset_node*> todo;
  
  outset_finish_parallel(outset* out, std::deque<outset_node*>& _todo)
  : out(out) {
    todo.swap(_todo);
  }
  
  void body() {
    switch (current_block_id) {
      case entry_block: {
        call(new outset_finish_parallel_rec(todo),
               exit_block);
        break;
      }
      case exit_block: {
        assert(! out->should_deallocate_automatically);
        break;
      }
      default:
        break;
    }
  }
  
};
  
void outset_finish(outset* out) {
  std::deque<outset_node*> todo;
  todo.push_back(out->root);
  outset_finish_partial(todo);
  if (! todo.empty()) {
    node* n;
    if (out->should_deallocate_automatically) {
      n = new outset_finish_and_deallocate_parallel(out, todo);
    } else {
      n = new outset_finish_parallel(out, todo);
    }
    prepare_node(n);
    add_node(n);
  } else {
    if (out->should_deallocate_automatically) {
      delete out;
    }
  }
}
  
void outset_tree_deallocate_partial(std::deque<outset_node*>& todo) {
  int k = 0;
  while ( (k < communication_delay) && (! todo.empty()) ) {
    outset_node* n = todo.back();
    todo.pop_back();
    for (int i = 0; i < 2; i++) {
      outset_node* child = tagged_pointer_of(n->children[i].load());
      if (child != nullptr) {
        todo.push_back(child);
      }
    }
    delete n;
    k++;
  }
}

class outset_tree_deallocate_parallel : public node {
public:
  
  enum {
    process_block=0,
    repeat_block
  };
  
  std::deque<outset_node*> todo;
  
  void body() {
    switch (current_block_id) {
      case process_block: {
        outset_tree_deallocate_partial(todo);
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
  
  size_t size() {
    return todo.size();
  }
  
  pasl::sched::thread_p split(size_t) {
    assert(size() >= 2);
    auto n = todo.front();
    todo.pop_front();
    auto t = new outset_tree_deallocate_parallel;
    prepare_node(t);
    t->todo.push_back(n);
    return t;
  }
  
};
  
void outset_tree_deallocate(outset_node* root) {
  outset_tree_deallocate_parallel d;
  d.todo.push_back(root);
  outset_tree_deallocate_partial(d.todo);
  if (! d.todo.empty()) {
    node* n = new outset_tree_deallocate_parallel(d);
    prepare_node(n);
    add_node(n);
  }
}
  
} // end namespace

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/* Test programs */

namespace tests {
  
template <class node>
using outset_of = typename node::outset_type;
  
std::atomic<int> async_leaf_counter;
std::atomic<int> async_interior_counter;

template <class node>
class async_bintree_rec : public node {
public:
  
  enum {
    async_bintree_rec_entry,
    async_bintree_rec_mid,
    async_bintree_rec_exit
  };
  
  int lo;
  int hi;
  node* consumer;
  
  int mid;
  
  async_bintree_rec(int lo, int hi, node* consumer)
  : lo(lo), hi(hi), consumer(consumer) { }
  
  void body() {
    switch (node::current_block_id) {
      case async_bintree_rec_entry: {
        int n = hi - lo;
        if (n == 0) {
          return;
        } else if (n == 1) {
          async_leaf_counter.fetch_add(1);
        } else {
          async_interior_counter.fetch_add(1);
          mid = (lo + hi) / 2;
          node::async(new async_bintree_rec(lo, mid, consumer), consumer,
                      async_bintree_rec_mid);
        }
        break;
      }
      case async_bintree_rec_mid: {
        node::async(new async_bintree_rec(mid, hi, consumer), consumer,
                    async_bintree_rec_exit);
        break;
      }
      case async_bintree_rec_exit: {
        break;
      }
      default:
        break;
    }
  }
  
};

template <class node>
class async_bintree : public node {
public:
  
  enum {
    async_bintree_entry,
    async_bintree_exit
  };
  
  int n;
  
  async_bintree(int n)
  : n(n) { }
  
  void body() {
    switch (node::current_block_id) {
      case async_bintree_entry: {
        async_leaf_counter.store(0);
        async_interior_counter.store(0);
        node::finish(new async_bintree_rec<node>(0, n, this),
                     async_bintree_exit);
        break;
      }
      case async_bintree_exit: {
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
class future_bintree_rec : public node {
public:
  
  enum {
    future_bintree_entry,
    future_bintree_branch2,
    future_bintree_force1,
    future_bintree_force2,
    future_bintree_exit
  };
  
  int lo;
  int hi;
  
  outset_of<node>* branch1_out;
  outset_of<node>* branch2_out;
  
  int mid;
  
  future_bintree_rec(int lo, int hi)
  : lo(lo), hi(hi) { }
  
  void body() {
    switch (node::current_block_id) {
      case future_bintree_entry: {
        int n = hi - lo;
        if (n == 0) {
          return;
        } else if (n == 1) {
          future_leaf_counter.fetch_add(1);
        } else {
          mid = (lo + hi) / 2;
          branch1_out = node::future(new future_bintree_rec<node>(lo, mid),
                                     future_bintree_branch2);
        }
        break;
      }
      case future_bintree_branch2: {
        branch2_out = node::future(new future_bintree_rec<node>(mid, hi),
                                   future_bintree_force1);
        break;
      }
      case future_bintree_force1: {
        node::force(branch1_out,
                    future_bintree_force2);
        break;
      }
      case future_bintree_force2: {
        node::force(branch2_out,
                    future_bintree_exit);
        break;
      }
      case future_bintree_exit: {
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
class future_bintree : public node {
public:
  
  enum {
    future_bintree_entry,
    future_bintree_force,
    future_bintree_exit
  };
  
  int n;
  
  outset_of<node>* root_out;
  
  future_bintree(int n)
  : n(n) { }
  
  void body() {
    switch (node::current_block_id) {
      case future_bintree_entry: {
        future_leaf_counter.store(0);
        future_interior_counter.store(0);
        root_out = node::future(new future_bintree_rec<node>(0, n),
                                future_bintree_force);
        break;
      }
      case future_bintree_force: {
        node::force(root_out,
                    future_bintree_exit);
        break;
      }
      case future_bintree_exit: {
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

template <class node>
class parallel_for_test : public node {
public:
  
  long n;
  int* array;
  
  enum {
    entry,
    exit
  };
  
  parallel_for_test(long n)
  : n(n) { }
  
  bool check() {
    for (long i = 0; i < n; i++) {
      if (array[i] != i) {
        return false;
      }
    }
    return true;
  }
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        array = (int*)malloc(n* sizeof(int));
        node::parallel_for(0, n, [&] (long i) {
          array[i] = (int)i;
        }, exit);
        break;
      }
      case exit: {
        assert(check());
        free(array);
        break;
      }
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
  
template <class Body_gen, class node>
class eager_parallel_for_rec : public node {
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
  
  eager_parallel_for_rec(int lo, int hi, Body_gen body_gen, node* join)
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
          node::async(new eager_parallel_for_rec(lo, mid, body_gen, join), join,
                      parallel_for_rec_branch2);
        }
        break;
      }
      case parallel_for_rec_branch2: {
        node::async(new eager_parallel_for_rec(mid, hi, body_gen, join), join,
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
class eager_parallel_for : public node {
public:
  
  enum {
    parallel_for_entry,
    parallel_for_exit
  };
  
  int lo;
  int hi;
  Body_gen body_gen;
  
  eager_parallel_for(int lo, int hi, Body_gen body_gen)
  : lo(lo), hi(hi), body_gen(body_gen) { }
  
  void body() {
    switch (node::current_block_id) {
      case parallel_for_entry: {
        node::finish(new eager_parallel_for_rec<Body_gen, node>(lo, hi, body_gen, this),
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
        f = node::future(new future_body<node>,
                         future_pool_call);
        break;
      }
      case future_pool_call: {
        auto loop_body = [=] (int i) {
          return new future_reader<node>(f, i);
        };
        node::call(new eager_parallel_for<decltype(loop_body), node>(0, n, loop_body),
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
  
} // end namespace

/*---------------------------------------------------------------------*/
/* Benchmark programs */

namespace benchmarks {
  
template <class node>
using outset_of = typename node::outset_type;
  
template <class Snzi>
void benchmark_snzi_thread(int my_id,
                           Snzi& snzi,
                           bool& should_stop,
                           long& nb_operations1,
                           long& nb_operations2,
                           unsigned int seed) {
  using node_type = typename Snzi::node_type;
  long c = 0;
  while (! should_stop) {
    node_type* target = snzi.get_target_for(my_id);
    snzi.increment(target);
    snzi.decrement(target);
    c++;
  }
  nb_operations1 = c;
  nb_operations2 = 0;
}

class single_cell_snzi_wrapper {
public:
  
  using snzi_type = std::atomic<int>;
  using node_type = void*;
  
  snzi_type snzi;

  node_type* get_target_for(int id) {
    return nullptr;
  }
  
  void increment(node_type* target) {
    snzi++;
  }
  
  void decrement(node_type* target) {
    snzi--;
  }
  
};

  
class fixed_size_snzi_wrapper {
public:
  
  using snzi_type = pasl::data::snzi::tree<direct::statreeopt::snzi_tree_height>;
  using node_type = typename snzi_type::node_type;
  
  snzi_type snzi;

  node_type* get_target_for(int id) {
    //    return snzi.get_target_of_value(id);
    return snzi.ith_leaf_node(id);
  }
  
  void increment(node_type* target) {
    target->increment();
  }
  
  void decrement(node_type* target) {
    target->decrement();
  }
  
};
  
class growable_size_snzi_wrapper {
public:
  
  using snzi_type = pasl::data::gsnzi::tree<>;
  using node_type = typename snzi_type::node_type;
  
  snzi_type snzi;
  
  node_type* get_target_for(int id) {
    //return snzi.get_target_of_value(id);
    return snzi.get_target_of_path(id);
  }
  
  void increment(node_type* target) {
    target->increment();
  }
  
  void decrement(node_type* target) {
    target->decrement();
  }
  
};

template <class Incounter>
void benchmark_incounter_thread(int my_id,
                                Incounter& incounter,
                                bool& should_stop,
                                long& nb_operations1,
                                long& nb_operations2,
                                unsigned int seed) {
#ifndef USE_STL_RANDGEN
  unsigned int rng = seed+my_id;
  auto random_int = [&] (int lo, int hi) {
    int r = random_int_in_range(rng, lo, hi);
    return r;
  };
#else
  std::mt19937 generator(seed+my_id);
  auto random_int = [&] (int lo, int hi) {
    std::uniform_int_distribution<int> distribution(lo, hi-1);
    return distribution(generator);
  };
#endif
  long c = 0;
  long nb_pending_increments = 0;
  int incr_prob_a = pasl::util::cmdline::parse_int("incr_prob_a");
  int incr_prob_b = pasl::util::cmdline::parse_int("incr_prob_b");
  if (incr_prob_a < 0 && incr_prob_a > incr_prob_b) {
    pasl::util::atomic::die("bogus incr_prob");
  }
  auto should_increment = [&] {
    return random_int(0, incr_prob_b) < incr_prob_a;
  };
  while (! should_stop) {
    if ((nb_pending_increments > 0) && (! should_increment())) {
      nb_pending_increments--;
      incounter.decrement(my_id, random_int);
    } else {
      nb_pending_increments++;
      incounter.increment(my_id, random_int);
    }
    c++;
  }
  nb_operations1 = c;
  nb_operations2 = nb_pending_increments;
  while (nb_pending_increments > 0) {
    incounter.decrement(my_id, random_int);
    nb_pending_increments--;
  }
}
  
class simple_incounter_wrapper {
public:
  
  std::atomic<int> counter;
  
  simple_incounter_wrapper() {
    counter.store(0);
  }
  
  template <class Random_int>
  void increment(int, const Random_int&) {
    counter++;
  }
  
  template <class Random_int>
  bool decrement(int, const Random_int&) {
    return counter-- == 0;
  }
  
  bool is_activated() const {
    return counter.load() == 0;
  }
  
};

class snzi_incounter_wrapper {
public:
  
  pasl::data::snzi::tree<direct::statreeopt::snzi_tree_height> snzi;
  
  snzi_incounter_wrapper() { }
  
  int my_leaf_node(int hash) const {
    return abs(hash) % snzi.get_nb_leaf_nodes();
  }
  
  template <class Random_int>
  void increment(int hash, const Random_int&) {
    int i = my_leaf_node(hash);
    snzi.ith_leaf_node(i)->increment();
  }
  
  template <class Random_int>
  bool decrement(int hash, const Random_int&) {
    int i = my_leaf_node(hash);
    return snzi.ith_leaf_node(i)->decrement();
  }
  
  bool is_activated() const {
    return ! snzi.is_nonzero();
  }
  
};

template <class Outset>
void benchmark_outset_thread(int my_id,
                             Outset& outset,
                             bool& should_stop,
                             long& nb_operations,
                             unsigned int seed) {
#ifndef USE_STL_RANDGEN
  unsigned int rng = seed+my_id;
  auto random_int = [&] (int lo, int hi) {
    return random_int_in_range(rng, lo, hi);
  };
#else
  std::mt19937 generator(seed+my_id);
  auto random_int = [&] (int lo, int hi) {
    std::uniform_int_distribution<int> distribution(lo, hi-1);
    return distribution(generator);
  };
#endif
  long c = 0;
  while (! should_stop) {
    outset.add((void*)nullptr, random_int, my_id);
    c++;
  }
  nb_operations = c;
}
  
class simple_outset_wrapper {
public:
  
  direct::simple::simple_outset outset;
  
  template <class Random_int>
  void add(void*, const Random_int&, int) {
    outset.insert(nullptr);
  }
  
};

void* dummyval = (void*) (((long*)nullptr) + 500);

class growable_outset_wrapper {
public:

  static constexpr int branching_factor = 4;
  static constexpr int block_capacity = 4096;

  pasl::data::outset::outset<direct::node*, branching_factor, block_capacity> set;

  template <class Random_int>
  void add(void*, const Random_int& random_int, int my_id) {
    bool b = set.insert((direct::node*)dummyval, my_id, random_int);
    assert(b);
  }
  
};
  
class perprocessor_outset_wrapper {
public:
  
  static constexpr int nb_buffers = 64;
  static constexpr int buffer_sz = 4096;
  
  using buffer_descriptor_type = struct buffer_descriptor_struct {
    direct::node** head;
    direct::node** start;
    buffer_descriptor_struct* next;
  };
  
  pasl::data::outset::static_cache_aligned_array<buffer_descriptor_type, nb_buffers> buffers;
  
  void allocate_fresh_buffer_for(buffer_descriptor_type& my_descr) {
    direct::node** p = (direct::node**)malloc(sizeof(direct::node*) * buffer_sz);
    assert(p != nullptr);
    my_descr.next = new buffer_descriptor_type(my_descr);
    my_descr.head = p;
    my_descr.start = p;
  }
  
  void push(buffer_descriptor_type& my_descr) {
    *my_descr.head = (direct::node*)dummyval;
    my_descr.head++;
  }
  
  perprocessor_outset_wrapper() {
    for (int i = 0; i < nb_buffers; i++) {
      buffers[i].next = nullptr;
      allocate_fresh_buffer_for(buffers[i]);
    }
  }
  
  ~perprocessor_outset_wrapper() {
    for (int i = 0; i < nb_buffers; i++) {
      buffer_descriptor_type& my_descr = buffers[i];
      free(my_descr.start);
      buffer_descriptor_type* next = my_descr.next;
      while (next != nullptr) {
        free(next->start);
        buffer_descriptor_type* tmp = next;
        next = next->next;
        delete tmp;
      }
    }
  }
  
  template <class Random_int>
  void add(void*, const Random_int& random_int, int my_id) {
    buffer_descriptor_type& my_descr = buffers[my_id];
    if (my_descr.head >= (my_descr.start + buffer_sz)) {
      allocate_fresh_buffer_for(my_descr);
    }
    push(my_descr);
  }
  
};
  
class single_buffer_outset_wrapper {
public:
  
  static constexpr int buffer_sz = 4096;

  using buffer_type = pasl::data::outset::block<direct::node*, buffer_sz, true>;
  
  static std::atomic<buffer_type*> buffer;
  
  single_buffer_outset_wrapper() {
    buffer.store(new buffer_type);
  }
  
  template <class Random_int>
  void add(void*, const Random_int& random_int, int my_id) {
    while (true) {
      buffer_type* b = buffer.load();
      bool failed_because_finish = false;
      bool failed_because_full = false;
      b->try_insert((direct::node*)dummyval, failed_because_finish, failed_because_full);
      assert(! failed_because_finish);
      if (failed_because_full) {
        buffer_type* new_buffer = new buffer_type;
        if (! compare_exchange(buffer, b, new_buffer)) {
          delete new_buffer;
        }
      } else {
        break;
      }
    }
  }
  
};

std::atomic<typename single_buffer_outset_wrapper::buffer_type*> single_buffer_outset_wrapper::buffer;
  
double since(std::chrono::time_point<std::chrono::high_resolution_clock> start) {
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> diff = end-start;
  return diff.count();
}

long sum(long n, long* xs) {
  long r = 0;
  for (int i = 0; i < n; i++) {
    r += xs[i];
  }
  return r;
}

#ifdef HAVE_HWLOC
hwloc_topology_t topology;
hwloc_cpuset_t* cpusets;
#endif
  
void cpu_binding_init(int nb_threads) {
#ifdef HAVE_HWLOC
  cpusets = new hwloc_cpuset_t[nb_threads];
  hwloc_topology_init(&topology);
  hwloc_topology_load(topology);
  int depth = hwloc_get_type_or_below_depth(topology, HWLOC_OBJ_CORE);
  if (nb_threads > hwloc_get_nbobjs_by_depth(topology, depth)) {
    pasl::util::atomic::die("too few cores to satisfy request");
  }
  for (int i = 0; i < nb_threads; i++) {
    hwloc_obj_t obj = hwloc_get_obj_by_depth(topology, depth, i);
    if (obj == nullptr) {
      pasl::util::atomic::die("failed to satisfy cpu binding request");
    }
    cpusets[i] = hwloc_bitmap_dup(obj->cpuset);
    hwloc_bitmap_singlify(cpusets[i]);
  }
#endif
}

void cpu_bind(int i) {
#ifdef HAVE_HWLOC
  bool should_cpu_bind = pasl::util::cmdline::parse_or_default_bool("should_cpu_bind", false);
  if (! should_cpu_bind) {
    return;
  }
  if (hwloc_set_cpubind(topology, cpusets[i], 0)) {
    char *str;
    int error = errno;
    //            hwloc_bitmap_asprintf(&str, obj->cpuset);
    pasl::util::atomic::die("Couldn't bind to cpuset %s: %s\n", str, strerror(error));            
    //free(str);
  }
  hwloc_bitmap_free(cpusets[i]);
#endif
}

void cpu_binding_destroy() {
#ifdef HAVE_HWLOC
  delete cpusets;
#endif
}

template <class Benchmark>
void launch_microbenchmark(const Benchmark& benchmark, int nb_threads, int nb_milliseconds) {
  cpu_binding_init(nb_threads);
  bool should_stop = false;
  std::vector<std::thread*> threads;
  long counters1[nb_threads];
  long counters2[nb_threads];
  for (int i = 0; i < nb_threads; i++) {
    counters1[i] = 0;
    counters2[i] = 0;
    threads.push_back(new std::thread([&, i] {
      cpu_bind(i);
      benchmark(i, should_stop, counters1[i], counters2[i]);
    }));
  }
  auto start = std::chrono::high_resolution_clock::now();
  std::this_thread::sleep_for(std::chrono::milliseconds(nb_milliseconds));
  should_stop = true;
  printf ("exectime_phase1 %.3lf\n", since(start));
  auto start_phase2 = std::chrono::high_resolution_clock::now();
  for (std::thread* thread : threads) {
    thread->join();
  }
  printf ("exectime %.3lf\n", since(start));
  printf ("exectime_phase2 %.3lf\n", since(start_phase2));
  long nb_operations_phase1 = sum(nb_threads, counters1);
  std::cout << "nb_operations_phase1  " << nb_operations_phase1 << std::endl;
  long nb_operations_phase2 = sum(nb_threads, counters2);
  long nb_operations = nb_operations_phase1 + nb_operations_phase2;
  std::cout << "nb_operations  " << nb_operations << std::endl;
  std::cout << "nb_operations_phase2  " << nb_operations_phase2 << std::endl;
  for (std::thread* t : threads) {
    delete t;
  }
  cpu_binding_destroy();
}

void launch_outset_add_duration() {
  int seed = pasl::util::cmdline::parse_int("seed");
  int nb_threads = pasl::util::cmdline::parse_int("proc");
  int nb_milliseconds = pasl::util::cmdline::parse_int("nb_milliseconds");
  simple_outset_wrapper* simple_outset = nullptr;
  growable_outset_wrapper* growable_outset = nullptr;
  perprocessor_outset_wrapper* perprocessor_outset = nullptr;
  single_buffer_outset_wrapper* single_buffer_outset = nullptr;
  pasl::util::cmdline::argmap_dispatch c;
  c.add("simple", [&] {
    simple_outset = new simple_outset_wrapper;
  });
  c.add("growabletree", [&] {
    growable_outset = new growable_outset_wrapper;
  });
  c.add("perprocessor", [&] {
    perprocessor_outset = new perprocessor_outset_wrapper;
  });
  c.add("single_buffer", [&] {
    single_buffer_outset = new single_buffer_outset_wrapper;
  });
  c.find_by_arg("edge_algo")();
  auto benchmark_thread = [&] (int my_id, bool& should_stop, long& counter1, long& counter2) {
    if (simple_outset != nullptr) {
      benchmark_outset_thread(my_id, *simple_outset, should_stop, counter1, seed);
    } else if (growable_outset != nullptr) {
      benchmark_outset_thread(my_id, *growable_outset, should_stop, counter1, seed);
    } else if (perprocessor_outset != nullptr) {
      benchmark_outset_thread(my_id, *perprocessor_outset, should_stop, counter1, seed);
    } else if (single_buffer_outset != nullptr) {
      benchmark_outset_thread(my_id, *single_buffer_outset, should_stop, counter1, seed);
    }
  };
  launch_microbenchmark(benchmark_thread, nb_threads, nb_milliseconds);
  if (simple_outset != nullptr) {
    delete simple_outset;
  } else if (growable_outset != nullptr) {
    delete growable_outset;
  } else if (perprocessor_outset != nullptr) {
    delete perprocessor_outset;
  } else if (single_buffer_outset != nullptr) {
    delete single_buffer_outset;
  }
}

void launch_incounter_mixed_duration() {
  int seed = pasl::util::cmdline::parse_int("seed");
  int nb_threads = pasl::util::cmdline::parse_int("proc");
  int nb_milliseconds = pasl::util::cmdline::parse_int("nb_milliseconds");
  simple_incounter_wrapper* simple_incounter = nullptr;
  snzi_incounter_wrapper* snzi_incounter = nullptr;
  pasl::util::cmdline::argmap_dispatch c;
  c.add("simple", [&] {
    simple_incounter = new simple_incounter_wrapper;
  });
  c.add("statreeopt", [&] {
    snzi_incounter = new snzi_incounter_wrapper;
  });
  c.find_by_arg("edge_algo")();
  auto benchmark_thread = [&] (int my_id, bool& should_stop, long& counter1, long& counter2) {
    if (simple_incounter != nullptr) {
      benchmark_incounter_thread(my_id, *simple_incounter, should_stop, counter1, counter2, seed);
    } else if (snzi_incounter != nullptr) {
      benchmark_incounter_thread(my_id, *snzi_incounter, should_stop, counter1, counter2, seed);
    } else {
      assert(false);
    }
  };
  launch_microbenchmark(benchmark_thread, nb_threads, nb_milliseconds);
  if (simple_incounter != nullptr) {
    assert(simple_incounter->is_activated());
    delete simple_incounter;
  } else if (snzi_incounter != nullptr) {
    assert(snzi_incounter->is_activated());
    delete snzi_incounter;
  }
}
  
void launch_snzi_alternated_duration() {
  int seed = pasl::util::cmdline::parse_int("seed");
  int nb_threads = pasl::util::cmdline::parse_int("proc");
  int nb_milliseconds = pasl::util::cmdline::parse_int("nb_milliseconds");
  fixed_size_snzi_wrapper* fixed_snzi = nullptr;
  growable_size_snzi_wrapper* growable_snzi = nullptr;
  single_cell_snzi_wrapper* single_cell_snzi = nullptr;
  pasl::util::cmdline::argmap_dispatch c;
  c.add("fixed", [&] {
    fixed_snzi = new fixed_size_snzi_wrapper;
  });
  c.add("growable", [&] {
    growable_snzi = new growable_size_snzi_wrapper;
  });
  c.add("single_cell", [&] {
    single_cell_snzi = new single_cell_snzi_wrapper;
  });
  c.find_by_arg("snzi")();
  auto benchmark_thread = [&] (int my_id, bool& should_stop, long& counter1, long& counter2) {
    if (fixed_snzi != nullptr) {
      benchmark_snzi_thread(my_id, *fixed_snzi, should_stop, counter1, counter2, seed);
    } else if (growable_snzi != nullptr) {
      benchmark_snzi_thread(my_id, *growable_snzi, should_stop, counter1, counter2, seed);
    } else if (single_cell_snzi != nullptr) {
      benchmark_snzi_thread(my_id, *single_cell_snzi, should_stop, counter1, counter2, seed);
    } else {
      assert(false);
    }
  };
  launch_microbenchmark(benchmark_thread, nb_threads, nb_milliseconds);
  if (fixed_snzi != nullptr) {
    delete fixed_snzi;
  } else if (growable_snzi != nullptr) {
    delete growable_snzi;
  } else if (single_cell_snzi != nullptr) {
    delete single_cell_snzi;
  }
}

double workload = 0.0;

void do_dummy_work() {
  if (workload == 0.0) {
    return;
  }
  pasl::util::microtime::microsleep(workload);
}
  
bool should_incounter_async_duration_terminate = false;
  
pasl::data::perworker::counter::carray<int> incounter_async_duration_counter;

template <class node>
class incounter_async_duration_loop : public node {
public:
  
  enum {
    entry,
    exit
  };
  
  node* join;
  
  incounter_async_duration_loop(node* join)
  : join(join) { }
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        if (! should_incounter_async_duration_terminate) {
          incounter_async_duration_counter++;
          do_dummy_work();
          node::async(new incounter_async_duration_loop(join), join,
                      exit);
        }
        break;
      }
      case exit: {
        node::jump_to(entry);
        break;
      }
    }
  }
  
};
  
template <class node>
class incounter_async_duration : public node {
public:
  
  enum {
    entry,
    exit
  };
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        incounter_async_duration_counter.init(0);
        node::finish(new incounter_async_duration_loop<node>(this),
                     exit);
        break;
      }
      case exit: {
        std::cout << "nb_operations  " << incounter_async_duration_counter.sum() << std::endl;
        break;
      }
    }
  }
  
};
  
pasl::data::perworker::counter::carray<int> mixed_duration_counter;
  
template <class node>
class mixed_duration_force : public node {
public:
  
  enum {
    entry,
    exit
  };

  mixed_duration_force(outset_of<node>* producer)
  : producer(producer) { }
  
  outset_of<node>* producer;
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        mixed_duration_counter++;
        do_dummy_work();
        node::force(producer,
                    exit);
        break;
      }
      case exit: {
        break;
      }
    }
  }
  
};
  
template <class node>
class mixed_duration_loop : public node {
public:
  
  enum {
    entry,
    recurse,
    loop,
    exit
  };
  
  mixed_duration_loop(node* join,
                      outset_of<node>* producer,
                      std::atomic<node*>* buffer)
  : join(join), producer(producer), buffer(buffer) { }
  
  node* join;
  outset_of<node>* producer;
  std::atomic<node*>* buffer;
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        node::async(new mixed_duration_force<node>(producer), join,
                    recurse);
        break;
      }
      case recurse: {
        if (buffer->load() == nullptr && should_communicate()) {
          node::async(new mixed_duration_loop(join, producer, buffer), join,
                      loop);
        } else {
          node::jump_to(loop);
        }
        break;
      }
      case loop: {
        node* orig = buffer->load();
        node* next = tagged_tag_with((node*)nullptr, 1);
        if (orig == nullptr) {
          node::jump_to(entry);
        } else if (orig == next) {
          break;
        } else {
          if (compare_exchange(*buffer, orig, next)) {
            pasl::sched::instrategy::schedule(orig);
          }
        }
        break;
      }
      case exit: {
        break;
      }
    }
  }
  
};
  
template <class node>
class mixed_duration_future : public node {
public:
  
  enum {
    entry,
    exit
  };
  
  mixed_duration_future(int nb_milliseconds, std::atomic<node*>* buffer)
  : nb_milliseconds(nb_milliseconds), buffer(buffer) { }
  
  std::atomic<node*>* buffer;
  int nb_milliseconds;
  outset_of<node>* cont;
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        std::thread timer([&] {
          std::this_thread::sleep_for(std::chrono::milliseconds(nb_milliseconds));
          node::out = cont;
          buffer->store(this);
        });
        timer.detach();
        node::detach(exit);
        cont = (outset_of<node>*)node::out;
        node::out = nullptr;
        break;
      }
      case exit: {
        break;
      }
    }
  }
  
};
  
template <class node>
class mixed_duration : public node {
public:
  
  enum {
    entry,
    gen,
    exit
  };
  
  mixed_duration(int nb_milliseconds)
  : nb_milliseconds(nb_milliseconds) { }
  
  std::atomic<node*> buffer;
  int nb_milliseconds;
  outset_of<node>* producer;
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        mixed_duration_counter.init(0);
        buffer.store(nullptr);
        producer = node::future(new mixed_duration_future<node>(nb_milliseconds, &buffer),
                                gen);
        break;
      }
      case gen: {
        node::finish(new mixed_duration_loop<node>(this, producer, &buffer),
                     exit);
        break;
      }
      case exit: {
        node::deallocate_future(producer);
        std::cout << "nb_operations  " << mixed_duration_counter.sum() << std::endl;
        break;
      }
    }
  }
  
};
  
long mixed_nb_cutoff = 1;
  
template <class node>
class mixed_nb_future : public node {
public:
  
  enum {
    entry,
    exit
  };
  
  outset_of<node>* future;
  
  mixed_nb_future(outset_of<node>* future)
  : future(future) { }
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        node::force(future,
                    exit);
        break;
      }
      case exit: {
        break;
      }
    }
  }
  
};
  
template <class node>
class mixed_nb_rec : public node {
public:
  
  enum {
    entry,
    loop,
    exit
  };
  
  long nb;
  
  outset_of<node>* future;
  
  mixed_nb_rec(long nb)
  : nb(nb) { }
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        if (nb <= mixed_nb_cutoff) {
          //pasl::util::microtime::wait_for(1<<11);
        } else {
          future = node::future(new mixed_nb_rec(nb / 2),
                                loop);
        }
        break;
      }
      case loop: {
        auto b = [&] (long) {
          return new mixed_duration_force<node>(future);
        };
        node::parallel_for_nested(0, nb, b,
                                  exit);
        break;
      }
      case exit: {
        node::deallocate_future(future);
        break;
      }
    }
  }
  
};

template <class node>
class mixed_nb : public node {
public:
  
  enum {
    entry,
    exit
  };
  
  long nb;
  
  mixed_nb(long nb)
  : nb(nb) { }
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        node::call(new mixed_nb_rec<node>(nb),
                   exit);
        break;
      }
      case exit: {
        std::cout << "nb_operations " << nb << std::endl;
        break;
      }
    }
  }
  
};

template <class node>
class incounter_async_nb_rec : public node {
public:

  enum {
    entry,
    exit
  };

  incounter_async_nb_rec(int lo, int hi, node* join)
    : lo(lo), hi(hi), join(join) { }

  int lo;
  int hi;
  node* join;

  void body() {
    switch (node::current_block_id) {
      case entry: {
        if ((hi - lo) <= 1) {
          do_dummy_work();
        } else {
          int mid = (lo + hi) / 2;
          node::async(new incounter_async_nb_rec(mid, hi, join), join,
                      exit);
          hi = mid;
        }
        break;
      }
      case exit: {
        node::jump_to(entry);
        break;
      }
    }
  }

};

template <class node>
class incounter_async_nb : public node {
public:

  enum {
    entry,
    exit
  };

  incounter_async_nb(int n)
    : n(n) { }

  int n;

  void body() {
    switch (node::current_block_id) {
      case entry: {
        node::finish(new incounter_async_nb_rec<node>(0, n, this),
                     exit);
        break;
      }
      case exit: {
        printf("nb_operations %d\n", n);
        break;
      }
    }
  }

};

template <class node>
class incounter_forkjoin_nb_rec : public node {
public:
  
  int lo;
  int hi;
  
  enum {
    entry,
    exit
  };
  
  incounter_forkjoin_nb_rec(int lo, int hi)
  : lo(lo), hi(hi) { }
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        if ((hi - lo) <= 1) {
          do_dummy_work();
        } else {
          int mid = (lo + hi) / 2;
          node::fork2(new incounter_forkjoin_nb_rec(lo, mid),
                      new incounter_forkjoin_nb_rec(mid, hi),
                      exit);
        }
        break;
      }
      case exit: {
        break;
      }
    }
  }
  
};
  
template <class node>
class incounter_forkjoin_nb : public node {
public:
  
  enum {
    entry,
    exit
  };
  
  incounter_forkjoin_nb(int n)
  : n(n) { }
  
  int n;
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        node::call(new incounter_forkjoin_nb_rec<node>(0, n),
                   exit);
        break;
      }
      case exit: {
        printf("nb_operations %d\n", n);
        break;
      }
    }
  }
  
};

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

void seidel_block(int N, double* a, int block_size) {
  for (int i = 1; i <= block_size; i++) {
    for (int j = 1; j <= block_size; j++) {
      a[i*N+j] = 0.2 * (a[i*N+j] + a[(i-1)*N+j] + a[(i+1)*N+j] + a[i*N+j-1] + a[i*N+j+1]);
    }
  }
}

void seidel_sequential(int numiters, int N, int block_size, double* data) {
  for (int iter = 0; iter < numiters; iter++) {
    for (int i = 0; i < N-2; i += block_size) {
      for (int j = 0; j < N-2; j += block_size) {
        seidel_block(N, &data[N * i + j], block_size);
      }
    }
  }
}
  
using private_clock_type = struct {
  long time;
  void* _padding[7];
};
  
template <class node>
class seidel_async_parallel_rec : public node {
public:
  
  using coordinate_type = std::pair<int,int>;
  using frontier_type = pasl::data::chunkedseq::bootstrapped::stack<coordinate_type>;
  
  frontier_type frontier;
  
  enum {
    entry,
    loop_header,
    loop_body,
    exit
  };
  
  int N; int block_size; double* data;
  matrix_type<std::atomic<int>>* incounters;
  matrix_type<private_clock_type>* clocks;
  
  outset_of<node>* future;
  bool initial_thread = true;

  seidel_async_parallel_rec(matrix_type<std::atomic<int>>* incounters,
                            matrix_type<private_clock_type>* clocks,
                            outset_of<node>* future,
                            int N, int block_size, double* data)
  : incounters(incounters), clocks(clocks), future(future), N(N),
  block_size(block_size), data(data) { }
  

  void advance_time(int i, int j) {
    long after = --clocks->subscript(i, j).time;
    int n = incounters->n;
    if ((after == 0) && ((i + 1) == n) && ((j + 1) == n)) {
      future->finished();
    }
  }
  
  void process_block(int i, int j) {
    int ii = i * block_size;
    int jj = j * block_size;
    seidel_block(N, &data[N * ii + jj], block_size);
  }
  
  void reset_block_count(int i, int j) {
    int nb_neighbors = 4;
    int n = incounters->n;
    if ((i == 0) || ((i + 1) == n)) {
      nb_neighbors--;
    }
    if ((j == 0) || ((j + 1) == n)) {
      nb_neighbors--;
    }
    incounters->subscript(i, j).store(nb_neighbors);
  }
  
  void decr_block(int i, int j) {
    if (--incounters->subscript(i, j) == 0) {
      frontier.push_back(std::make_pair(i, j));
    }
  }
  
  void decr_neighbors(int i, int j) {
    int n = incounters->n;
    if (((i + 1) < n) && ((j + 1) < n)) {
      decr_block(i + 1, j);
      decr_block(i, j + 1);
    } else if (((i + 1) < n) && ((j + 1) == n)) {
      decr_block(i + 1, j);
    } else if (((i + 1) == n) && ((j + 1) < n)) {
      decr_block(i, j + 1);
    } else if (((i + 1) == n) && ((j + 1) == n)) {
      // nothing to do
    } else {
      assert(false);
    }
    if (clocks->subscript(i, j).time == 0) {
      return;
    }
    if ((i > 0) && (j > 0)) {
      decr_block(i - 1, j);
      decr_block(i, j - 1);
    } else if ((i > 0) && (j == 0)) {
      decr_block(i - 1, j);
    } else if ((i == 0) && (j > 0)) {
      decr_block(i, j - 1);
    } else if ((i == 0) && (j == 0)) {
      // nothing to do
    } else {
      assert(false);
    }
  }
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        if (incounters->n < 1) {
          break;
        }
        if (initial_thread) {
          decr_block(0, 0);
        }
        node::jump_to(loop_header);
        break;
      }
      case loop_header: {
        if (frontier.empty()) {
          node::jump_to(exit);
        } else {
          node::jump_to(loop_body);
        }
        break;
      }
      case loop_body: {
        assert(! frontier.empty());
        coordinate_type coordinate = frontier.pop_front();
        int i = coordinate.first;
        int j = coordinate.second;
        advance_time(i, j);
        process_block(i, j);
        reset_block_count(i, j);
        decr_neighbors(i, j);
        node::jump_to(loop_header);
        break;
      }
      case exit: {
        break;
      }
    }
  }
  
  size_t size() {
    return frontier.size();
  }
  
  pasl::sched::thread_p split(size_t) {
    auto n = new seidel_async_parallel_rec(incounters, clocks, future, N, block_size, data);
    n->initial_thread = false;
    assert(size() >= 2);
    size_t half = size() / 2;
    frontier.split(half, n->frontier);
    node::split_with(n);
    return n;
  }
  
};
  
template <class node>
class seidel_async : public node {
public:
  
  enum {
    entry,
    init_first_row_incounters,
    init_first_col_incounters,
    init_incounters,
    launch,
    exit
  };
  
  int numiters; int N; int block_size; double* data;
  
  int n;
  matrix_type<std::atomic<int>>* incounters;
  matrix_type<private_clock_type>* clocks;
  int nb_blocks;
  outset_of<node>* future;
  
  seidel_async(int numiters, int N, int block_size, double* data)
  : numiters(numiters), N(N), block_size(block_size), data(data) { }
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        n = (N - 2) / block_size;
        incounters = new matrix_type<std::atomic<int>>(n);
        clocks = new matrix_type<private_clock_type>(n);
        nb_blocks = n * n;
        node::parallel_for(0, nb_blocks, [&] (long i) {
          clocks->items[i].time = numiters;
        }, init_incounters);
        break;
      }
      case init_incounters: {
        node::parallel_for(0, nb_blocks, [&] (long i) {
          incounters->items[i].store(2);
        }, init_first_row_incounters);
        break;
      }
      case init_first_row_incounters: {
        node::parallel_for(0, n, [&] (int i) {
          incounters->subscript(i, 0).store(1);
        }, init_first_col_incounters);
        break;
      }
      case init_first_col_incounters: {
        node::parallel_for(0, n, [&] (int i) {
          incounters->subscript(0, i).store(1);
        }, launch);
        break;
      }
      case launch: {
        future = node::allocate_future();
        node::spawn(new seidel_async_parallel_rec<node>(incounters, clocks, future, N, block_size, data));
        node::force(future,
                    exit);
        break;
      }
      case exit: {
        node::deallocate_future(future);
        delete incounters;
        delete clocks;
        break;
      }
    }
  }
  
};

void seidel_initialize(matrix_type<double>& mtx) {
  int N = mtx.n;
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      mtx.subscript(i, j) = (double) ((i == 25 && j == 25) || (i == N-25 && j == N-25)) ? 500 : 0;
    }
  }
}

double epsilon = 0.001;

int count_nb_diffs(const matrix_type<double>& lhs,
                   const matrix_type<double>& rhs) {
  if (lhs.n != rhs.n) {
    return std::max(lhs.n, rhs.n);
  }
  int nb_diffs = 0;
  int n = lhs.n;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      double diff = std::abs(lhs.subscript(i, j) - rhs.subscript(i, j));
      if (diff > epsilon) {
        nb_diffs++;
      }
    }
  }
  return nb_diffs;
}
  
template <class node>
class pmemset : public node {
public:
  
  char* ptr;
  int value;
  size_t num;
  
  static constexpr int cutoff = 1 << 8;
  
  pmemset(char* ptr, int value, size_t num)
  : ptr(ptr), value(value), num(num) { }
  
  enum {
    entry,
    exit
  };
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        node::parallel_for_rng(0, num, cutoff, [&] (long lo, long hi) {
          memset(ptr + lo, value, hi - lo);
        }, exit);
        break;
      }
      case exit: {
#ifndef NDEBUG
        for (size_t i = 0; i < num; i++) {
          assert(ptr[i] == (unsigned char)value);
        }
#endif
        break;
      }
    }
  }
  
};
  
template <class node, class ForwardIt, class T>
class pfill : public node {
public:
  
  ForwardIt first;
  ForwardIt last;
  T value;
  
  static constexpr int cutoff = 1 << 8;
  
  pfill(ForwardIt first, ForwardIt last, const T& value)
  : first(first), last(last), value(value) { }
  
  enum {
    entry,
    exit
  };
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        std::size_t num = last - first;
        node::parallel_for_rng(0, num, cutoff, [&] (long lo, long hi) {
          std::fill(first + lo, first + hi, value);
        }, exit);
        break;
      }
      case exit: {
#ifndef NDEBUG
        std::size_t num = last - first;
        for (size_t i = 0; i < num; i++) {
          assert(first[i] == value);
        }
#endif
        break;
      }
    }
  }
  
};
  
template <class Vertex_id_bag>
class symmetric_vertex {
public:
  
  typedef Vertex_id_bag vtxid_bag_type;
  typedef typename vtxid_bag_type::value_type vtxid_type;
  
  symmetric_vertex() { }
  
  symmetric_vertex(vtxid_bag_type neighbors)
  : neighbors(neighbors) { }
  
  vtxid_bag_type neighbors;
  
  vtxid_type get_in_neighbor(vtxid_type j) const {
    return neighbors[j];
  }
  
  vtxid_type get_out_neighbor(vtxid_type j) const {
    return neighbors[j];
  }
  
  vtxid_type* get_in_neighbors() const {
    return neighbors.data();
  }
  
  vtxid_type* get_out_neighbors() const {
    return neighbors.data();
  }
  
  void set_in_neighbor(vtxid_type j, vtxid_type nbr) {
    neighbors[j] = nbr;
  }
  
  void set_out_neighbor(vtxid_type j, vtxid_type nbr) {
    neighbors[j] = nbr;
  }
  
  vtxid_type get_in_degree() const {
    return vtxid_type(neighbors.size());
  }
  
  vtxid_type get_out_degree() const {
    return vtxid_type(neighbors.size());
  }
  
  void set_in_degree(vtxid_type j) {
    neighbors.alloc(j);
  }
  
  // todo: use neighbors.resize()
  void set_out_degree(vtxid_type j) {
    neighbors.alloc(j);
  }
  
  void swap_in_neighbors(vtxid_bag_type& other) {
    neighbors.swap(other);
  }
  
  void swap_out_neighbors(vtxid_bag_type& other) {
    neighbors.swap(other);
  }
  
  void check(vtxid_type nb_vertices) const {
#ifndef NDEBUG
    for (vtxid_type i = 0; i < neighbors.size(); i++)
      check_vertex(neighbors[i], nb_vertices);
#endif
  }
  
};

template <class Item>
class pointer_seq {
public:
  
  typedef size_t size_type;
  typedef Item value_type;
  typedef pointer_seq<Item> self_type;
  
  value_type* array;
  size_type sz;
  
  pointer_seq()
  : array(NULL), sz(0) { }
  
  pointer_seq(value_type* array, size_type sz)
  : array(array), sz(sz) { }
  
  ~pointer_seq() {
    clear();
  }
  
  void clear() {
    sz = 0;
    array = NULL;
  }
  
  value_type& operator[](size_type ix) const {
    assert(ix >= 0);
    assert(ix < sz);
    return array[ix];
  }
  
  size_type size() const {
    return sz;
  }
  
  void swap(self_type& other) {
    std::swap(sz, other.sz);
    std::swap(array, other.array);
  }
  
  void alloc(size_type n) {
    assert(false);
  }
  
  void set_data(value_type* data) {
    this->data = data;
  }
  
  value_type* data() const {
    return array;
  }
  
  template <class Body>
  void for_each(const Body& b) const {
    for (size_type i = 0; i < sz; i++)
      b(array[i]);
  }
  
};

using edgeid_type = size_t;

template <class Vertex_id, bool Is_alias = false>
class flat_adjlist_seq {
public:
  
  typedef flat_adjlist_seq<Vertex_id> self_type;
  typedef Vertex_id vtxid_type;
  typedef size_t size_type;
  typedef pointer_seq<vtxid_type> vertex_seq_type;
  typedef symmetric_vertex<vertex_seq_type> value_type;
  typedef flat_adjlist_seq<vtxid_type, true> alias_type;
  
  char* underlying_array;
  vtxid_type* offsets;
  vtxid_type nb_offsets;
  vtxid_type* edges;
  
  flat_adjlist_seq()
  : underlying_array(NULL), offsets(NULL),
  nb_offsets(0), edges(NULL) { }
  
  flat_adjlist_seq(const flat_adjlist_seq& other) {
    if (Is_alias) {
      underlying_array = other.underlying_array;
      offsets = other.offsets;
      nb_offsets = other.nb_offsets;
      edges = other.edges;
    } else {
      assert(false);
    }
  }
  
  ~flat_adjlist_seq() {
    if (! Is_alias)
      clear();
  }
  
  void get_alias(alias_type& alias) const {
    alias.underlying_array = NULL;
    alias.offsets = offsets;
    alias.nb_offsets = nb_offsets;
    alias.edges = edges;
  }
  
  alias_type get_alias() const {
    alias_type alias;
    alias.underlying_array = NULL;
    alias.offsets = offsets;
    alias.nb_offsets = nb_offsets;
    alias.edges = edges;
    return alias;
  }
  
  void clear() {
    if (underlying_array != NULL)
      free(underlying_array);
    offsets = NULL;
    edges = NULL;
  }
  
  vtxid_type degree(vtxid_type v) const {
    assert(v >= 0);
    assert(v < size());
    return offsets[v + 1] - offsets[v];
  }
  
  value_type operator[](vtxid_type ix) const {
    assert(ix >= 0);
    assert(ix < size());
    return value_type(vertex_seq_type(&edges[offsets[ix]], degree(ix)));
  }
  
  vtxid_type size() const {
    return nb_offsets - 1;
  }
  
  void swap(self_type& other) {
    std::swap(underlying_array, other.underlying_array);
    std::swap(offsets, other.offsets);
    std::swap(nb_offsets, other.nb_offsets);
    std::swap(edges, other.edges);
  }
  
  void alloc(size_type) {
    assert(false);
  }
  
  void init(char* bytes, vtxid_type nb_vertices, edgeid_type nb_edges) {
    nb_offsets = nb_vertices + 1;
    underlying_array = bytes;
    offsets = (vtxid_type*)bytes;
    edges = &offsets[nb_offsets];
  }
  
  value_type* data() {
    assert(false);
    return NULL;
  }
  
};

template <class Adjlist_seq>
class adjlist {
public:
  
  typedef Adjlist_seq adjlist_seq_type;
  typedef typename adjlist_seq_type::value_type vertex_type;
  typedef typename vertex_type::vtxid_bag_type::value_type vtxid_type;
  typedef typename adjlist_seq_type::alias_type adjlist_seq_alias_type;
  typedef adjlist<adjlist_seq_alias_type> alias_type;
  
  edgeid_type nb_edges;
  adjlist_seq_type adjlists;
  
  adjlist()
  : nb_edges(0) { }
  
  adjlist(edgeid_type nb_edges)
  : nb_edges(nb_edges) { }
  
  vtxid_type get_nb_vertices() const {
    return vtxid_type(adjlists.size());
  }
  
  void check() const {
#ifndef NDEBUG
    for (vtxid_type i = 0; i < adjlists.size(); i++)
      adjlists[i].check(get_nb_vertices());
    size_t m = 0;
    for (vtxid_type i = 0; i < adjlists.size(); i++)
      m += adjlists[i].get_in_degree();
    assert(m == nb_edges);
    m = 0;
    for (vtxid_type i = 0; i < adjlists.size(); i++)
      m += adjlists[i].get_out_degree();
    assert(m == nb_edges);
#endif
  }
  
};

static constexpr uint64_t GRAPH_TYPE_ADJLIST = 0xdeadbeef;

template <class Vertex_id>
void read_adjlist_from_file(std::string fname, adjlist<flat_adjlist_seq<Vertex_id>>& graph) {
  using vtxid_type = Vertex_id;
  std::ifstream in(fname, std::ifstream::binary);
  uint64_t graph_type;
  int nbbits;
  vtxid_type nb_vertices;
  edgeid_type nb_edges;
  bool is_symmetric;
  uint64_t header[5];
  in.read((char*)header, sizeof(header));
  graph_type = header[0];
  nbbits = int(header[1]);
  nb_vertices = vtxid_type(header[2]);
  nb_edges = edgeid_type(header[3]);
  is_symmetric = bool(header[4]);
  if (graph_type != GRAPH_TYPE_ADJLIST)
    assert(false);
  if (sizeof(vtxid_type) * 8 < nbbits)
    assert(false);
  edgeid_type contents_szb;
  char* bytes;
  in.seekg (0, in.end);
  contents_szb = edgeid_type(in.tellg()) - sizeof(header);
  in.seekg (sizeof(header), in.beg);
  bytes = (char*)malloc(contents_szb);
  if (bytes == NULL)
    assert(false);
  in.read (bytes, contents_szb);
  in.close();
  vtxid_type nb_offsets = nb_vertices + 1;
  if (contents_szb != sizeof(vtxid_type) * (nb_offsets + nb_edges))
    assert(false);
  graph.adjlists.init(bytes, nb_vertices, nb_edges);
  graph.nb_edges = nb_edges;
}
  
namespace frontiersegbase {
  
  template <
  class Graph,
  template <
  class Vertex,
  class Cache_policy
  >
  class Vertex_container
  >
  class frontiersegbase {
  public:
    
    /*---------------------------------------------------------------------*/
    
    using self_type = frontiersegbase<Graph, Vertex_container>;
    using size_type = size_t;
    using graph_type = Graph;
    using vtxid_type = typename graph_type::vtxid_type;
    using const_vtxid_pointer = const vtxid_type*;
    
    class edgelist_type {
    public:
      
      const_vtxid_pointer lo;
      const_vtxid_pointer hi;
      
      edgelist_type()
      : lo(nullptr), hi(nullptr) { }
      
      edgelist_type(size_type nb, const_vtxid_pointer edges)
      : lo(edges), hi(edges + nb) { }
      
      size_type size() const {
        return size_type(hi - lo);
      }
      
      void clear() {
        hi = lo;
      }
      
      static edgelist_type take(edgelist_type edges, size_type nb) {
        assert(nb <= edges.size());
        assert(nb >= 0);
        edgelist_type edges2 = edges;
        edges2.hi = edges2.lo + nb;
        assert(edges2.size() == nb);
        return edges2;
      }
      
      static edgelist_type drop(edgelist_type edges, size_type nb) {
        assert(nb <= edges.size());
        assert(nb >= 0);
        edgelist_type edges2 = edges;
        edges2.lo = edges2.lo + nb;
        assert(edges2.size() + nb == edges.size());
        return edges2;
      }
      
      void swap(edgelist_type& other) {
        std::swap(lo, other.lo);
        std::swap(hi, other.hi);
      }
      
      template <class Body>
      void for_each(const Body& func) const {
        for (auto e = lo; e < hi; e++)
          func(*e);
      }
    };
    
  private:
    
    /*---------------------------------------------------------------------*/
    
    static size_type out_degree_of_vertex(graph_type g, vtxid_type v) {
      return size_type(g.adjlists[v].get_out_degree());
    }
    
    static vtxid_type* neighbors_of_vertex(graph_type g, vtxid_type v) {
      return g.adjlists[v].get_out_neighbors();
    }
    
    edgelist_type create_edgelist(vtxid_type v) const {
      graph_type g = get_graph();
      size_type degree = out_degree_of_vertex(g, v);
      vtxid_type* neighbors = neighbors_of_vertex(g, v);
      return edgelist_type(vtxid_type(degree), neighbors);
    }
    
    /*---------------------------------------------------------------------*/
    
    class graph_env {
    public:
      
      graph_type g;
      
      graph_env() { }
      graph_env(graph_type g) : g(g) { }
      
      size_type operator()(vtxid_type v) const {
        return out_degree_of_vertex(g, v);
      }
      
    };
    
    using cache_type = pasl::data::cachedmeasure::weight<vtxid_type, vtxid_type, size_type, graph_env>;
    using seq_type = Vertex_container<vtxid_type, cache_type>;
    //  using seq_type = data::chunkedseq::bootstrapped::stack<vtxid_type, chunk_capacity, cache_type>;
    
    /*---------------------------------------------------------------------*/
    
    // FORMIKE: suggesting rename to fr, mid, bk
    edgelist_type f;
    seq_type m;
    edgelist_type b;
    
    /*---------------------------------------------------------------------*/
    
    void check() const {
#ifdef FULLDEBUG
      size_type nf = f.size();
      size_type nb = b.size();
      size_type nm = 0;
      graph_type g = get_graph();
      m.for_each([&] (vtxid_type v) {
        nm += out_degree_of_vertex(g, v);
      });
      size_type n = nf + nb + nm;
      size_type e = nb_outedges();
      size_type em = nb_outedges_of_middle();
      size_type szm = m.size();
      assert(n == e);
      assert((szm > 0) ? em > 0 : true);
      assert((em > 0) ? (szm > 0) : true);
#endif
    }
    
    size_type nb_outedges_of_middle() const {
      return m.get_cached();
    }
    
  public:
    
    /*---------------------------------------------------------------------*/
    
    frontiersegbase() { }
    
    frontiersegbase(graph_type g) {
      set_graph(g);
    }
    
    /* We use the following invariant so that client code can use empty check
     * of the container interchangeably with a check on the number of outedges:
     *
     *     empty() iff nb_outedges() == 0
     */
    
    bool empty() const {
      return f.size() == 0
      && m.empty()
      && b.size() == 0;
    }
    
    size_type nb_outedges() const {
      size_type nb = 0;
      nb += f.size();
      nb += nb_outedges_of_middle();
      nb += b.size();
      return nb;
    }
    
    void push_vertex_back(vtxid_type v) {
      check();
      size_type d = out_degree_of_vertex(get_graph(), v);
      // FORMIKE: this optimization might actually be slowing down things
      // MIKE: this is not an optimization; it's necessary to maintain invariant 1 above
      if (d > 0)
        m.push_back(v);
      check();
    }
    
    /*
     void push_edgelist_back(edgelist_type edges) {
     assert(b.size() == 0);
     check();
     b = edges;
     check();
     } */
    
    edgelist_type pop_edgelist_back() {
      size_type nb_outedges1 = nb_outedges();
      assert(nb_outedges1 > 0);
      edgelist_type edges;
      check();
      if (b.size() > 0) {
        edges.swap(b);
      } else if (! m.empty()) {
        // FORMIKE: if we allow storing nodes with zero out edges,
        // then we may consider having a while loop here, to pop
        // from middle until we get a node with more than zero edges
        edges = create_edgelist(m.pop_back());
      } else {
        assert(f.size() > 0);
        edges.swap(f);
      }
      check();
      size_type nb_popped_edges = edges.size();
      assert(nb_popped_edges > 0);
      size_type nb_outedges2 = nb_outedges();
      assert(nb_outedges2 + nb_popped_edges == nb_outedges1);
      assert(b.size() == 0);
      return edges;
    }
    
    // pops the back edgelist containing at most `nb` edges
    /*
     edgelist_type pop_edgelist_back_at_most(size_type nb) {
     size_type nb_outedges1 = nb_outedges();
     edgelist_type edges = pop_edgelist_back();
     size_type nb_edges1 = edges.size();
     if (nb_edges1 > nb) {
     size_type nb_edges_to_keep = nb_edges1 - nb;
     // FORMIKE: could have a split(edges, nb_to_keep, edges_dst) method to combine take/drop
     edgelist_type edges_to_keep = edgelist_type::take(edges, nb_edges_to_keep);
     edges = edgelist_type::drop(edges, nb_edges_to_keep);
     push_edgelist_back(edges_to_keep);
     }
     size_type nb_edges2 = edges.size();
     assert(nb_edges2 <= nb);
     assert(nb_edges2 >= 0);
     size_type nb_outedges2 = nb_outedges();
     assert(nb_outedges2 + nb_edges2 == nb_outedges1);
     check();
     return edges;
     } */
    
    /* The container is erased after the first `nb` edges.
     *
     * The erased edges are moved to `other`.
     */
    void split(size_type nb, self_type& other) {
      check();// other.check();
      assert(other.nb_outedges() == 0);
      size_type nb_outedges1 = nb_outedges();
      assert(nb_outedges1 >= nb);
      if (nb_outedges1 == nb)
        return;
      size_type nb_f = f.size();
      size_type nb_m = nb_outedges_of_middle();
      if (nb <= nb_f) { // target in front edgelist
        m.swap(other.m);
        b.swap(other.b);
        edgelist_type edges = f;
        f = edgelist_type::take(edges, nb);
        other.f = edgelist_type::drop(edges, nb);
        nb -= f.size();
      } else if (nb <= nb_f + nb_m) { // target in middle sequence
        b.swap(other.b);
        nb -= nb_f;
        vtxid_type middle_vertex = -1000;
        bool found = m.split([nb] (vtxid_type n) { return nb <= n; }, middle_vertex, other.m);
        assert(found && middle_vertex != -1000);
        edgelist_type edges = create_edgelist(middle_vertex);
        nb -= nb_outedges_of_middle();
        b = edgelist_type::take(edges, nb);
        other.f = edgelist_type::drop(edges, nb);
        nb -= b.size();
      } else { // target in back edgelist
        nb -= nb_f + nb_m;
        edgelist_type edges = b;
        b = edgelist_type::take(edges, nb);
        other.b = edgelist_type::drop(edges, nb);
        nb -= b.size();
      }
      size_type nb_outedges2 = nb_outedges();
      size_type nb_other_outedges = other.nb_outedges();
      assert(nb_outedges1 == nb_outedges2 + nb_other_outedges);
      assert(nb == 0);
      check(); other.check();
    }
    
    // concatenate with data of `other`; leaving `other` empty
    // pre: back edgelist empty
    // pre: front edgelist of `other` empty
    void concat(self_type& other) {
      size_type nb_outedges1 = nb_outedges();
      size_type nb_outedges2 = other.nb_outedges();
      assert(b.size() == 0);
      assert(other.f.size() == 0);
      m.concat(other.m);
      b.swap(other.f);
      assert(nb_outedges() == nb_outedges1 + nb_outedges2);
      assert(other.nb_outedges() == 0);
    }
    // FORMIKE: let's call the function above "concat_core",
    // and have a function "concat" that pushes s1.b and s2.f into the middle sequences
    
    void swap(self_type& other) {
      check();
      other.check();
      // size_type nb1 = nb_outedges();
      // size_type nb2 = other.nb_outedges();
      f.swap(other.f);
      m.swap(other.m);
      b.swap(other.b);
      // assert(nb2 == nb_outedges());
      // assert(nb1 == other.nb_outedges());
      check(); other.check();
    }
    
    void clear_when_front_and_back_empty() {
      check();
      m.clear();
      assert(nb_outedges() == 0);
      check();
    }
    
    void clear() {
      check();
      f = edgelist_type();
      m.clear();
      b = edgelist_type();
      assert(nb_outedges() == 0);
      check();
    }
    
    template <class Body>
    void for_each_edgelist(const Body& func) const {
      if (f.size() > 0)
        func(f);
      m.for_each([&] (vtxid_type v) { func(create_edgelist(v)); });
      if (b.size() > 0)
        func(b);
    }
    
    template <class Body>
    void for_each_edgelist_when_front_and_back_empty(const Body& func) const {
      m.for_each([&] (vtxid_type v) { func(create_edgelist(v)); });
    }
    
    template <class Body>
    void for_each_outedge_when_front_and_back_empty(const Body& func) const {
      for_each_edgelist_when_front_and_back_empty([&] (edgelist_type edges) {
        for (auto e = edges.lo; e < edges.hi; e++)
          func(*e);
      });
    }
    
    template <class Body>
    void for_each_outedge(const Body& func) const {
      for_each_edgelist([&] (edgelist_type edges) {
        for (auto e = edges.lo; e < edges.hi; e++)
          func(*e);
      });
    }
    
    // Warning: "func" may only call "push_vertex_back"
    // Returns the number of edges that have been processed
    template <class Body>
    size_type for_at_most_nb_outedges(size_type nb, const Body& func) {
      size_type nb_left = nb;
      size_type f_size = f.size();
      // process front if not empty
      if (f_size > 0) {
        if (f_size >= nb_left) {
          // process only part of the front
          auto e = edgelist_type::take(f, nb_left);
          f = edgelist_type::drop(f, nb_left);
          e.for_each(func);
          return nb;
        } else {
          // process all of the front, to begin with
          nb_left -= f_size;
          f.for_each(func);
          f.clear();
        }
      }
      // assume now front to be empty, and work on middle
      while (nb_left > 0 && ! m.empty()) {
        vtxid_type v = m.pop_back();
        edgelist_type edges = create_edgelist(v);
        size_type d = edges.size();
        if (d <= nb_left) {
          // process all of the edges associated with v
          edges.for_each(func);
          nb_left -= d;
        } else { // d > nb_left
          // save the remaining edges into the front
          f = edgelist_type::drop(edges, nb_left);
          auto edges2 = edgelist_type::take(edges, nb_left);
          edges2.for_each(func);
          return nb;
        }
      }
      // process the back if not empty
      size_type b_size = b.size();
      if (nb_left > 0 && b_size > 0) {
        if (b_size >= nb_left) {
          // process only part of the back, leave the rest to the front
          auto e = edgelist_type::take(b, nb_left);
          f = edgelist_type::drop(b, nb_left);
          b.clear();
          e.for_each(func);
          return nb;
        } else {
          // process all of the back
          nb_left -= b_size;
          b.for_each(func);
          b.clear();
        }
      }
      return nb - nb_left;
    }
    
    graph_type get_graph() const {
      return m.get_measure().get_env().g;
    }
    
    void set_graph(graph_type g) {
      using measure_type = typename cache_type::measure_type;
      graph_env env(g);
      measure_type meas(env);
      m.set_measure(meas);
    }
    
  };
  
  /*---------------------------------------------------------------------*/
  
  static constexpr int chunk_capacity = 1024;
  
  template <
  class Vertex,
  class Cache_policy
  >
  using chunkedbag = pasl::data::chunkedseq::bootstrapped::bagopt<Vertex, chunk_capacity, Cache_policy>;
  
  template <
  class Vertex,
  class Cache_policy
  >
  using chunkedstack = pasl::data::chunkedseq::bootstrapped::stack<Vertex, chunk_capacity, Cache_policy>;
  
} // end namespace

/*---------------------------------------------------------------------*/

template <class Graph>
using frontiersegbag = frontiersegbase::frontiersegbase<Graph, frontiersegbase::chunkedbag>;

template <class Graph>
using frontiersegstack = frontiersegbase::frontiersegbase<Graph, frontiersegbase::chunkedstack>;

template <class Vertex_id, bool Is_alias = false>
using flat_adjlist = adjlist<flat_adjlist_seq<Vertex_id, Is_alias>>;

template <class Vertex_id>
using flat_adjlist_alias = flat_adjlist<Vertex_id, true>;

template <class Vertex_id>
flat_adjlist_alias<Vertex_id> get_alias_of_adjlist(const flat_adjlist<Vertex_id>& graph) {
  flat_adjlist_alias<Vertex_id> alias;
  alias.adjlists = graph.adjlists.get_alias();
  alias.nb_edges = graph.nb_edges;
  return alias;
}
  
template <class Index, class Item>
bool try_to_mark_non_idempotent(std::atomic<Item>* visited, Index target) {
  Item orig = 0;
  if (! compare_exchange(visited[target], orig, 1))
    return false;
  return true;
}

template <class Adjlist, class Item, bool idempotent>
bool try_to_mark(const Adjlist& graph,
                 std::atomic<Item>* visited,
                 typename Adjlist::vtxid_type target) {
  using vtxid_type = typename Adjlist::vtxid_type;
  const vtxid_type max_outdegree_for_idempotent = 30;
  if (visited[target].load(std::memory_order_relaxed))
    return false;
  if (idempotent) {
    if (graph.adjlists[target].get_out_degree() <= max_outdegree_for_idempotent) {
      visited[target].store(1, std::memory_order_relaxed);
      return true;
    } else {
      return try_to_mark_non_idempotent<vtxid_type,Item>(visited, target);
    }
  } else {
    return try_to_mark_non_idempotent<vtxid_type,Item>(visited, target);
  }
}
  
template <class Number, class Size>
void fill_array_seq(Number* array, Size sz, Number val) {
  memset(array, val, sz*sizeof(Number));
  // for (Size i = Size(0); i < sz; i++)
  //   array[i] = val;
}
  
template <
class Adjlist_seq,
bool report_nb_edges_processed = false,
bool report_nb_vertices_visited = false
>
int* dfs_by_vertexid_array(const adjlist<Adjlist_seq>& graph,
                           typename adjlist<Adjlist_seq>::vtxid_type source,
                           long* nb_edges_processed = nullptr,
                           long* nb_vertices_visited = nullptr,
                           int* visited_from_caller = nullptr) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  if (report_nb_edges_processed)
    *nb_edges_processed = 0;
  if (report_nb_vertices_visited)
    *nb_vertices_visited = 1;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  int* visited;
  if (visited_from_caller != nullptr) {
    visited = visited_from_caller;
    // don't need to initialize visited
  } else {
    visited = malloc_array<int>(nb_vertices);
    fill_array_seq(visited, nb_vertices, 0);
  }
  LOG_BASIC(ALGO_PHASE);
  vtxid_type* frontier = malloc_array<vtxid_type>(nb_vertices);
  vtxid_type frontier_size = 0;
  frontier[frontier_size++] = source;
  visited[source] = 1;
  while (frontier_size > 0) {
    vtxid_type vertex = frontier[--frontier_size];
    vtxid_type degree = graph.adjlists[vertex].get_out_degree();
    vtxid_type* neighbors = graph.adjlists[vertex].get_out_neighbors();
    if (report_nb_edges_processed)
      (*nb_edges_processed) += degree;
    for (vtxid_type edge = 0; edge < degree; edge++) {
      vtxid_type other = neighbors[edge];
      if (visited[other])
        continue;
      if (report_nb_vertices_visited)
        (*nb_vertices_visited)++;
      visited[other] = 1;
      frontier[frontier_size++] = other;
    }
  }
  free(frontier);
  return visited;
}

int pdfs_split_cutoff = 128;
int pdfs_poll_cutoff = 16;

template <class node, class Frontier, class Adjlist_alias>
class pdfs_rec : public node {
public:
  
  using vtxid_type = typename Adjlist_alias::vtxid_type;
  using edgelist_type = typename Frontier::edgelist_type;
  using graph_alias_type = typename Adjlist_alias::alias_type;
  
  Frontier frontier;
  std::atomic<int>* visited;
  graph_alias_type graph_alias;
  
  int nb_since_last_split;
  node* join;
  
  pdfs_rec(Adjlist_alias graph_alias, std::atomic<int>* visited, node* join)
  : visited(visited), join(join), graph_alias(graph_alias) {
    frontier.set_graph(graph_alias);
  }
  
  enum {
    entry,
    loop_header,
    loop_body,
    exit
  };
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        nb_since_last_split = 0;
        node::jump_to(loop_header);
        break;
      }
      case loop_header: {
        if (frontier.nb_outedges() > 0) {
          node::jump_to(loop_body);
        } else {
          node::jump_to(exit);
        }
        break;
      }
      case loop_body: {
        nb_since_last_split +=
          frontier.for_at_most_nb_outedges(pdfs_poll_cutoff, [&](vtxid_type other_vertex) {
            if (try_to_mark<graph_alias_type, int, false>(graph_alias, visited, other_vertex)) {
              frontier.push_vertex_back(other_vertex);
            }
          });
        node::jump_to(loop_header);
        break;
      }
      case exit: {
        break;
      }
    }
  }
  
  size_t size() {
    auto f = frontier.nb_outedges();
    if (f == 0) {
      nb_since_last_split = 0;
      return 0;
    }
    if (f > pdfs_split_cutoff
        || (nb_since_last_split > pdfs_split_cutoff && f > 1)) {
      return f;
    } else {
      return 1;
    }
  }
  
  pasl::sched::thread_p split(size_t) {
    assert(frontier.nb_outedges() >= 2);
    auto n = new pdfs_rec(graph_alias, visited, join);
    node::split_with(n, join);
    auto m = frontier.nb_outedges() / 2;
    frontier.split(m, n->frontier);
    frontier.swap(n->frontier);
    nb_since_last_split = 0;
    return n;
  }
  
};

template <class Vertex_id>
class graph_constants {
public:
  static constexpr Vertex_id unknown_vtxid = Vertex_id(-1);
};

template <class node, class Frontier, class Adjlist>
class pdfs : public node {
public:
  
  using vtxid_type = typename Adjlist::vtxid_type;
  using edgelist_type = typename Frontier::edgelist_type;
  
  Adjlist graph;
  std::atomic<int>* visited;
  std::atomic<int>** result;
  vtxid_type source;
  
  pdfs(Adjlist graph, vtxid_type source, std::atomic<int>** result)
  : source(source), graph(graph), result(result) { }
  
  enum {
    entry,
    dfs,
    exit
  };
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        auto nb_vertices = graph.get_nb_vertices();
        visited = malloc_array<std::atomic<int>>(nb_vertices);
        node::call(new pmemset<node>((char*)visited, 0, nb_vertices * sizeof(std::atomic<int>)),
                   dfs);
        break;
      }
      case dfs: {
#ifndef NDEBUG
        for (long i = 0; i < graph.get_nb_vertices(); i++) {
          assert(visited[i].load() == 0);
        }
#endif
        auto n = new pdfs_rec<node, Frontier, Adjlist>(graph, visited, this);
        visited[source].store(1);
        n->frontier.push_vertex_back(source);
        node::finish(n,
                     exit);
        break;
      }
      case exit: {
        *result = visited;
        break;
      }
    }
  }
  
};

#define PUSH_ZERO_ARITY_VERTICES 0

template <class Adjlist_seq>
typename adjlist<Adjlist_seq>::vtxid_type*
bfs_by_dual_arrays(const adjlist<Adjlist_seq>& graph,
                   typename adjlist<Adjlist_seq>::vtxid_type source) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  vtxid_type* dists = malloc_array<vtxid_type>(nb_vertices);
  std::fill(dists, dists+nb_vertices, unknown);
  LOG_BASIC(ALGO_PHASE);
  vtxid_type* stacks[2];
  stacks[0] = malloc_array<vtxid_type>(graph.get_nb_vertices());
  stacks[1] = malloc_array<vtxid_type>(graph.get_nb_vertices());
  vtxid_type nbs[2]; // size of the stacks
  nbs[0] = 0;
  nbs[1] = 0;
  vtxid_type cur = 0; // either 0 or 1, depending on parity of dist
  vtxid_type nxt = 1; // either 1 or 0, depending on parity of dist
  vtxid_type dist = 0;
  dists[source] = 0;
  stacks[cur][nbs[cur]++] = source;
  while (nbs[cur] > 0) {
    vtxid_type nb = nbs[cur];
    for (vtxid_type ix = 0; ix < nb; ix++) {
      vtxid_type vertex = stacks[cur][ix];
      vtxid_type degree = graph.adjlists[vertex].get_out_degree();
      vtxid_type* neighbors = graph.adjlists[vertex].get_out_neighbors();
      for (vtxid_type edge = 0; edge < degree; edge++) {
        vtxid_type other = neighbors[edge];
        if (dists[other] != unknown)
          continue;
        dists[other] = dist+1;
        if (PUSH_ZERO_ARITY_VERTICES || graph.adjlists[other].get_out_degree() > 0)
          stacks[nxt][nbs[nxt]++] = other;
      }
    }
    nbs[cur] = 0;
    cur = 1 - cur;
    nxt = 1 - nxt;
    dist++;
  }
  free(stacks[0]);
  free(stacks[1]);
  return dists;
}
  
int pbfs_cutoff = 1024;
int pbfs_polling_cutoff = 1024;

template <class Index, class Item>
static bool pbfs_try_to_set_dist(Index target,
                                 Item unknown, Item dist,
                                 std::atomic<Item>* dists) {
  if (dists[target].load(std::memory_order_relaxed) != unknown)
    return false;
  else if (! compare_exchange(dists[target], unknown, dist))
    return false;
  return true;
}

template <class node, class Frontier, class Adjlist_alias>
class pbfs_process_layer : public node {
public:
  
  using vtxid_type = typename Adjlist_alias::vtxid_type;
  using edgelist_type = typename Frontier::edgelist_type;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  using graph_alias_type = typename Adjlist_alias::alias_type;
  using outset_type = outset_of<node>;
  
  graph_alias_type graph_alias;
  Frontier prev;
  Frontier next;
  vtxid_type dist_of_next;
  std::atomic<vtxid_type>* dists;
  Frontier* _next;
  
  std::vector<std::pair<outset_type*, Frontier*>> futures;
  
  pbfs_process_layer(Adjlist_alias graph_alias,
                     vtxid_type dist_of_next,
                     std::atomic<vtxid_type>* dists,
                     Frontier* _prev,
                     Frontier* _next)
  : graph_alias(graph_alias), prev(graph_alias), next(graph_alias),
    dist_of_next(dist_of_next), dists(dists), _next(_next) {
    _prev->swap(prev);
  }
  
  enum {
    entry,
    process_loop_header,
    process_loop_body,
    concat_loop_header,
    concat_loop_body,
    concat_loop_after_force,
    exit
  };
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        node::jump_to(process_loop_header);
        break;
      }
      case process_loop_header: {
        if (prev.nb_outedges() > 0) {
          node::jump_to(process_loop_body);
        } else {
          node::jump_to(concat_loop_header);
        }
        break;
      }
      case process_loop_body: {
        prev.for_at_most_nb_outedges(pbfs_polling_cutoff, [&] (vtxid_type other) {
          if (pbfs_try_to_set_dist(other, unknown, dist_of_next, dists)) {
            next.push_vertex_back(other);
          }
        });
        node::jump_to(process_loop_header);
        break;
      }
      case concat_loop_header: {
        if (futures.empty()) {
          node::jump_to(exit);
        } else {
          node::jump_to(concat_loop_body);
        }
        break;
      }
      case concat_loop_body: {
        node::force(futures.back().first,
                    concat_loop_after_force);
        break;
      }
      case concat_loop_after_force: {
        auto p = futures.back();
        futures.pop_back();
        next.concat(*p.second);
        delete p.second;
        node::deallocate_future(p.first);
        node::jump_to(concat_loop_header);
        break;
      }
      case exit: {
        _next->swap(next);
        break;
      }
    }
  }
  
  size_t size() {
    return prev.nb_outedges();
  }
  
  pasl::sched::thread_p split(size_t) {
    Frontier prev2;
    assert(prev.nb_outedges() >= 2);
    prev.split(prev.nb_outedges() / 2, prev2);
    Frontier* next2 = new Frontier(graph_alias);
    auto n = new pbfs_process_layer(graph_alias, dist_of_next, dists, &prev2, next2);
    assert(prev2.nb_outedges() == 0);
    outset_type* out = node::split_and_join_with(n);
    futures.push_back(std::make_pair(out, next2));
    return n;
  }
  
};

template <class node, class Frontier, class Adjlist_alias>
class pbfs : public node {
public:
  
  using vtxid_type = typename Adjlist_alias::vtxid_type;
  using edgelist_type = typename Frontier::edgelist_type;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  using adjlist_alias_type = typename Adjlist_alias::alias_type;
  
  adjlist_alias_type graph_alias;
  std::atomic<vtxid_type>* dists;
  std::atomic<vtxid_type>** result;
  vtxid_type source;
  Frontier frontiers[2];
  vtxid_type dist = 0;
  vtxid_type cur = 0; // either 0 or 1, depending on parity of dist
  vtxid_type nxt = 1; // either 1 or 0, depending on parity of dist
  
  pbfs(Adjlist_alias graph_alias, vtxid_type source, std::atomic<vtxid_type>** result)
  : source(source), graph_alias(graph_alias), result(result) {
    frontiers[0].set_graph(graph_alias);
    frontiers[1].set_graph(graph_alias);
  }
  
  enum {
    entry,
    init_source,
    loop_header,
    loop_body,
    exit
  };
  
  void body() {
    switch (node::current_block_id) {
      case entry: {
        auto nb_vertices = graph_alias.get_nb_vertices();
        dists = malloc_array<std::atomic<vtxid_type>>(nb_vertices);
        node::call(new pfill<node, std::atomic<vtxid_type>*, vtxid_type>(dists, dists + nb_vertices, unknown),
                   init_source);
        break;
      }
      case init_source: {
        dists[source].store(dist);
        frontiers[0].push_vertex_back(source);
        node::jump_to(loop_header);
        break;
      }
      case loop_header: {
        if (frontiers[cur].empty()) {
          node::jump_to(exit);
        } else {
          node::jump_to(loop_body);
        }
        break;
      }
      case loop_body: {
        dist++;
        if (frontiers[cur].nb_outedges() <= pbfs_cutoff) {
          frontiers[cur].for_each_outedge_when_front_and_back_empty([&] (vtxid_type other) {
            if (pbfs_try_to_set_dist(other, unknown, dist, dists)) {
              frontiers[nxt].push_vertex_back(other);
            }
          });
          frontiers[cur].clear_when_front_and_back_empty();
          node::jump_to(loop_header);
        } else {
          auto n = new pbfs_process_layer<node, Frontier, adjlist_alias_type>(graph_alias,
                                                                              dist,
                                                                              dists,
                                                                              &frontiers[cur],
                                                                              &frontiers[nxt]);
          node::call(n,
                     loop_header);
        }
        cur = 1 - cur;
        nxt = 1 - nxt;
        break;
      }
      case exit: {
        *result = dists;
        break;
      }
    }
  }
  
};
  
} // end namespace

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/

void test_random_number_generator() {
  constexpr int nb_buckets = 40;
  constexpr int nb_rounds = 100000;
  unsigned int buckets[nb_buckets];
  unsigned int rng = 123;
  for (int j = 0; j < nb_buckets; j++) {
    buckets[j] = 0;
  }
#ifndef USE_STL_RANDGEN
  for (int i = 0; i < nb_rounds; i++) {
    int k = random_int_in_range(rng, 0, nb_buckets);
    buckets[k]++;
  }
#endif
  unsigned int maxv = buckets[0];
  unsigned int minv = buckets[0];
  for (int i = 0; i < nb_buckets; i++) {
    maxv = std::max(buckets[i], maxv);
    minv = std::min(buckets[i], minv);
  }
  std::cout << "max = " << maxv << std::endl;
  std::cout << "min = " << minv << std::endl;
}

namespace cmdline = pasl::util::cmdline;

using todo_type = enum { todo_measured, todo_administrative };

std::deque<std::pair<todo_type, pasl::sched::thread_p>> todo;

void add_todo(todo_type tp, pasl::sched::thread_p t) {
  todo.push_back(std::make_pair(tp, t));
}

void add_measured(pasl::sched::thread_p t) {
  add_todo(todo_measured, t);
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
  add_todo(todo_administrative, new todo_function<decltype(f)>(f));
}

void add_measured(std::function<void()> f) {
  add_todo(todo_measured, new todo_function<decltype(f)>(f));
}

bool should_force_simple_algorithm() {
  bool result = false;
  if (cmdline::parse_or_default_string("cmd","") == "seidel_forkjoin") {
    result = true;
  }
  return result;
}

void choose_edge_algorithm() {
  cmdline::argmap_dispatch c;
  c.add("simple", [&] {
    direct::edge_algorithm = direct::edge_algorithm_simple;
  });
  c.add("statreeopt", [&] {
    direct::edge_algorithm = direct::edge_algorithm_statreeopt;
  });
  c.add("growabletree", [&] {
    direct::edge_algorithm = direct::edge_algorithm_growabletree;
  });
  c.find_by_arg_or_default_key("edge_algo", "tree")();
}

void read_seidel_params(int& numiters, int& N, int& block_size) {
  numiters = cmdline::parse_or_default_int("numiters", 1);
  N = cmdline::parse_or_default_int("N", 128);
  int block_size_lg = cmdline::parse_or_default_int("block_size_lg", 2);
  block_size = 1<<block_size_lg;
  benchmarks::epsilon = cmdline::parse_or_default_double("epsilon", benchmarks::epsilon);
}

template <class seidel_parallel>
void do_seidel() {
  int numiters;
  int N;
  int block_size;
  bool do_consistency_check = cmdline::parse_or_default_bool("consistency_check", false);
  read_seidel_params(numiters, N, block_size);
  benchmarks::matrix_type<double>* test_mtx = new benchmarks::matrix_type<double>(N+2, 0.0);
  benchmarks::seidel_initialize(*test_mtx);
  add_measured(new seidel_parallel(numiters, N+2, block_size, &(test_mtx->items[0])));
  add_todo([=] {
    if (do_consistency_check) {
      benchmarks::matrix_type<double> reference_mtx(N+2, 0.0);
      benchmarks::seidel_initialize(reference_mtx);
      benchmarks::seidel_sequential(numiters, N+2, block_size, &(reference_mtx.items[0]));
      int nb_diffs = benchmarks::count_nb_diffs(reference_mtx, *test_mtx);
      assert(nb_diffs == 0);
    }
    delete test_mtx;
  });
}

template <class Adjlist, class Load_visited_fct>
void report_dfs_results(const Adjlist& graph,
                        const Load_visited_fct& load_visited_fct) {
  using vtxid_type = typename Adjlist::vtxid_type;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  //  vtxid_type nb_visited = pbbs::sequence::plusReduce((vtxid_type*)nullptr, nb_vertices, load_visited_fct);
  vtxid_type nb_visited = 0;
  for (long i = 0; i < nb_vertices; i++) {
    nb_visited += load_visited_fct(i);
  }
  std::cout << "nb_visited\t" << nb_visited << std::endl;
}

template <class Adjlist, class Counter_cell, class Load_dist_fct>
void report_bfs_results(const Adjlist& graph,
                         Counter_cell unknown,
                         const Load_dist_fct& load_dist_fct) {
  using vtxid_type = typename Adjlist::vtxid_type;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  //  vtxid_type max_dist = pbbs::sequence::maxReduce((vtxid_type*)nullptr, nb_vertices, load_dist_fct);
  vtxid_type max_dist = 0;
  for (long i = 0; i < nb_vertices; i++) {
    max_dist = std::max(max_dist, load_dist_fct(i));
  }
  auto is_visited = [&] (vtxid_type i) { return load_dist_fct(i) == unknown ? 0 : 1; };
  //  vtxid_type nb_visited = pbbs::sequence::plusReduce((vtxid_type*)nullptr, nb_vertices, is_visited);
  vtxid_type nb_visited = 0;
  for (long i = 0; i < nb_vertices; i++) {
    nb_visited += is_visited(i);
  }
  std::cout << "max_dist\t" << max_dist << std::endl;
  std::cout << "nb_visited\t" << nb_visited << std::endl;
}

template <class node, class Adjlist>
void launch_graph_benchmark_for_representation(std::string bench) {
  using vtxid_type = typename Adjlist::vtxid_type;
  using adjlist_type = Adjlist;
  using adjlist_alias_type = typename adjlist_type::alias_type;
  using frontier_type = benchmarks::frontiersegbag<adjlist_alias_type>;
  int source = cmdline::parse_or_default_int("source", 0);
  std::string infile = cmdline::parse_or_default_string("infile", "");
  adjlist_type* graph = new adjlist_type;
  std::atomic<int>** pdfs_visited = new std::atomic<int>*;
  int** dfs_visited = new int*;
  std::atomic<vtxid_type>** pbfs_dists = new std::atomic<vtxid_type>*;
  vtxid_type** bfs_dists = new vtxid_type*;
  *dfs_visited = nullptr;
  *pdfs_visited = nullptr;
  *bfs_dists = nullptr;
  *pbfs_dists = nullptr;
  benchmarks::read_adjlist_from_file<vtxid_type>(infile, *graph);
  auto graph_alias = benchmarks::get_alias_of_adjlist(*graph);
  if (bench == "dfs") {
    add_measured([=] {
      *dfs_visited = benchmarks::dfs_by_vertexid_array(*graph, source);
    });
  } else if (bench == "pdfs") {
    add_measured(new benchmarks::pdfs<node, frontier_type, adjlist_alias_type>(graph_alias, source, pdfs_visited));
  } else if (bench == "bfs") {
    add_measured([=] {
      *bfs_dists = benchmarks::bfs_by_dual_arrays(*graph, source);
    });
  } else if (bench == "pbfs") {
    add_measured(new benchmarks::pbfs<node, frontier_type, adjlist_alias_type>(graph_alias, source, pbfs_dists));
  } else {
    assert(false);
  }
  add_todo([=]{
    if (*dfs_visited != nullptr) {
      report_dfs_results(*graph, [&] (vtxid_type i) { return (*dfs_visited)[i]; });
    } else if (*pdfs_visited != nullptr) {
      report_dfs_results(*graph, [&] (vtxid_type i) { return (*pdfs_visited)[i].load(); });
    } else if (*bfs_dists != nullptr) {
      using vtxid_type = typename Adjlist::vtxid_type;
      vtxid_type unknown = benchmarks::graph_constants<vtxid_type>::unknown_vtxid;
      report_bfs_results(*graph, unknown, [&] (vtxid_type i) { return (*bfs_dists)[i]; });
    } else if (*pbfs_dists != nullptr) {
      using vtxid_type = typename Adjlist::vtxid_type;
      vtxid_type unknown = benchmarks::graph_constants<vtxid_type>::unknown_vtxid;
      report_bfs_results(*graph, unknown, [&] (vtxid_type i) { return (*pbfs_dists)[i].load(); });
    }
    delete graph;
    if (*dfs_visited != nullptr) {
      free(*dfs_visited);
    } else if (*pdfs_visited != nullptr) {
      free(*pdfs_visited);
    } else if (*bfs_dists != nullptr) {
      free(*bfs_dists);
    } else if (*pbfs_dists != nullptr) {
      free(*pbfs_dists);
    }
    delete dfs_visited;
    delete pdfs_visited;
    delete bfs_dists;
    delete pbfs_dists;
  });
}

template <class node>
void launch_graph_benchmark(std::string bench) {
  using vtxid_type32 = int;
  using adjlist_seq_type32 = benchmarks::flat_adjlist_seq<vtxid_type32, false>;
  using adjlist_type32 = benchmarks::adjlist<adjlist_seq_type32>;
  using vtxid_type64 = long;
  using adjlist_seq_type64 = benchmarks::flat_adjlist_seq<vtxid_type64, false>;
  using adjlist_type64 = benchmarks::adjlist<adjlist_seq_type64>;
  int nb_bits = cmdline::parse_or_default_int("bits", 32);
  if (nb_bits == 32) {
    launch_graph_benchmark_for_representation<node, adjlist_type32>(bench);
  } else {
    launch_graph_benchmark_for_representation<node, adjlist_type64>(bench);
  }
}

std::string cmd_param = "cmd";

template <class node>
void choose_command() {
  cmdline::argmap_dispatch c;
  c.add("incounter_async_duration", [&] {
    int nb_milliseconds = cmdline::parse_int("nb_milliseconds");
    std::thread timer([&, nb_milliseconds] {
      std::this_thread::sleep_for(std::chrono::milliseconds(nb_milliseconds));
      benchmarks::should_incounter_async_duration_terminate = true;
    });
    timer.detach();
    add_measured(new benchmarks::incounter_async_duration<node>);
  });
  c.add("mixed_duration", [&] {
    int nb_milliseconds = cmdline::parse_int("nb_milliseconds");
    add_measured(new benchmarks::mixed_duration<node>(nb_milliseconds));
  });
  c.add("mixed_nb", [&] {
    int n = cmdline::parse_or_default_int("n", 1);
    add_measured(new benchmarks::mixed_nb<node>(n));
  });
  c.add("incounter_async_nb", [&] {
    int n = cmdline::parse_or_default_int("n", 1);
    add_measured(new benchmarks::incounter_async_nb<node>(n));
  });
  c.add("incounter_forkjoin_nb", [&] {
    int n = cmdline::parse_or_default_int("n", 1);
    add_measured(new benchmarks::incounter_forkjoin_nb<node>(n));
  });
  c.add("async_bintree", [&] {
    int n = cmdline::parse_or_default_int("n", 1);
    add_measured(new tests::async_bintree<node>(n));
  });
  c.add("future_bintree", [&] {
    int n = cmdline::parse_or_default_int("n", 1);
    add_measured(new tests::future_bintree<node>(n));
  });
  c.add("future_pool", [&] {
    int n = cmdline::parse_or_default_int("n", 1);
    tests::fib_input = cmdline::parse_or_default_int("fib_input", tests::fib_input);
    add_measured(new tests::future_pool<node>(n));
  });
  c.add("parallel_for_test", [&] {
    int n = cmdline::parse_or_default_int("n", 1);
    add_measured(new tests::parallel_for_test<node>(n));
  });
  c.add("seidel_async", [&] {
    do_seidel<benchmarks::seidel_async<node>>();
  });
  c.add("dfs", [&] {
    launch_graph_benchmark<node>("dfs");
  });
  c.add("pdfs", [&] {
    launch_graph_benchmark<node>("pdfs");
  });
  c.add("bfs", [&] {
    launch_graph_benchmark<node>("bfs");
  });
  c.add("pbfs", [&] {
    launch_graph_benchmark<node>("pbfs");
  });
  c.find_by_arg(cmd_param)();
}

void launch() {
  communication_delay = cmdline::parse_or_default_int("communication_delay",
                                                      communication_delay);
  if (should_force_simple_algorithm()) {
    direct::edge_algorithm = direct::edge_algorithm_simple;
    choose_command<direct::node>();
  } else {
    cmdline::argmap_dispatch c;
    c.add("direct", [&] {
        choose_edge_algorithm();
        choose_command<direct::node>();
      });
    c.add("portpassing", [&] {
        choose_command<portpassing::node>();
      });
    c.find_by_arg("algo")();
  }
  while (! todo.empty()) {
    std::pair<todo_type, pasl::sched::thread_p> p = todo.front();
    todo.pop_front();
    pasl::sched::thread_p t = p.second;
    if (p.first == todo_measured) {
      LOG_BASIC(ENTER_ALGO);
      auto start = std::chrono::system_clock::now();
      pasl::sched::threaddag::launch(t);
      auto end = std::chrono::system_clock::now();
      LOG_BASIC(EXIT_ALGO);
      STAT_IDLE(sum());
      STAT(dump(stdout));
      STAT_IDLE(print_idle(stdout));
      std::chrono::duration<float> diff = end - start;
      printf ("exectime %.3lf\n", diff.count());
    } else {
      pasl::sched::threaddag::launch(t);
    }
  }
}

template <class Benchmark>
void launch_sequential_baseline_benchmark(const Benchmark& benchmark) {
  auto start = std::chrono::system_clock::now();
  benchmark();
  auto end = std::chrono::system_clock::now();
  std::chrono::duration<float> diff = end - start;
  printf ("exectime %.3lf\n", diff.count());
}

int main(int argc, char** argv) {
  cmdline::set(argc, argv);
  benchmarks::workload = pasl::util::cmdline::parse_or_default_double("workload", 0.0);
  std::string cmd = pasl::util::cmdline::parse_string(cmd_param);
  if (cmd == "incounter_mixed_duration") {
    benchmarks::launch_incounter_mixed_duration();
  } else if (cmd == "outset_add_duration") {
    benchmarks::launch_outset_add_duration();
  } else if (cmd == "snzi_alternated_duration") {
    benchmarks::launch_snzi_alternated_duration();
  } else if (cmd == "seidel_sequential") {
    int numiters;
    int N;
    int block_size;
    read_seidel_params(numiters, N, block_size);
    benchmarks::matrix_type<double>* test_mtx = new benchmarks::matrix_type<double>(N+2, 0.0);
    launch_sequential_baseline_benchmark([&] {
      benchmarks::seidel_sequential(numiters, N+2, block_size, &(test_mtx->items[0]));
    });
    delete test_mtx;
  } else if (cmd == "test_random_number_generator") {
    test_random_number_generator();
  } else {
    pasl::sched::threaddag::init();
    launch();
    pasl::sched::threaddag::destroy();
  }
  return 0;
}

/***********************************************************************/

