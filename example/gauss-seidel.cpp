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

class incounter {
public:
  
  thread_p t;
  
  std::pair<snzi2*,snzi2*> add(snzi2* p) {
    snzi2* p1 = new snzi2;
    snzi2* p2 = new snzi2;
    p1->parent = p;
    p2->parent = p;
    return std::make_pair(p1, p2);
  }
  
  void rem(snzi2* p) {
    while (p->parent != nullptr) {
      snzi2* tmp = p;
      p = p->parent;
      int b = p->nb_finished.load();
      delete tmp;
      int old = 0;
      if (b == 0 && p->nb_finished.compare_exchange_strong(old, 1)) {
        return;
      }
      schedule(t);
      delete this;
    }
  }
  
};
  
} } }

/*---------------------------------------------------------------------*/

namespace pasl {
namespace sched {
namespace outstrategy {
  
  class outlist {
  public:
    
    
    
  };
  
} } }

/*---------------------------------------------------------------------*/

namespace pasl {
namespace sched {
  
  class multishot2  : public thread {
  public:
    
    int block_id;
    int cont_id;
    
    void prepare(int target) {
      threaddag::reuse_calling_thread();
      cont_id = target;
    }
    
    virtual void mrun() = 0;
    
    void run() {
      block_id = cont_id;
      mrun();
    }
    
    void async(multishot2* thread) {
      
    }
    
    void finish(multishot2* thread, int target) {
      
    }
    
    void future(multishot2* thread) {
      
    }
    
    void touch(multishot2* thread) {
      
    }
    
  };
  
}
}


namespace pasl {
  namespace pctl {
    
    
    void ex() {
      std::cout << "hi" << std::endl;
      
    }
    
  }
}

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (bool sequential) {
    pasl::pctl::ex();
  });
  return 0;
}

/***********************************************************************/
