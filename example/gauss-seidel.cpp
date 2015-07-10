/*!
 * \file gauss-seidel.cpp
 * \brief Benchmarking script for parallel sorting algorithms
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include <random>

#include "pasl.hpp"
#include "io.hpp"
#include "dpsdatapar.hpp"
#include "tagged.hpp"

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Forward declarations */

namespace pasl {
namespace sched {
namespace instrategy {
class snzi2;
} } }

namespace pasl {
namespace sched {
instrategy::snzi2** my_snzi2_node();
std::pair<instrategy::snzi2*,instrategy::snzi2*>* my_snzi2_tmp();
} }

/*---------------------------------------------------------------------*/
/* Incounter */

namespace pasl {
namespace sched {
namespace instrategy {
  
class snzi2 {
public:
  
  snzi2* parent;
  std::atomic<int> nb_finished;
  
  snzi2() {
    parent = nullptr;
    nb_finished.store(0);
  }
  
};
  
class incounter : public common {
public:
  
  void check(thread_p t) {

  }
  
  void delta(thread_p t, int64_t d) {
    if (d == 1) {
      snzi2** cur = my_snzi2_node();
      auto tmp = my_snzi2_tmp();
      snzi2* parent = *cur;
      snzi2* n1;
      snzi2* n2;
      if (parent == nullptr) {
        n1 = new snzi2;
        n2 = nullptr;
      } else {
        n1 = new snzi2;
        n2 = new snzi2;
        n1->parent = parent;
        n2->parent = parent;
      }
      tmp->first  = n1;
      tmp->second = n2;
    } else if (d == -1l) {
      snzi2** cur = my_snzi2_node();
      snzi2* p = *cur;
      assert(p != nullptr);
      while (p->parent != nullptr) {
        snzi2* tmp = p;
        p = p->parent;
        int b = p->nb_finished.load();
        delete tmp;
        int old = 0;
        if (b == 0 && p->nb_finished.compare_exchange_strong(old, 1)) {
          return;
        }
      }
      *cur = nullptr;
      delete p;
      start(t);
    } else {
      assert(false);
    }
  }
  
};
  
} } }

/*---------------------------------------------------------------------*/



/*---------------------------------------------------------------------*/
/* Outlist */

namespace pasl {
namespace sched {
namespace outstrategy {
  
data::perworker::array<std::mt19937> generator;
  
class outlist : public common {
public:
  
  static constexpr int N = 5;
  
  /* one of the following:
   *   - nullptr
   *   - value of type thread_p
   *   - value of type outlist*, 
   *     with the least-significant bit set to 1
   */
  using pointer_type = union {
    outlist* outlist;
    thread_p thread;
  };
  
  std::atomic<int> index;
  std::atomic<pointer_type> items[N];
  
  outlist() {
    index.store(0);
    for (int i = 0; i < N; i++) {
      pointer_type ptr = { .thread = nullptr };
      items[i].store(ptr);
    }
  }
  
  void add(pointer_type v) {
    while (true) { // progress since i increases at every iteration, until N
      int i = index.load();
      if (i == N) {
        break;
      }
      bool b = index.compare_exchange_strong(i, i + 1);
      if (! b) {
        continue;  // concurrent increase of the index
      }
      pointer_type old = { .outlist = nullptr };
      bool b2 = items[i].compare_exchange_strong(old, v);
      assert(b2 || !b2);
      return;
      // if b2 is false, the task has finished concurrently, so we return
      // if b2 is true, then we successfully added the task, and we return
    }
    int k = index.load();
    if (k == -1) {
      return; // task has concurrently finished
    }
    assert(k == N); // all cells filled
    std::uniform_int_distribution<int> distribution(0,N-1);
    int i = distribution(generator.mine()); // could also be based on the processor id
    while (true) { // process since x has a very constrainted evolution
      pointer_type x = items[i].load();
      if (data::tagged::extract_tag<long>(x.outlist) == 1) {
        outlist* s2 = data::tagged::extract_value<outlist*>(x.outlist);
        if (s2 == nullptr) {
          return; // task has concurrently finished
        }
        s2->add(v);
        return;
      }
      outlist* s2 = new outlist();
      if (x.outlist == nullptr) {
        // x can be null because of the delay between the two cas performed above
        pointer_type p = { .outlist = s2 };
        bool b = items[i].compare_exchange_strong(x, p);
        if (b) {
          s2->add(v);
          return;
        } else {
          x = items[i].load(); // concurrently set the cell
        }
      }
      if (data::tagged::extract_value<long>(x.outlist) != 1) {
        s2->index.store(1);
        s2->items[0].store(x);
        pointer_type p = { .outlist = data::tagged::create<outlist*, outlist*>(s2, 1) };
        bool b = items[i].compare_exchange_strong(x, p);
        if (b) {
          return;
        } else {
          delete s2;
          assert(data::tagged::extract_tag<long>(items[i].load()) == 1);
        }
      } else {
        delete s2;
      }
    }
  }
  
  void add(thread_p t) {
    pointer_type ptr = { .thread = t };
    add(ptr);
  }
  
