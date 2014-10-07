/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file scheduler.hpp
 * \brief Contains the code that controls the main actions of 
 * the scheduler. 
 *
 */


#include <cstdlib>

#include "logging.hpp"
#include "instrategy.hpp"
#include "outstrategy.hpp"
#include "stats.hpp"

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

namespace pasl {
namespace sched {
namespace scheduler {

/***********************************************************************/
  
typedef util::worker::controller_t controller_t;
typedef controller_t* controller_p;

class _private : public signature {
protected:

  /*! \brief Contains either the thread that is currently running on the
   *  worker or NULL if there is no such thread.
   */

  thread_p current_thread;
  outstrategy_p current_outstrategy;
  bool should_communicate;
  bool reuse_thread_requested;
  ticks_t date_enter_wait;

  /*! \brief Returns true if the worker must continue executing its \a run()
   *  method.
   */
  virtual bool stay();


public:
  
  //! Executes the given thread.
  virtual void exec(thread_p t);
  
  virtual void init();
  virtual void destroy();
   
  void new_launch();
  void enter_wait();
  void exit_wait();

  void add_thread(thread_p thread);
  void add_dependency(thread_p t1, thread_p t2);

  outstrategy_p capture_outstrategy();
  void decr_dependencies(thread_p t);
  void reuse_calling_thread();
  thread_p get_current_thread() const;

  void schedule(thread_p t);

  //! Add a thread to the pool of ready threads
  virtual void add_to_pool_of_ready_threads(thread_p t) = 0;

  virtual void check_on_interrupt() {
  }
                
  bool& should_communicate_flag() {
   return should_communicate;
  }
  
  virtual  // TODO: why was the virtual missing? 
  bool should_call_communicate() {
    return false;
  }
 
  virtual  // TODO: why was the virtual missing?                    
  size_t nb_threads() {
    return 0;
  }

  // TODO: needed? redundant with scheduler_t ?
  virtual void reject() { }
  virtual void unblock() { }

};
  
/*---------------------------------------------------------------------*/
  
class _shared {

};

/*---------------------------------------------------------------------*/

/*! \class factory
 *  \brief A class that is able to allocate a set of schedulers, that
 * can then be mapped to each of the pthreads that will run in
 * parallel.
 */
template<class Shared, class Private>
class factory : public util::worker::controller_factory_t {
private:
  Shared* shared;  
public:
  factory() {}

  void create_shared_state() { 
    shared = new Shared();
  }
  void delete_shared_state() {
    delete shared;
  }
  controller_p create_controller() {
    return static_cast<controller_p> (new Private(shared));
  }
  void destroy_controller(controller_p c) {
    Private* priv = static_cast<Private*>(c);
    delete priv;
  }
};
  
/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif // _SCHEDULER_H_
