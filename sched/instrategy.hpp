/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file instrategy.hpp
 * \brief Instrategies
 *
 */

#ifndef _INSTRATEGY_H_
#define _INSTRATEGY_H_

#include "thread.hpp"
#include "tagged.hpp"
#include "messagestrategy.hpp"

namespace pasl {
namespace sched {
namespace instrategy {

/***********************************************************************/
/**
 * \ingroup thread 
 * \defgroup instrategy Instrategies
 * @{
 * A mechanism for detecting when threads become ready.
 * @}
 */
  
/*---------------------------------------------------------------------*/

/*! \class signature
 *  \brief A mechanism for detecting when a thread becomes ready.
 *
 * The instrategy of a given thread `t` maintains a \a join \a counter, 
 * which determines both the number of `t`'s dependencies and how many
 * are satisfied.
 *
 * \ingroup instrategy
 */
class signature {
public:
  
  virtual ~signature() { }

  //! Initializes the instrategy
  virtual void init(thread_p t) = 0;

  //! Marks the thread ready
  virtual void start(thread_p t) = 0;

  /*! \brief Checks whether all dependencies are satisfied and, if so,
   *  schedules the thread
   */
  virtual void check(thread_p t) = 0;

  //! Adds to the join counter the integer `d`
  virtual void delta(thread_p t, int64_t d) = 0;

  //! Same as `delta`, but called only by the message handler
  virtual void msg_delta(thread_p t, int64_t d) = 0;

};
  
/*---------------------------------------------------------------------*/

static inline void schedule(thread_p t) {
  scheduler::get_mine()->schedule(t);
}

/*---------------------------------------------------------------------*/

/*! \class common
 *  \brief Implements a few methods of the instrategy
 *
 * \ingroup instrategy
 */
class common : public signature {
public: 
  
  virtual void init(thread_p t) { 
    check(t); 
  }

  virtual void start(thread_p t) {
    schedule(t);
    delete this;
  }

  virtual void msg_delta(thread_p t, int64_t d) {
    assert(false);
  }
};

/*---------------------------------------------------------------------*/

/*! \class ready
 *  \brief For a thread with no dependencies.
 * \ingroup instrategy
 */
class ready : public common {
public:

  void check(thread_p t) {
    start(t);
  } 
  
  void delta(thread_p t, int64_t d) {
    assert(false);
  }
};

/*---------------------------------------------------------------------*/

/*! \class unary
 *  \brief For a thread with one dependency.
 * \ingroup instrategy
 */
class unary : public common {
public:

  void check(thread_p t) {
    return;
  }

  void delta(thread_p t, int64_t d) {
    if (d == -1)
      start(t);
    else
      assert(d == 1);
  }
};


/*---------------------------------------------------------------------*/

/*! \class fetch_add
 *  \brief Updates the join counter using atomic fetch and add.
 * \ingroup instrategy
 */
class fetch_add : public common {
protected:
  std::atomic<int64_t> counter;

public:

  fetch_add() : counter(0) { }
  
  void check(thread_p t) {
    if (counter == 0l)
      start(t);
  }

  void delta(thread_p t, int64_t d) {
    int64_t old = counter.fetch_add(d);
    if ((old+d) == 0l)
      start(t);
  }
};

/*---------------------------------------------------------------------*/

/*! \class message
 *  \brief Updates the join counter using message passing.
 * \ingroup instrategy
 */
class message : public common {
protected:
  worker_id_t master;
  int64_t counter;

public:

  message() : counter(0) { }

  void init (thread_p t) {
    master = util::worker::get_my_id();
    common::init(t);
  }
  
  void check(thread_p t) {
    if (counter == 0l)
      start(t);
  }

  void master_delta(thread_p t, int64_t d) { 
    // TEMPORARY:
    //    printf("counter=%lld %lld\n",counter, d);
    assert (d >= 0 || counter > 0);
    counter += d;
    check(t);
  }

  void delta(thread_p t, int64_t d) {
    int64_t my_id = util::worker::get_my_id();
    if (my_id == master) 
      master_delta(t, d);
    else 
      messagestrategy::send(master, messagestrategy::in_delta(this, t, d));
  }