  void finished() {
    int j = 0;
    // first, try to block the index
    while (true) {
      j = index.load();
      if (j == N) {
        break;
      }
      bool b = index.compare_exchange_strong(j, -1);
      if (b) {
        break;
      }
    }
    // then, try to process cells and to block the leaves.
    for (int i = 0; i < j; i++) {
      while (true) {
        pointer_type x = items[i].load();
        if (data::tagged::extract_tag<long>(x) == 1) {
          outlist* s2 = data::tagged::extract_value<outlist*>(x);
          s2->finished();
          break;
        }
      }
      pointer_type x = items[i].load();
      pointer_type p = { .outlist = data::tagged::create<outlist*, outlist*>(nullptr, 1) };
      bool b = items[i].compare_exchange_strong(x, p); // try to block the cell
      if (b) {
        thread_p t = data::tagged::extract_value<thread_p>(x);
        if (t != nullptr) {
          decr_dependencies(t);
        }
      }
    }
  }
  
};
  
} } }

/*---------------------------------------------------------------------*/
/* Multishot thread */

namespace pasl {
namespace sched {
  
class multishot  : public thread {
public:
  
  using self_type = multishot;
  
  const int null_block_id = -1;
  int block_id = null_block_id;
  int cont_id = 0;
  instrategy::snzi2* now = nullptr;
  std::pair<instrategy::snzi2*,instrategy::snzi2*> tmp;
  
  multishot() {
    tmp = std::make_pair(nullptr, nullptr);
  }
  
  void prepare(int target) {
    threaddag::reuse_calling_thread();
    cont_id = target;
  }
  
  virtual void run_multishot() = 0;
  
  void run() {
    block_id = cont_id;
    cont_id = null_block_id;
    assert(block_id != null_block_id);
    run_multishot();
  }
  
  void async(self_type* thread, self_type* join) {
    assert(now != nullptr);
    assert(tmp.first == nullptr);
    assert(tmp.second == nullptr);
    threaddag::fork(thread, join);
    assert(tmp.first != nullptr);
    assert(tmp.second != nullptr);
    now = tmp.first;
    thread->now = tmp.second;
    assert(thread->now != nullptr);
    tmp = std::make_pair(nullptr, nullptr);
  }
  
  void finish(self_type* thread, int target) {
    assert(tmp.first == nullptr);
    assert(tmp.second == nullptr);
    prepare(target);
    instrategy::incounter* in = new instrategy::incounter();
    threaddag::unary_fork_join(thread, this, in);
    assert(tmp.first != nullptr);
    assert(tmp.second == nullptr);
    thread->now = tmp.first;
    tmp = std::make_pair(nullptr, nullptr);
  }
  
  void jump_to(int target) {
    prepare(target);
    threaddag::continue_with(this);
  }
  
  void future(self_type* thread) {
    assert(false);
  }
  
  void touch(self_type* thread) {
    assert(false);
  }
  
  THREAD_COST_UNKNOWN
};
  
multishot* my_thread() {
  multishot* t = (multishot*)threaddag::my_sched()->get_current_thread();
  assert(t != nullptr);
  return t;
}

instrategy::snzi2** my_snzi2_node() {
  multishot* t = my_thread();
  return &(t->now);
}
  
std::pair<instrategy::snzi2*,instrategy::snzi2*>* my_snzi2_tmp() {
  multishot* t = my_thread();
  return &(t->tmp);
}
  
} }

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/* Async/finish benchmark */

int cnt = 0;

class generate_async_tree_rec : public pasl::sched::multishot {
public:
  
  enum { entry=0 };
  
  int depth;
  multishot* join;
  
  generate_async_tree_rec(int depth, multishot* join)
  : depth(depth), join(join) { }
  
  void run_multishot() {
    assert(now != nullptr);
    switch (block_id) {
      case entry: {
        pasl::util::atomic::msg([&] {
          //std::cout << "generate_async_tree_rec(" << depth << ")" << std::endl;
          cnt++;
        });
        if (depth == 1) {
          ;
        } else {
          int d = depth-1;
          multishot* t1 = new generate_async_tree_rec(d, join);
          multishot* t2 = new generate_async_tree_rec(d, join);
          async(t1, join);
          async(t2, join);
        }
        break;
      }
      default: {
        assert(false);
        break;
      }
    }
    assert(now != nullptr);
  }
  
};

class generate_async_tree : public pasl::sched::multishot {
public:
  
  enum { entry=0, exit };
  
  int depth;
  
  generate_async_tree(int depth)
  : depth(depth) { }
  
  void run_multishot() {
    switch (block_id) {
      case entry: {
        pasl::util::atomic::msg([&] {
          std::cout << "generate_async_tree(" << depth << ")" << std::endl;
        });
        multishot* t = new generate_async_tree_rec(depth, this);
        finish(t, exit);
        break;
      }
      case exit: {
        pasl::util::atomic::msg([&] {
          std::cout << "generate_async_tree: finished" << std::endl;
        });
        assert(cnt == ((1<<depth)-1));
        break;
      }
      default: {
        assert(false);
        break;
      }
    }
  }
  
};

/*---------------------------------------------------------------------*/




/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::util::cmdline::set(argc, argv);
  int depth = 14;
  pasl::sched::threaddag::init();
  pasl::sched::threaddag::launch(new generate_async_tree(depth));
  pasl::sched::threaddag::destroy();
  return 0;
}

/***********************************************************************/
