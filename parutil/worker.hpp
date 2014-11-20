/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file worker.hpp
 * \brief A worker is an OS thread that hosts a task scheduler. 
 * 
 */

#ifndef _PASL_UTIL_WORKER_H_
#define _PASL_UTIL_WORKER_H_

#include <assert.h>
#include <deque>
#include <signal.h>
#include <cstdlib>
#ifdef USE_CILK_RUNTIME
#include <cilk/cilk_api.h>
#endif

#include "atomic.hpp"
#include "tls.hpp"
#include "aliases.hpp"
#include "machine.hpp"

namespace pasl {
namespace util {
namespace worker {

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Worker ID */

tls_extern_declare(worker_id_t, worker_id);

//! Returns the id of calling worker.
static inline worker_id_t get_my_id () {
#ifdef USE_CILK_RUNTIME
  return __cilkrts_get_worker_number();
#else
  return tls_getter(worker_id_t, worker_id);
#endif
}

//! A special worker id code returned when threads don't exist yet 
const worker_id_t undef = -1l;
  
/*---------------------------------------------------------------------*/

/*! \class periodic_t
 *  \brief An object whose state needs to be updated regularly.
 */
class periodic_t {
public:
  //! Updates the object state
  virtual void check() = 0;
};

typedef periodic_t* periodic_p;

/*---------------------------------------------------------------------*/
/* Worker controller */
  
/*! \brief Minimum time between any communication events on a given
 * worker (microseconds).
 */ 
extern double delta;
  
/*! \class controller_t
 *  \brief An interface between an OS thread and a PASL scheduler.
 *
 * Periodic checks
 * =========================
 *
 * Each controller maintains its own set of periodic-check objects. 
 * The checks are typically used to deliver messages, detect termination 
 * of a set of tasks, etc. As such, in order to ensure progress of the 
 * program, subclass of this signature must regularly run the checks. A
 * subclass can run all the checks by calling `check_periodic()`.
 */
class controller_t {
protected:
  int nb_workers;
  worker_id_t my_id;
  
public:

  /** @name Initialization and teardown */
  ///@{
  virtual void init();
  virtual void destroy();
  ///@}

  /** @name Busy worker */
  ///@{
  //! Executes the main worker loop
  virtual void run() = 0;
  ///@}

  /** @name Idling behavior */
  ///@{

  //! Notification on new launch
  virtual void new_launch() { }
  //! Handler for when a worker starts waiting
  virtual void enter_wait();
  //! Handler for when a worker stops waiting
  virtual void exit_wait();
  //! Handler for when a worker goes idle
  virtual void yield();
  ///@}
  
  worker_id_t get_id() {
    return my_id;
  }

  /** @name Per-worker random number generation */
  ///@{
protected:
  unsigned rand_seed;
public:
  void mysrand(unsigned seed);
  unsigned myrand();

  /*! \brief Returns an id chosen uniformly at random from the set of
   * worker ids, excluding the id of this worker. Return result is
   * undefined if `nb_workers == 1`.
   */
  worker_id_t random_other();
  ///@}
  
  /** @name Periodic checks */
  ///@{
protected:
  ticks_t last_check_periodic;
  typedef std::deque<periodic_p> periodic_set_t;
  periodic_set_t periodic_set;  
  
public:  
  /*! \brief Adds to the set of periodic checks the check `p`. */
  void add_periodic(periodic_p p);
  /*! \brief Removes from the set of periodic checks the check `p`. */
  void rem_periodic(periodic_p p);
  /*! \brief Runs all the checks in the set of periodic checks.
   *  \warning May be called asynchronously by the worker's signal
   *   handler.
   */
  void check_periodic();
  ///@}

  /** @name Asynchronous interrupts */
  ///@{
protected:
  struct sigaction sa;
  bool allow_interrupt;
  ticks_t date_of_last_interrupt;
  bool interrupt_was_blocked;
  void interrupt_init();
  static void dummy_sighandler(int sig, siginfo_t *si, void *uc);
  static void controller_sighandler(int sig, siginfo_t *si, void *uc);

public:
  static void* ping_loop(void* arg);
  virtual void check_on_interrupt() = 0;
  virtual bool should_be_interrupted() { return false; }
  void interrupt_handled();
  ///@}

  controller_t() : my_id(undef), allow_interrupt(false) { 
    last_check_periodic = ticks::now();
  }
  
  friend class group_t;
};
  
typedef controller_t* controller_p;

/*! \class controller_factory_t
 *  \brief Allocates a set of controllers.
 */  
class controller_factory_t {
public:
  virtual ~controller_factory_t() { }
  virtual void create_shared_state() = 0;
  virtual void delete_shared_state() = 0;
  virtual controller_p create_controller() = 0;
  virtual void destroy_controller(controller_p c) = 0;
};
  
typedef controller_factory_t* controller_factory_p;

/*---------------------------------------------------------------------*/
/* Worker group */
  
//! To represent the status of the worker group.
typedef enum { NOT_INIT, PASSIVE, ACTIVE } status_t;

/*! \class group_t
 *  \brief To represent a group of workers.
 */
class group_t {  
protected:
  machine::binding_policy_p        bindpolicy;
  int                     nb_workers;
  pthread_t*              pthreads;       // one pthread per worker
  status_t                state;
  bool                    all_workers_should_exit;
  bool                    worker0_should_exit;
  bool                    worker0_running;
  controller_factory_p    factory;
  controller_p*           controllers;    // one controller per worker
  barrier_t               creation_barrier;
  barrier_t               destruction_barrier;

  static void* build_thread (void* arg);

public:
  group_t();
  /*! \brief Initializes a worker group with `nb_workers` workers and
   *  binding policy `bindpolicy`.
   */
  void init(int nb_workers, machine::binding_policy_p bindpolicy);
  void set_factory(controller_factory_p factory);
  bool is_active();
  void set_nb(int nb) {
    nb_workers = nb;
  }
  //! Returns the number of workers in the group
  int get_nb() const;
  //! Returns the id of the calling worker
  worker_id_t get_my_id();
  /*! \brief Returns the id of the calling worker, if the group state
   *  is not `PASSIVE`; otherwise, returns the id of the calling
   *  worker. */
  worker_id_t get_my_id_or_undef();
  //! Returns true if the calling worker needs to terminate
  bool exit_controller();
  //! Starts running the controller of `worker[0]`
  void run_worker0();
  //! Initiates the process of terminating a group
  void request_exit_worker0();
  //! Spawns one OS thread per worker
  void create_threads();
  //! Deallocates memory held by the group
  void destroy_threads();
  controller_p get_controller(worker_id_t id);
  controller_p get_controller0();
  machine::binding_policy_p get_bindpolicy();
  void check_worker_id(worker_id_t id) const {
    assert((id >= 0) && (id < nb_workers));
  }
  template <class Body>
  void for_each_worker(const Body& body) const {
    assert(get_nb() > 0);
    for (worker_id_t i = 0; i < get_nb(); i++)
      body(i);
  }

  /** @name Interrupts */
  ///@{
protected:
  bool                    ping_loop_should_exit;
  pthread_t               ping_loop_thread;
  bool*                   ping_received;
  ticks_t*                last_ping_date;

  void ping_loop_create();
  void ping_loop_destroy();
public:
  void send_interrupt(worker_id_t id);
  ///@}

  friend class controller_t;  
};

/*---------------------------------------------------------------------*/
/* Aliases */

typedef group_t* group_p;

//! The work group for the entire program.
extern group_t the_group;

//! Returns the id of the calling worker
static inline int get_nb() {
  return the_group.get_nb();
}
  
/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_UTIL_WORKER_H_ */


