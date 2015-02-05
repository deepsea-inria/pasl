/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file workstealing.hpp
 *
 */

#ifndef _WORKSTEALING_H_
#define _WORKSTEALING_H_

#include <math.h>

#include "classes.hpp"
#include "container.hpp"
#include "scheduler.hpp"

/*! \defgroup workstealing Work stealing
 *  \ingroup scheduler
 * @{
 */
namespace pasl {
namespace sched {
namespace workstealing {

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Forward declarations and aliases */

class threadset_shared;
class threadset_private;

typedef threadset_private* threadset_private_p;
typedef threadset_shared* threadset_shared_p;

/*---------------------------------------------------------------------*/

// LATER: find a better name instead of threadset

class threadset_shared : public scheduler::_shared {
protected:
  /*! \brief The maximum number of times in a communicate phase that
   *  one worker tries to migrate a thread to a remote worker.
   */
  int nb_tries_per_communicate;

public:
  threadset_shared();
  ~threadset_shared();
};

/*---------------------------------------------------------------------*/

class threadset_private : public scheduler::_private {
protected:
  bool stay_in_acquire();

  virtual void add_to_pool_of_ready_threads(thread_p t) {
    local_push(t);
  }

public:
  virtual void acquire() = 0;
  virtual void communicate() = 0;
  virtual void wait() = 0;

  virtual bool   local_has() = 0;
  virtual void   local_push(thread_p thread) = 0;
  virtual thread_p local_pop() = 0;
  virtual thread_p local_peek() = 0;
  virtual bool   remote_has() = 0;
  virtual void   remote_push(thread_p thread) = 0;
  virtual thread_p remote_pop() = 0;

  virtual size_t nb_threads() = 0;
};

/*---------------------------------------------------------------------*/
/* Worker with a private deque */

class private_deque : public threadset_private {
protected:
  data::stl::deque_seq<thread_p> my_ready_threads;

/*
  void check_for_duplicates() {
#ifndef NDEBUG
    for (int i = 0; i < my_ready_threads.size(); i++) {
      thread_p t = my_ready_threads[i];
      int cnt = 0;
      for (int j = 0; j < my_ready_threads.size(); j++) {
        if (my_ready_threads[j] == t)
          cnt++;
      }
      assert(cnt == 1);
    }
#endif
  }
  */

public:
  inline size_t nb_threads() {
    return my_ready_threads.size();
  }

  inline bool local_has() {
    return nb_threads() > 0;
  }

  inline virtual void local_push(thread_p thread) {
    my_ready_threads.push_back(thread);
  }

  inline virtual thread_p local_pop() {
    thread_p t = my_ready_threads.pop_back();
    LOG_THREAD(THREAD_POP, t);
    return t;
  }

  inline virtual thread_p local_peek() {
    thread_p t = my_ready_threads.back();
    return t;
  }

  template <class Func>
  void for_each_in_deque(const Func& f) {
    for (auto it = my_ready_threads.begin(); it != my_ready_threads.end(); it++) {
      f(*it);
    }
  }

/*
  void print(std::string msg) {
    util::atomic::acquire_print_lock();
    printf("%s; deque of %lld:\t",msg.c_str(),my_id);
    for_each_in_deque([&] (thread_p t) { printf("%p, "); });
    printf("\n");
    util::atomic::release_print_lock();
  }
 */

  // equivalent to testing whether the worker was interrupted
  bool is_one_thread_running() {
    return current_thread != NULL;
  }

  inline bool remote_can_split() {
    if (nb_threads() < 1)
      return false;
    thread_p thread = my_ready_threads.front();
    bool b = thread->size() > 1;
    //! \todo this condition is overly conservative because it fails in the case where we're just rescheduling ourselves
    // if (b && is_one_thread_running())
    //  atomic::die("assumption violated regarding splittable threads\n");
    return b;
  }

  inline bool remote_has() {
    return remote_can_split() || nb_threads() > 1;
  }

  inline void remote_push(thread_p thread) {
    my_ready_threads.push_front(thread);
  }

  inline thread_p remote_peek() {
    if (remote_can_split())
      assert(false);
    else
      return my_ready_threads.front();
  }

  inline thread_p remote_pop() {
    if (remote_can_split()) {
      STAT_COUNT(THREAD_SPLIT);
      thread_p t = my_ready_threads.front();
      size_t sz = t->size();
      assert(sz > 1);
      return t->split(sz / 2);
    } else {
      assert(remote_has());
      return my_ready_threads.pop_front();
    }
  }

  inline thread_p try_local_pop() {
    if (local_has())
      return local_pop();
    else
      return NULL;
  }

  virtual void check() = 0;

