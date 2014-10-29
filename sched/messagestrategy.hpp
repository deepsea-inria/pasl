/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file messagestrategy.hpp
 * \brief A mechanism for transfering messages point-to-point between
 * the workers.
 */

#ifndef _MESSAGESTRATEGY_H_
#define _MESSAGESTRATEGY_H_

#include <algorithm>
#include <queue>

#include "worker.hpp"
#include "classes.hpp"
#include "pcb.hpp"
#include "perworker.hpp"

/**
 * \defgroup messagestrategy Inter-worker message-passing communication
 * \ingroup sync
 * \brief Primitives that provide message-passing communication between
 * worker threads.
 *
 * @{
 */

namespace pasl {
namespace sched {
namespace messagestrategy {

/***********************************************************************/
  
struct message_s;
typedef struct message_s message_t;

/*! \class messagestrategy
 *  \ingroup messagestrategy
 *  \brief A mechanism through which workers communicate point to
 *  point via asynchronous message passing.
 *
 * An object of this class maintains for each worker a buffer of
 * incoming messages. Messages are removed from the buffer and
 * processed only by calling the method `check()`. To ensure that
 * messages are handled quickly, a worker needs to call this method
 * periodically.
 *
 * The `send()` operation is nonblocking: it simply pushes the
 * message on the buffer of the target worker.
 *
 */
class messagestrategy : public util::worker::periodic_t {
protected:
  virtual void handle_message(message_t msg);
public:
  virtual ~messagestrategy() { }
  virtual void init() = 0;
  virtual void destroy() = 0;
  //! Sends a message \a msg to worker with id \a target.
  virtual void send(worker_id_t target, message_t msg) = 0;
  // implements periodic_t::check()
};
extern messagestrategy* the_messagestrategy;

/*---------------------------------------------------------------------*/
/**
 * \defgroup message_types Messages
 * \ingroup messagestrategy
 * @{
 */

enum tag_e { IN_DELTA, OUT_ADD, OUT_FINISHED, EMPTY };

typedef struct message_s {
  tag_e tag;
  union {
    struct { instrategy_p in; thread_p t; int64_t d; } in_delta;
    struct { outstrategy_p out; thread_p td; } out_add;
    struct { outstrategy_p out; } out_finished;
  } data;
} message_t;
  
/** @} */

/*---------------------------------------------------------------------*/
/**
 * \defgroup message_constructors Message constructors
 * \ingroup messagestrategy
 * @{
 */

static inline message_t in_delta(instrategy_p in, thread_p t, int64_t d) {
  message_t msg;
  msg.tag = IN_DELTA;
  msg.data.in_delta.in = in;
  msg.data.in_delta.t = t;
  msg.data.in_delta.d = d;
  return msg;
}

static inline message_t out_add(outstrategy_p out, thread_p td) {
  message_t msg;
  msg.tag = OUT_ADD;
  msg.data.out_add.out = out;
  msg.data.out_add.td = td;
  return msg;
}
  
static inline message_t out_finished(outstrategy_p out) {
  message_t msg;
  msg.tag = OUT_FINISHED;
  msg.data.out_finished.out = out;
  return msg;
}

static inline message_t empty() {
  message_t msg;
  msg.tag = EMPTY;
  return msg;
}

//! Sends to worker \a target the message \a msg.
static inline void send(worker_id_t target, message_t msg) {
  assert(the_messagestrategy != NULL);
  the_messagestrategy->send(target, msg);
}
  
/** @} */

/*---------------------------------------------------------------------*/
/* Various message strategies */

/*! \class pcb
 *  \ingroup messagestrategy
 *  \brief A message strategy that uses P(P-1) buffers to store
 *  messages, where P is the number of workers.
 *
 * Thanks to having one buffer per pair of workers, the send and
 * receive operations require neither atomic operations nor
 * fences. 
 * 
 * \remark Although the fences are \a not necessary for x86 TSO, they
 * may be with other consistency models.
 *
 * It is expensive for a worker to check all of its P-1 buffers on
 * every call to `check()`. To reduce the cost, our `check()` method
 * checks at most N buffers and a time. It goes round robin through
 * the buffers to ensure fairness.
 *
 */
class pcb : public messagestrategy {
private:

  typedef data::pcb::linked<message_t> pcb_t;
  typedef data::perworker::array<pcb_t> pcb_vector_t;
  typedef data::perworker::array<pcb_vector_t> pcb_matrix_t;
  typedef data::perworker::array<int> target_t;
  
  pcb_matrix_t channels;
  target_t target;
  
  int nb_processed_per_round;

public:
  void init();
  void destroy();
  void send(worker_id_t target, message_t msg);
  void check();
};

/***********************************************************************/

} // end namespace
  
typedef messagestrategy::message_t message_t;
/*messages passed by value, so we do not need: typedef message_t* message_p; */
typedef messagestrategy::messagestrategy messagestrategy_t;
typedef messagestrategy_t* messagestrategy_p;
  
} // end namespace
} // end namespace

/** @} */

#endif // _MESSAGESTRATEGY_H_
