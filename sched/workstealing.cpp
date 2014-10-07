/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file workstealing.cpp
 *
 */

#include <math.h>

#include <iostream>
//#include <chrono>
//#include <thread>

#include "atomic.hpp"
#include "workstealing.hpp"
#include "barrier.hpp"
#include "cmdline.hpp"

namespace pasl {
namespace sched {
namespace workstealing {

/***********************************************************************/

threadset_shared::threadset_shared() {
  nb_tries_per_communicate =
    util::cmdline::parse_or_default_int("nb_tries_per_communicate", 1, false);
}

threadset_shared::~threadset_shared() {
}

bool threadset_private::stay_in_acquire() {
  return scheduler::_private::stay() && ! local_has();
}

/*---------------------------------------------------------------------*/

class alarm_by_ticks : public alarm {
private:
  ticks_t last_communicate;

public:
  void init(util::worker::controller_p controller) {
    this->controller = controller;
    last_communicate = util::ticks::now();
  }

  bool ready() {
    double delay = util::ticks::microseconds_since(last_communicate);
    return (delay > util::worker::delta);
  }

  void reset() {
    last_communicate = util::ticks::now();
  }
};

/*---------------------------------------------------------------------*/

class alarm_by_poisson : public alarm {
private:
  ticks_t last_communicate;
  double delay_to_next_communicate;

  void pick_delay_to_next_communicate() {
    // pick x uniformy distributed in [0,1)
    double x = (double) (controller->myrand()) / ((double) (RAND_MAX));
    // use a formula to predict time to next event (Knuth 3.4.1)
    delay_to_next_communicate = - log(x) * ((double) util::worker::delta);
    //atomic::aprintf("delay %lf\n", delay_to_next_communicate);
  }

public:
  void init(util::worker::controller_p controller) {
    this->controller = controller;
    last_communicate = util::ticks::now();
    pick_delay_to_next_communicate();
  }

  bool ready() {
    double delay = util::ticks::microseconds_since(last_communicate);
    bool r = (delay > delay_to_next_communicate);
    // if (!r) atomic::aprintf("too early %lf\n", delay);
    return r;
  }

