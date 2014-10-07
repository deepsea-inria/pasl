/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file classes.hpp
 * \brief Signatures of all the core classes involved in the
 * scheduler.
 *
 * This file is needed because many classes have back pointers, so the
 * signature of the classes needs to be given up front.
 *
 */

#ifndef _PASL_SCHED_CLASSES_H_
#define _PASL_SCHED_CLASSES_H_

#include "worker.hpp"

/***********************************************************************/

namespace pasl {
namespace sched {
  
/*---------------------------------------------------------------------*/
  
//! \brief ideal size of threads (microseconds)
extern double kappa;
  
class thread;
typedef thread* thread_p;
  
/*---------------------------------------------------------------------*/

namespace instrategy {
  class signature;
}

typedef instrategy::signature instrategy_t;
typedef instrategy_t* instrategy_p;

/*---------------------------------------------------------------------*/

namespace outstrategy {
  class signature;
  class noop;
  class unary;
  class future;
}

typedef outstrategy::signature outstrategy_t;
typedef outstrategy_t* outstrategy_p;

typedef outstrategy::future future_t;
typedef future_t* future_p;
  
/*---------------------------------------------------------------------*/

/*! \addtogroup scheduler
 * @{
 */
namespace scheduler {
  
  /*! \class signature
   *  \brief Represents the part of the scheduler which executes
   *  independently in a worker thread.
   *
   * Worker execution
   * =========================
   *
   * A subclass must implement a scheduler loop, which is a piece of
   * code, represented by a `run()` method, that executes an instance
   * of the scheduler on the worker. This method may return only after
   * `stay()` returns true.
   *
   * When a worker goes idle, it should call `enter_wait()`; once it
   * finds work, it should call `exit_wait()`.
   *
   * Thread deallocation policy
   * =========================
   *
   * After it executes a thread, the scheduler deallocates the thread
   * object, unless the thread itself requests otherwise. A thread can
   * make this request by calling `reuse_thread()`.
   *
   */
  class signature : public util::worker::controller_t {
  public:
    /** @name Thread management */
    
    ///@{
    
    virtual void exec(thread_p thread) = 0;
    
    //! Adds the given thread to the set of ready threads
    virtual void add_thread(thread_p thread) = 0;
    
    virtual bool local_has() {
      assert(false);
      return false;
    }

    virtual thread_p local_peek() {
      assert(false);
      return nullptr;
    }
    
    //! Creates a dependency edge from thread `t2` to `t1`.
    virtual void add_dependency(thread_p t1, thread_p t2) = 0;
    
    //! Notify the scheduler from the existence of a ready thread
    virtual void schedule(thread_p t) = 0;
    
    //! Satisfies one dependency of thread `t`.
    virtual void decr_dependencies(thread_p t) = 0;
    
    /*! Captures the outstrategy of the current thread.
     *  \return the outstrategy of the current thread
     *  \post The field `out` of the current thread is replaced
     *  with a new `outstrategy::noop`.
     */
    virtual outstrategy_p capture_outstrategy() = 0;
    
    /*! \brief Ensures that the scheduler does not deallocate the
     *  calling thread.
     */
    virtual void reuse_calling_thread() = 0;
    
    //! Returns a pointer to the current thread
    virtual thread_p get_current_thread() const = 0;
    
    virtual void communicate() { }
    
    virtual bool& should_communicate_flag() = 0;
    
    virtual bool should_call_communicate() = 0;
    
    virtual size_t nb_threads() = 0;

    virtual void reject() { }
    virtual void unblock() { }

    ///@}
    
  };
  
  extern util::worker::controller_factory_t* the_factory;
  
  typedef signature scheduler_t;
  typedef scheduler_t* scheduler_p;
  
  //! Returns a pointer to the scheduler of the calling worker
  static inline scheduler_p get_mine () {
    worker_id_t my_id = util::worker::get_my_id();
    signature::controller_t* sched = util::worker::the_group.get_controller(my_id);
    assert(sched != nullptr);
    return static_cast<scheduler_p>(sched);
  }
  
} // end namespace

/*! @} */

typedef scheduler::scheduler_p scheduler_p;

} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_SCHED_CLASSES_H_ */
