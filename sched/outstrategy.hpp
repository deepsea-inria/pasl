/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file outstrategy.hpp
 * \brief Outstrategies
 *
 */

#ifndef _OUTSTRATEGY_H_
#define _OUTSTRATEGY_H_

#include <list>
#include <atomic>

#include "classes.hpp"
#include "thread.hpp"
#include "messagestrategy.hpp"
#include "tagged.hpp"

namespace portpassing {
void portpassing_finished(pasl::sched::thread_p);
}

namespace direct {
namespace statreeopt {
void unary_finished(pasl::sched::thread_p t);
}}

namespace direct {
namespace growabletree {
  void unary_finished(pasl::sched::thread_p t);
}}

namespace pasl {
namespace sched {
namespace outstrategy {

/***********************************************************************/

/**
 * \ingroup thread
 * \defgroup outstrategy Outstrategies
 * @{
 * An outstrategy is a representation of the continuation of a
 * thread.
 *
 * Basic outstrategies
 * ====================
 *
  outstrategy  | meaning
  -------------|-------------------
  `noop`       | no outgoing edges
  `unary`      | one outgoing edge
  `single`     | one outgoing edge, and moreover this edge is the only incoming in the join thread
 *
 *
 * @}
 */

/*! \class signature
 *  \brief Represents the continuation of a thread.
 *
 * The outstrategy of a thread `t` is an object that maintains the list
 * of edges that leave `t` and a state indicating whether `t` is
 * finished executing. When `t` is finished, all of its out-going
 * edges must be satisfied. An edge from `t` to `td` is satisfied by
 * decrementing the dependencies of `td` (i.e., calling
 * `decr_dependencies(td)`).
 *
 * \ingroup outstrategy
 */
class signature {
public:

  //! Adds the given thread `td` to the list of out-going edges
  virtual void add(thread_p td) = 0;

  //! Same as `add`, but called only from the message handler
  virtual void msg_add(thread_p td) = 0;

  //! Puts the object into the finished state
  virtual void finished() = 0;

  //! Same as `finished()`, but called only by the message handler
  virtual void msg_finished() = 0;

  typedef std::vector<thread_p> edgelist_t;

  //! Copies the list of edges into `vec`
  virtual void copy_edgelist(edgelist_t& vec) { }

#ifdef CHECKINV
  // used for debugging to test if an out-strategy is "end"
  virtual bool is_end() { return false;}
#endif
};


/*---------------------------------------------------------------------*/

static inline void decr_dependencies(thread_p t) {
  scheduler::get_mine()->decr_dependencies(t);
}

/*---------------------------------------------------------------------*/
/* Outstrategies */

/*! \class common
 *  \brief Outstrategy which supports zero or one out edges.
 *  \ingroup outstrategy
 */
class common : public signature {
public:
  
  virtual ~common() { }

  virtual void finished() {
    delete this;
  }
  
  virtual void msg_add(thread_p td) {
    assert(false);
  }
  
  virtual void msg_finished() {
    assert(false);
  }
};

/*---------------------------------------------------------------------*/

/*! \class noop
 *  \brief Outstrategy which does nothing.
 * \ingroup outstrategy
 */
class noop : public common {
public: 

  virtual void add (thread_p td) {
    assert (false);
  }
};


/*---------------------------------------------------------------------*/

/*! \class end
 *  \brief Outstrategy which terminates the entire computation.
 * \ingroup outstrategy
 */
class end : public noop {
public: 

  virtual void finished () {
    STAT_IDLE(finished_launch());
    util::worker::the_group.request_exit_worker0();
    noop::finished();
  }

#ifdef CHECKINV
  virtual bool is_end() { return true;}
#endif
};


/*---------------------------------------------------------------------*/

/*! \class unary
 *  \brief Outstrategy which decrements the unsatisfied dependencies of
 *  a single thread.
 * \todo add one function: get_target
 * \ingroup outstrategy
 */
class unary : public common {
protected:
  thread_p tone;

public:

  unary() : tone(NULL) { }
  
  void add(thread_p td) {
    assert (tone == NULL);
    tone = td;
  }

  void finished() {
    assert (tone != NULL);
    decr_dependencies(tone);
    tone = NULL; // optional
    common::finished();
  }
    