  void reset() {
    last_communicate = util::ticks::now();
    pick_delay_to_next_communicate();
  }
};

/*---------------------------------------------------------------------*/

alarm* create_alarm() {
  alarm* alarm;
  bool use_poisson = util::cmdline::parse_or_default_bool("poisson", true);
  if (use_poisson)
    alarm = new alarm_by_poisson();
  else
    alarm = new alarm_by_ticks();
  return alarm;
}

/*---------------------------------------------------------------------*/
/* CAS-based sender-initiated work stealing */

//! \todo: const instead of define
#define WAITING   ((cas_si_shared::state_t)0x1)
#define INCOMING  ((cas_si_shared::state_t)0x3)
#define WORKING   ((cas_si_shared::state_t)0x5)

cas_si_shared::cas_si_shared() : threadset_shared::threadset_shared() {
  for (worker_id_t id = 0; id < util::worker::get_nb(); id++)
    states[id].store(WORKING);
}

cas_si_shared::~cas_si_shared() {
}

/*---------------------------------------------------------------------*/

void cas_si_private::init() {
  allow_interrupt = false;
  scheduler::_private::init();
  _alarm = create_alarm();
  _alarm->init(this);
}

void cas_si_private::destroy() {
  delete _alarm;
  scheduler::_private::destroy();
}

void cas_si_private::acquire() {
  assert(shared->states[my_id].load() == WORKING);
  shared->states[my_id].store(WAITING);
  while (true) {
    if (shared->states[my_id].load() == WAITING || shared->states[my_id].load() == INCOMING) {
      if (! stay_in_acquire()) {
        cancel_acquire();
        return;
      } else
        util::worker::controller_t::yield();
    } else {
      thread_p thread = (thread_p) shared->states[my_id].load();
      shared->states[my_id].store(WORKING);
      remote_push(thread);
      LOG_THREAD(THREAD_SEND, thread);
      STAT_COUNT(THREAD_SEND);
      return;
    }
  }
}

/* used when shceduler->stay() returns false even though there are
 * still some incomplete threads around
 */
void cas_si_private::cancel_acquire() {
  bool s = false;
  cas_si_shared::state_t state;
  while (! s) {
    state = shared->states[my_id].load();
    s = shared->states[my_id].compare_exchange_strong(state, WORKING);
  }
  if (state != WAITING && state != WORKING && state != INCOMING) {
    remote_push(state);
    STAT_COUNT(THREAD_RECOVER);
  }
}

void cas_si_private::communicate() {
  LOG_BASIC(COMMUNICATE);
  STAT_COUNT(COMMUNICATE);
  if (nb_workers < 2) return;
  if (! remote_has()) return;
  _alarm->reset();
  should_communicate = false;
  for (int nb_tries = 0; nb_tries < shared->nb_tries_per_communicate; nb_tries++) {
    worker_id_t id = random_other();
    if (shared->states[id].load() != WAITING) continue;
    thread_p orig = WAITING;
    bool s = shared->states[id].compare_exchange_strong(orig, INCOMING);
    if (! s) continue;
    else {
      shared->states[id].store(remote_pop());
      return;
    }
  }
}

void cas_si_private::run() {
  while (stay()) {
    thread_p t = try_local_pop();
    if (t != NULL) {
      should_communicate = false;
      exec(t);
      check();
    } else
      wait();
  }
}

void cas_si_private::wait() {
  enter_wait();
  acquire();
  exit_wait();
}

void cas_si_private::check() {
  if (should_communicate || _alarm->ready()) {
    //! \todo  time_to_communicate() should not be confused with time_to_check_periodic..
    scheduler::_private::check_periodic();
    communicate();
  }
}
  
bool cas_si_private::should_call_communicate() {
  for (int nb_tries = 0; nb_tries < shared->nb_tries_per_communicate; nb_tries++) {
    worker_id_t id = random_other();
    if (shared->states[id].load() == WAITING)
      return true;
  }
  return false;
}

void cas_si_private::check_on_interrupt() {
  assert(false);
  //! \todo worker.cpp should not call this function directly but a function scheduler::_private::interrupt() which would do the logging and then call the virtual function check_on_interrupt
  LOG_BASIC(INTERRUPT);
  STAT_COUNT(INTERRUPT);
  _private::check_on_interrupt();
  should_communicate = true;
  check();
}

#undef WAITING
#undef INCOMING
#undef WORKING

/*---------------------------------------------------------------------*/
/* CAS-based receiver initiated work stealing */

//! \todo: factorize code!

cas_ri_shared::cas_ri_shared() : threadset_shared::threadset_shared() {
  for (worker_id_t id = 0; id < util::worker::get_nb(); id++)
    requests[id].store(REQUEST_WAITING);
  answers.init(ANSWER_REJECT);
}

cas_ri_shared::~cas_ri_shared() {

}

/*---------------------------------------------------------------------*/

void cas_ri_private::init() {
  allow_interrupt = false;
  scheduler::_private::init();
  last_communicate = util::ticks::now();
  my_request_ptr = & (shared->requests[my_id]);
}

void cas_ri_private::destroy() {
  scheduler::_private::destroy();
}

void cas_ri_private::reject() { // TODO: rename this to reject_and_block
  request_t i = my_request_ptr->load();
  if (i == REQUEST_BLOCKED) {
    return;
  } else if (i == REQUEST_WAITING) {
    request_t orig = REQUEST_WAITING;
    bool s = my_request_ptr->compare_exchange_strong(orig, REQUEST_BLOCKED);
    if (! s)
      reject();
  } else {
    // i is the id of another thread
    shared->answers[i] = ANSWER_REJECT;
    bool s = my_request_ptr->compare_exchange_strong(i, REQUEST_BLOCKED);
    if (! s)
      util::atomic::die("cas_ri invariant broken: my_request_ptr was changed while holding an id.");
  }
}

void cas_ri_private::sleep_in_acquire(double nb_microseconds) {
  // std::chrono::milliseconds dura( 1 );
  // std::this_thread::sleep_for( dura );
  pasl::util::ticks::microseconds_sleep(nb_microseconds);
}

void cas_ri_private::unblock() {
  // assert(my_request_ptr->load() == REQUEST_BLOCKED);
  my_request_ptr->store(REQUEST_WAITING);
}

void cas_ri_private::acquire() {
  if (nb_workers < 2) {
    scheduler::_private::check_periodic();
    return;
  }
  reject();
  // TODO: writes in answer_ptr should be "store" function calls

  thread_p thread = NULL;
  answer_t* answer_ptr = & (shared->answers[my_id]);
  while (true) {
    scheduler::_private::check_periodic();
    if (! stay_in_acquire())
      goto cleanup;

    // may yield here
    sleep_in_acquire(1);

    *answer_ptr = ANSWER_WAITING;
    worker_id_t id = random_other();
    if (shared->requests[id].load() != REQUEST_WAITING){
      continue;
    }
    request_t orig = REQUEST_WAITING;
    bool s = shared->requests[id].compare_exchange_strong(orig, my_id);
    if (! s)
      continue;

    while (*answer_ptr == ANSWER_WAITING) {
      sleep_in_acquire(1); // may yield here as well
      //util::atomic::print([&] { std::cout << "***waiting answer " << my_id << std::endl; });
      if (! stay_in_acquire()) {
        shared->requests[id].store(REQUEST_WAITING);
        goto cleanup;
      }
    }
    //util::atomic::aprintf("reception from %d to %d\n", my_id, id);

    if (*answer_ptr == ANSWER_REJECT){
      continue;
    }
    thread = (thread_p) *answer_ptr;
    break;
  }
  remote_push(thread);
  //! \todo: thread_receive event?
  LOG_THREAD(THREAD_SEND, thread);
  STAT_COUNT(THREAD_SEND);

  cleanup:
  unblock();
}

bool cas_ri_private::time_to_communicate() {
  double delay = util::ticks::microseconds_since(last_communicate);
  return (delay > util::worker::delta);
}

void cas_ri_private::communicate() {
  LOG_BASIC(COMMUNICATE);
  STAT_COUNT(COMMUNICATE);
  last_communicate = util::ticks::now();
  should_communicate = false;
  if (nb_workers < 2) return;
  scheduler::_private::check_periodic();

  request_t j = my_request_ptr->load();
  if (j == REQUEST_WAITING)
    return;
  if (remote_has()) {
    shared->answers[j] = remote_pop();
  } else {
    shared->answers[j] = ANSWER_REJECT;
  }
  my_request_ptr->store(REQUEST_WAITING);
}

/* deprecated, but keep around for now
void cas_ri_private::reject_queries() {
  request_t j = my_request_ptr->load();
  if (j == REQUEST_WAITING)
    return;
  shared->answers[j] = ANSWER_REJECT;
  my_request_ptr->store(REQUEST_WAITING);
}
*/
  
bool cas_ri_private::should_call_communicate() {
  return my_request_ptr->load() != REQUEST_WAITING;
}

void cas_ri_private::run() {
  while (stay()) {
    thread_p t = try_local_pop();
    if (t != NULL) {
      should_communicate = false;
      exec(t);
      check();
    } else
      wait();
  }
}

void cas_ri_private::wait() {
  enter_wait();
  acquire();
  exit_wait();
}

void cas_ri_private::check() {
  communicate();

  //! \todo Problem if calls to check_periodic!

/*
  if (should_communicate || time_to_communicate()) {
    //! \todo  time_to_communicate() should not be confused with time_to_check_periodic..
    scheduler::_private::check_periodic();
    communicate();
  }
*/
}

void cas_ri_private::check_on_interrupt() {
  //! \todo worker.cpp should not call this function directly but a function scheduler::_private::interrupt() which would do the logging and then call the virtual function check_on_interrupt
  LOG_BASIC(INTERRUPT);
  STAT_COUNT(INTERRUPT);
  _private::check_on_interrupt();
  should_communicate = true;
}


/*---------------------------------------------------------------------*/
// with interrupts

void cas_ri_interrupt_private::acquire() {
  thread_p thread = NULL;
  answer_t* answer_ptr = & (shared->answers[my_id]);
  while (true) {
    if (! stay_in_acquire())
      goto cleanup;

    // may yield here
    *answer_ptr = ANSWER_WAITING;
    worker_id_t id = random_other();
    if (shared->requests[id].load() != REQUEST_WAITING)
      continue;
    worker_id_t orig = REQUEST_WAITING;
    bool s = shared->requests[id].compare_exchange_strong(orig, my_id);
    if (! s)
      continue;

    while (*answer_ptr == ANSWER_WAITING) {
      communicate();
      if (! stay_in_acquire()) {
        shared->requests[id].store(REQUEST_WAITING);
        goto cleanup;
      }
    }
    if (*answer_ptr == ANSWER_REJECT)
      continue;
    thread = (thread_p) *answer_ptr;
    break;
    communicate();
  }
  remote_push(thread);
  //! \todo: thread_receive event?
  LOG_THREAD(THREAD_SEND, thread);
  STAT_COUNT(THREAD_SEND);

  cleanup:
  my_request_ptr->store(REQUEST_WAITING);
}


void cas_ri_interrupt_private::check() {
}

void cas_ri_interrupt_private::check_on_interrupt() {
  //! \todo worker.cpp should not call this function directly but a function scheduler::_private::interrupt() which would do the logging and then call the virtual function check_on_interrupt
//atomic::aprintf("receiving\n");
  LOG_BASIC(INTERRUPT);
  STAT_COUNT(INTERRUPT);
  _private::check_on_interrupt();
  communicate();
  //should_communicate = true;

}

/*
//! \todo: cancel copy-past here
void cas_ri_interrupt_private::acquire() {
  reject();
  thread_p thread = NULL;
  answer_t* answer_ptr = & (shared->answers[my_id]);
  while (true) {
    if (! stay_in_acquire())
      goto cleanup;

    // may yield here
    *answer_ptr = ANSWER_WAITING;
    worker_id_t id = random_other();
    request_t* request_ptr = & (shared->requests[id]);
    if (*request_ptr != REQUEST_WAITING)
      continue;
    bool s = atomic::compare_and_swap_ptr((void *volatile *)request_ptr,
                                          (void*) REQUEST_WAITING,
                                          (void*) my_id);
    if (! s)
      continue;

   // ticks_t last = ticks::now();
   // worker::the_group.send_interrupt(id);
//atomic::aprintf("sending\n");
    while (*answer_ptr == ANSWER_WAITING) {
      double length = ticks::microseconds_since(last);
      if (length > 200.) {
        ticks_t last = ticks::now();
        worker::the_group.send_interrupt(id);
      }

      if (! stay_in_acquire()) {
        *request_ptr = REQUEST_WAITING;
        goto cleanup;
      }
    }
    if (*answer_ptr == ANSWER_REJECT)
      continue;
    thread = (thread_p) *answer_ptr;
    break;
  }
  remote_push(thread);
  //! \todo: thread_receive event?
  LOG_THREAD(THREAD_SEND, thread);
  STAT_COUNT(THREAD_SEND);

  cleanup:
  *my_request_ptr = REQUEST_WAITING;
}
*/

/*---------------------------------------------------------------------*/
/* Work stealing with shared deques */

static thread_p const STEAL_RES_EMPTY = (thread_p) 0;
static thread_p const STEAL_RES_ABORT = (thread_p) 1;

thread_p chase_lev_deque::cb_get (
     chase_lev_deque::buffer_t buf, int64_t capacity, int64_t i) {
  return buf[i % capacity].load();
}

void chase_lev_deque::cb_put (
    chase_lev_deque::buffer_t buf, int64_t capacity, int64_t i, thread_p x) {
  buf[i % capacity].store(x);
}

chase_lev_deque::buffer_t chase_lev_deque::new_buffer(size_t capacity) {
  return new std::atomic<thread_p>[capacity];
}

void chase_lev_deque::delete_buffer(chase_lev_deque::buffer_t buf) {
  delete [] buf;
}

chase_lev_deque::buffer_t chase_lev_deque::grow (
    chase_lev_deque::buffer_t old_buf,
    int64_t old_capacity,
    int64_t new_capacity,
    int64_t b,
    int64_t t) {
  chase_lev_deque::buffer_t new_buf = new_buffer(new_capacity);
  for (int64_t i = t; i < b; i++)
    cb_put (new_buf, new_capacity, i, cb_get (old_buf, old_capacity, i));
  return new_buf;
}

bool chase_lev_deque::cas_top (int64_t old_val, int64_t new_val) {
  int64_t ov = old_val;
  return top.compare_exchange_strong(ov, new_val);
}

void chase_lev_deque::init(int64_t init_capacity) {
  capacity.store(init_capacity);
  buf = new_buffer(capacity.load());
  bottom.store(0l);
  top.store(0l);
  for (int64_t i = 0; i < capacity.load(); i++)
    buf[i].store(NULL); // optional
}

void chase_lev_deque::destroy() {
  assert (bottom.load() - top.load() == 0); // maybe wrong
  delete_buffer(buf);
}

void chase_lev_deque::push_back(thread_p item) {
  int64_t b = bottom.load();
  int64_t t = top.load();
  if (b-t >= capacity.load() - 1) {
    chase_lev_deque::buffer_t old_buf = buf;
    int64_t old_capacity = capacity.load();
    int64_t new_capacity = capacity.load() * 2;
    buf = grow (old_buf, old_capacity, new_capacity, b, t);
    capacity.store(new_capacity);
    // UNSAFE! delete old_buf;
  }
  cb_put (buf, capacity.load(), b, item);
  // requires fence store-store
  bottom.store(b + 1);
}

thread_p chase_lev_deque::pop_front() {
  int64_t t = top.load();
  // requires fence load-load
  int64_t b = bottom.load();
  // would need a fence load-load if were to read the pointer on deque->buf
  if (t >= b)
    return STEAL_RES_EMPTY;
  thread_p item = cb_get (buf, capacity.load(), t);
  // requires fence load-store
  if (! cas_top (t, t + 1))
    return STEAL_RES_ABORT;
  return item;
}

thread_p chase_lev_deque::pop_back() {
  int64_t b = bottom.load() - 1;
  bottom.store(b);
  // requires fence store-load
  int64_t t = top.load();
  if (b < t) {
    bottom.store(t);
    return NULL;
  }
  thread_p item = cb_get (buf, capacity.load(), b);
  if (b > t)
    return item;
  if (! cas_top (t, t + 1)) {
    item = NULL;
  }
  bottom.store(t + 1);
  return item;
}

size_t chase_lev_deque::nb_threads() {
  return (size_t)bottom.load() - top.load();
}

bool chase_lev_deque::empty() {
  return nb_threads() < 1;
}

shared_deques_shared::shared_deques_shared() {
  //  scheduler::_shared();
  deques.init(NULL);
  creation_barrier.init(util::worker::get_nb());
}

shared_deques_shared::~shared_deques_shared() {
}

void shared_deques_private::init() {
  my_deque.init(1024l);
  scheduler::_private::init();
  _shared->deques[util::worker::get_my_id()] = &my_deque;
}

void shared_deques_private::destroy() {
  scheduler::_private::destroy();
}

// moves threads from fresh to ready set
void shared_deques_private::flush() {
  for (int i = 0; i < my_fresh.size(); i++)
    my_deque.push_back(my_fresh[i]);
  my_fresh.clear();
}

void shared_deques_private::run() {
  if (!initialized)
    _shared->creation_barrier.wait();
  initialized = true;
  while (stay()) {
    flush();
    thread_p t = my_deque.pop_back();
    if (t != NULL) {
      exec(t);
      check();
    } else {
      enter_wait();
      acquire();
      exit_wait();
    }
  }
}

void shared_deques_private::check() {
  scheduler::_private::check_periodic();
  flush(); // need to flush in case periodic check schedules a thread
}

void shared_deques_private::acquire() {
  if (nb_workers < 2) {
    check();
    return;
  }
  int nb_tries = 0;
  while (stay()) {
    check();
    worker_id_t id_target = random_other();
    chase_lev_deque* target = _shared->deques[id_target];
    thread_p thread = target->pop_front();
    if (thread == STEAL_RES_EMPTY) {
      LOG_BASIC(STEAL_FAIL);
    } else if (thread == STEAL_RES_ABORT) {
      LOG_BASIC(STEAL_ABORT);
    } else {
      LOG_BASIC(STEAL_SUCCESS);
      STAT_COUNT(THREAD_SEND);
      my_deque.push_back(thread);
      return;
    }
    nb_tries++;
    if (nb_tries > util::worker::get_nb()) {
      nb_tries = 0;

      //controller_t::yield();
      // TODO: microsleep
    }
  }
}

void shared_deques_private::check_on_interrupt() {

}

void shared_deques_private::add_to_pool_of_ready_threads(thread_p thread) {
  my_fresh.push_back(thread);
}

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace
