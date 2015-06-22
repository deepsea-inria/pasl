/*!
 * \file gauss-seidel.cpp
 * \brief Benchmarking script for parallel sorting algorithms
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include "pasl.hpp"
#include "io.hpp"
#include "dpsdatapar.hpp"

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
        n1->parent = nullptr;
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
  
class outlist {
public:
  
  
  
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