  virtual void add_to_pool_of_ready_threads(thread_p t) {
    local_push(t);
  }

};

/*---------------------------------------------------------------------*/
/* Alarm interface */
/* to be used by sender initiated to schedule calls to communicate() */

class alarm {
protected:
  util::worker::controller_p controller;

public:
  virtual ~alarm() { }
  virtual void init(util::worker::controller_p controller) = 0;
  virtual bool ready() = 0;
  virtual void reset() = 0;
};

alarm* create_alarm();

/*---------------------------------------------------------------------*/
/* CAS-based sender-initiated work stealing */

class cas_si_private;

class cas_si_shared : public threadset_shared {
protected:
  typedef thread_p state_t;
  data::perworker::array<std::atomic<state_t>> states;

public:
  cas_si_shared();
  ~cas_si_shared();

friend class cas_si_private;
};

class cas_si_private : public private_deque {
protected:
  cas_si_shared* shared;
  alarm* _alarm;

  void cancel_acquire();
  void check();

public:
  cas_si_private(cas_si_shared* shared) : shared(shared) {}
  void init();
  void destroy();
  void run();
  void acquire();
  void communicate();
  void wait();
  void check_on_interrupt();
  bool should_call_communicate();
};

/*---------------------------------------------------------------------*/
/* CAS-based receiver-initiated work stealing */

class cas_ri_interrupt_private;

class cas_ri_private;

typedef thread_p answer_t;
typedef worker_id_t request_t;
static const answer_t ANSWER_REJECT = (answer_t) NULL;
static const answer_t ANSWER_WAITING = (answer_t) 1;
static const request_t REQUEST_WAITING = -1;
static const request_t REQUEST_BLOCKED = -2;


class cas_ri_shared : public threadset_shared {
protected:
  data::perworker::array<answer_t> answers;
  data::perworker::array<std::atomic<request_t>> requests;

public:
  cas_ri_shared();
  ~cas_ri_shared();

friend class cas_ri_private;
friend class cas_ri_interrupt_private;
};

class cas_ri_private : public private_deque {
protected:
  cas_ri_shared* shared;
  ticks_t last_communicate;
  void check();
  void sleep_in_acquire(double nb_microseconds);
  bool time_to_communicate();
  std::atomic<request_t>* my_request_ptr;

public:
  cas_ri_private(cas_ri_shared* shared) : shared(shared) {}
  void init();
  void destroy();
  void run();
  void reject();
  void acquire();
  void communicate();
  void wait();
  void check_on_interrupt();
  bool should_call_communicate();
  void unblock();
};



/*---------------------------------------------------------------------*/
/* CAS-based receiver-initiated work stealing with interrupts */

//class cas_ri_interrupt_private;


class cas_ri_interrupt_shared : public cas_ri_shared {
protected:

public:
  cas_ri_interrupt_shared() : cas_ri_shared() {}
  ~cas_ri_interrupt_shared() {}

friend class cas_ri_interrupt_private;
};

class cas_ri_interrupt_private : public cas_ri_private  {
protected:

public:
  cas_ri_interrupt_private(cas_ri_interrupt_shared* shared) : cas_ri_private(shared) {}

  void check_on_interrupt();
  bool should_be_interrupted() { return shared->requests[my_id] != REQUEST_WAITING; }
  void check();
  void acquire();

};

/*---------------------------------------------------------------------*/
/* Work stealing with shared deques */

class shared_deques_private;

class chase_lev_deque {
protected:

  typedef std::atomic<thread_p>* buffer_t;

  volatile buffer_t  buf;         // deque contents
  std::atomic<int64_t> capacity;  // maximum number of elements
  std::atomic<int64_t> bottom;    // index of the first unused cell
  std::atomic<int64_t> top;       // index of the last used cell

  static thread_p cb_get (buffer_t buf, int64_t capacity, int64_t i);
  static void cb_put (buffer_t buf, int64_t capacity, int64_t i, thread_p x);
  buffer_t new_buffer(size_t capacity);
  void delete_buffer(buffer_t buf);
  buffer_t grow (buffer_t old_buf,
                 int64_t old_capacity,
                 int64_t new_capacity,
                 int64_t b,
                 int64_t t);
  bool cas_top (int64_t old_val, int64_t new_val);

public:
  chase_lev_deque() : buf(NULL), bottom(0l), top(0l) {
    capacity.store(0l);
  }
  void init(int64_t init_capacity);
  void destroy();
  void push_back(thread_p item);
  thread_p pop_front();
  thread_p pop_back();
  size_t nb_threads();
  bool empty();
  friend class shared_deques_shared;
};

class shared_deques_shared : public scheduler::_shared {
protected:
  data::perworker::array<chase_lev_deque*> deques;
  barrier_t creation_barrier;

public:
  shared_deques_shared();
  ~shared_deques_shared();
  friend class shared_deques_private;
};

class shared_deques_private : public scheduler::_private {
protected:
  shared_deques_shared* _shared;
  chase_lev_deque my_deque;
  std::vector<thread_p> my_fresh;
  bool initialized;

  void flush();

public:
  shared_deques_private(shared_deques_shared* _shared)
    : _shared(_shared),initialized(false) { }
  void init();
  void destroy();
  void run();
  void acquire();
  void check();
  void check_on_interrupt();
  void add_to_pool_of_ready_threads(thread_p thread);

};

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

/*! @} */

#endif