  void copy_edgelist(edgelist_t& vec) {
    if (tone != NULL)
      vec.push_back(tone);
  }

};
  
/*---------------------------------------------------------------------*/

/*! \class message
 *  \brief An outstrategy that supports concurrent calls to `add` and
 *  \a finish.
 *
 * Communication is achieved by message passing.
 *
 * \ingroup outstrategy
 */
class message : public common {
protected:
  //! Sole worker that can add edges and mark the thread finished.
  worker_id_t master;
  //! List of edges
  edgelist_t ts;

public:
  message() : common(), master(util::worker::get_my_id()) { }

  virtual void add (thread_p td) {
    if (util::worker::get_my_id() == master)
      msg_add(td);
    else
      messagestrategy::send(master, messagestrategy::out_add(this, td));
  }
  
  virtual void msg_add(thread_p td) {
    assert(util::worker::get_my_id() == master);
    ts.push_back(td);
  }
  
  virtual void finished() {
    if (util::worker::get_my_id() == master)
      msg_finished();
    else
      messagestrategy::send(master, messagestrategy::out_finished(this));
  }

  virtual void msg_finished() {
    satisfy_dependencies();
    common::finished();
  }
  
  virtual void satisfy_dependencies() {
    for (edgelist_t::iterator it = ts.begin(); it != ts.end(); it++)
      decr_dependencies(*it);
    ts.clear();
  }

  virtual void copy_edgelist(edgelist_t& vec) {
    for (edgelist_t::iterator it = ts.begin(); it != ts.end(); it++)
      vec.push_back(*it);
  }
  
  worker_id_t get_master() {
    return master;
  }

};

/*---------------------------------------------------------------------*/  
  
/*! \class future
 *  \brief The outstrategy of a future.
 *
 * \ingroup outstrategy future
 */
class future : public common {
protected:
  bool lazy;
public:
  future(bool lazy) : lazy(lazy) { }
  virtual bool thread_finished() = 0;
};
  
/*---------------------------------------------------------------------*/

class one_to_one_future : public common {
protected:
  
  std::atomic<thread_p> state;
  
  static constexpr thread_p init = nullptr;
  const thread_p ready = init + 1;
  
public:
  
  one_to_one_future()
  : state(init) { }
  
  virtual void add(thread_p t) {
    thread_p old = init;
    if (state.compare_exchange_strong(old, t)) {
      return;
    }
    assert(old == ready);
    finished();
  }
  
  virtual void finished() {
    thread_p old = init;
    if (! state.compare_exchange_strong(old, ready)) {
      bool b = state.compare_exchange_strong(old, ready);
      assert(b);
      (void) b; // avoid unused variable warning (used by the assert)
      decr_dependencies(old);
    }
    common::finished();
  }
  
};
  
/*---------------------------------------------------------------------*/

/*! \class future_message
 *  \brief An instance of the outstrategy `future` that is based on 
 *  message passing.
 * \ingroup outstrategy future
 */
class future_message : public future {
protected:
  worker_id_t master;
  message out;

public:
  //! Indicates whether the body of the future must be executed
  bool requested;
  //! Indicates whether the body of the future completed 
  bool completed;
  
  future_message(bool lazy) 
    : future(lazy), requested(!lazy), completed(false) { 
    master = out.get_master();
  }
  
  virtual void add(thread_p td) {
    if (util::worker::get_my_id() == master)
      msg_add(td);
    else
      messagestrategy::send(master, messagestrategy::out_add(this, td));
  }
  
  virtual void msg_add(thread_p td) {
    assert(util::worker::get_my_id() == master);
    if (! requested) {
      requested = true;
      decr_dependencies(td);
    } 
    if (completed)
      decr_dependencies(td);
    else
      out.msg_add(td);
  }

  virtual void finished() {
    if (util::worker::get_my_id() == master)
      msg_finished();
    else
      messagestrategy::send(master, messagestrategy::out_finished(this));
  }

  virtual void msg_finished() {
    assert(util::worker::get_my_id() == master);
    completed = true;
    out.satisfy_dependencies();
  }

  virtual void copy_edgelist(edgelist_t& vec) {
    out.copy_edgelist(vec);
  }
  