  void msg_delta(thread_p t, int64_t d) {
    master_delta(t, d);
  }

};
  
/*---------------------------------------------------------------------*/
  
/*! \class distributed
 *  \brief Each processor maintains a counter storing the difference
 *   between the number of threads created and the number of threads
 *   executed, and one of the processor checks on a regular basis
 *   whether the sum of all these counters is zero; when it is the
 *   case, the thread has terminated.
 *
 * \warning this implementation is x86 specific because our concurrent
 * counters rely on TSO
 *
 * \ingroup instrategy
 */
class distributed : public common, public util::worker::periodic_t {
protected:
  
  data::perworker::counter::carray<int64_t> counter;
  
  thread_p t;
  
public:
  
  /*! \param t pointer to the thread to schedule when the counter
   * equals zero */
  distributed(thread_p t)
  : t(t) {
    counter.init(0l);
  }
  
  void init(thread_p t) {
    this->t = t;
    common::init(t);
    scheduler::get_mine()->add_periodic(this);
  }
  
  void start(thread_p t) {
    assert(t == this->t);
    common::start(t);
  }
  
  void check(thread_p t) {
    assert(t == this->t);
    if (counter.sum() == 0) {
      scheduler::get_mine()->rem_periodic(this);
      start(t);
    }
  }
  
  void check() {
    check(t);
  }
  
  void delta(thread_p t, int64_t d) {
    assert(t == this->t);
    worker_id_t my_id = util::worker::get_my_id();
    counter.delta(my_id, d);
  }
  
  int64_t get_diff() {
    return counter.sum();
  }
  
};
  
/*---------------------------------------------------------------------*/
 
const long READY_TAG = 1;
const long UNARY_TAG = 2;
const long FETCH_ADD_TAG = 3;
  
static inline long extract_tag(instrategy_p in) {
  return data::tagged::extract_tag<thread_p, instrategy_p>(in);
}
  
static inline instrategy_p ready_new() {
#ifndef DEBUG_OPTIM_STRATEGY
  return data::tagged::create<thread_p, instrategy_p>(NULL, READY_TAG);
#else
  return new ready();
#endif
}

static inline instrategy_p unary_new() {
#ifndef DEBUG_OPTIM_STRATEGY
  return data::tagged::create<thread_p, instrategy_p>(NULL, UNARY_TAG);
#else
  return new unary();
#endif
}

static inline instrategy_p fetch_add_new() {
#ifndef DEBUG_OPTIM_STRATEGY
  return data::tagged::create<int64_t, instrategy_p>(0l, FETCH_ADD_TAG);
#else
  return new fetch_add();
#endif
}

/*---------------------------------------------------------------------*/
  

static inline void check(instrategy_p in, thread_p t) {
  if (extract_tag(in) == READY_TAG)
    schedule(t);
  else if (extract_tag(in) == UNARY_TAG)
    return;
  else if (extract_tag(in) == FETCH_ADD_TAG) {
    if (data::tagged::extract_value<int64_t, instrategy_p>(in) == 0l)
      schedule(t);
  } else
    in->check(t);
}
  
static inline void init(instrategy_p in, thread_p t) {
  if (extract_tag(in) == READY_TAG)
    schedule(t);
  else if (extract_tag(in) == UNARY_TAG)
    return;
  else if (extract_tag(in) == FETCH_ADD_TAG) 
    check(in, t);
  else
    in->init(t);
}

static inline void delta(instrategy_p& in, thread_p t, int64_t d) {
  if (extract_tag(in) == READY_TAG)
    util::atomic::die("instrategy::delta: bogus tag READY_TAG");
  else if (extract_tag(in) == UNARY_TAG) {
    if (d == -1)
      schedule(t);
    else
      assert(d == 1);
  } else if (extract_tag(in) == FETCH_ADD_TAG) {
    int64_t old = data::tagged::atomic_fetch_and_add<instrategy_p>(&in, d);
    if ((old+d) == 0l)
      schedule(t);
  } else {
    in->delta(t, d);
  }
}

static inline void msg_delta(instrategy_p in, thread_p t, int64_t d) {
  assert(extract_tag(in) == 0);
  in->msg_delta(t, d);
}

/*! \brief Deallocates instrategy `in`
 *  \warning The scheduler deallocates instrategies automatically; as
 *  such, this routine should be used only in the case where the given
 *  instrategy is not passed to the scheduler.
 */
static inline void destroy(instrategy_p in) {
  if (extract_tag(in) == 0)
    delete in;
}
  
/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif // _INSTRATEGY_H_