  virtual bool thread_finished() {
    return completed;
  }
};
  
/*---------------------------------------------------------------------*/
  
const long NOOP_TAG = 1;
const long UNARY_TAG = 2;
const long PORTPASSING_UNARY_TAG = 3;
const long DIRECT_STATREEOPT_UNARY_TAG = 4;
const long DIRECT_GROWABLETREE_UNARY_TAG = 4;
  
static inline long extract_tag(outstrategy_p out) {
  return data::tagged::extract_tag<thread_p, outstrategy_p>(out);
}

static inline outstrategy_p unary_new() {
#ifndef DEBUG_OPTIM_STRATEGY
  return data::tagged::create<thread_p, outstrategy_p>(NULL, UNARY_TAG);
#else
  return new unary();
#endif
}

static inline outstrategy_p noop_new() {
#ifndef DEBUG_OPTIM_STRATEGY
  return data::tagged::create<thread_p, outstrategy_p>(NULL, NOOP_TAG);
#else
  return new noop();
#endif
}
  
static inline outstrategy_p portpassing_unary_new(thread_p t) {
#ifndef DEBUG_OPTIM_STRATEGY
  return data::tagged::create<thread_p, outstrategy_p>(t, PORTPASSING_UNARY_TAG);
#else
  assert(false);
  return nullptr;
#endif
}
  
static inline outstrategy_p direct_statreeopt_unary_new(thread_p t) {
#ifndef DEBUG_OPTIM_STRATEGY
  return data::tagged::create<thread_p, outstrategy_p>(t, DIRECT_STATREEOPT_UNARY_TAG);
#else
  assert(false);
  return nullptr;
#endif
}
  
static inline outstrategy_p direct_growabletree_unary_new(thread_p t) {
#ifndef DEBUG_OPTIM_STRATEGY
  return data::tagged::create<thread_p, outstrategy_p>(t, DIRECT_GROWABLETREE_UNARY_TAG);
#else
  assert(false);
  return nullptr;
#endif
}
  
/*---------------------------------------------------------------------*/

static inline void add(outstrategy_p& out, thread_p td) {
  long tag = extract_tag(out);
  assert(tag != PORTPASSING_UNARY_TAG);
  assert(tag != DIRECT_STATREEOPT_UNARY_TAG);
  assert(tag != DIRECT_GROWABLETREE_UNARY_TAG);
  if (tag > 0) {
    assert(   tag == UNARY_TAG);
    out = data::tagged::create<thread_p, outstrategy_p>(td, tag);
  } else {
    out->add(td);
  }
}

static inline void msg_add(outstrategy_p out, thread_p td) {
  assert(extract_tag(out) == 0);
  out->msg_add(td);
}

static inline void finished(thread_p t, outstrategy_p out) {
  long tag = extract_tag(out);
  if (tag > 0) {
    if (tag == NOOP_TAG)
      return;
    thread_p tjoin = data::tagged::extract_value<thread_p, outstrategy_p>(out);
    if (tag == UNARY_TAG)
      decr_dependencies(tjoin);
    else if (tag == PORTPASSING_UNARY_TAG)
      portpassing::portpassing_finished(tjoin);
    else if (tag == DIRECT_STATREEOPT_UNARY_TAG)
      direct::statreeopt::unary_finished(tjoin);
    else if (tag == DIRECT_GROWABLETREE_UNARY_TAG)
      direct::growabletree::unary_finished(tjoin);
    else
      util::atomic::die("bogus tag (finished)");
  } else {
    out->finished();
  }
}

static inline void msg_finished(outstrategy_p out) {
  assert(extract_tag(out) == 0);
  out->msg_finished();
}

static inline void copy_edgelist(outstrategy_p out, signature::edgelist_t& vec) {
  long tag = extract_tag(out);
  if (tag > 0) {
    if (tag == NOOP_TAG)
      return;
    else if (tag == UNARY_TAG) {
      thread_p t = data::tagged::extract_value<thread_p, outstrategy_p>(out);
      if (t != NULL)
        vec.push_back(t);
    } else
      util::atomic::die("bogus tag (copy_edgelist)");
  } else {
    out->copy_edgelist(vec);
  }
}
  
#ifdef CHECKINV
static inline bool is_end(outstrategy_p out) {
  long tag = extract_tag(out);
  if (tag > 0) return false;
  return out->is_end();
}
#endif
  
/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif // _OUTSTRATEGY_H_
