#include <time.h>
#include "tls.hpp"
#include "scheduler.hpp"
#include "messagestrategy.hpp"

namespace pasl {
namespace sched {
namespace scheduler {


/***********************************************************************/

util::worker::controller_factory_t* the_factory;

bool _private::stay() {
  return ! periodic_set.empty() || ! util::worker::the_group.exit_controller();
}

/*---------------------------------------------------------------------*/
/* Methods required by worker::controller_t */

void _private::init() { 
  controller_t::init();
  //add_periodic(messagestrategy::the_messagestrategy);
  current_thread = nullptr;
  should_communicate = false;
}

void _private::destroy() {
  controller_t::destroy();
  //rem_periodic(messagestrategy::the_messagestrategy);
}

void _private::new_launch() {
  //STAT_ONLY(date_enter_wait = ticks::now());
  STAT_IDLE_ONLY(date_enter_wait = util::microtime::now());
}

void _private::enter_wait() {
  LOG_BASIC(ENTER_WAIT);
  STAT_COUNT(ENTER_WAIT);
 // STAT_IDLE_ONLY(date_enter_wait = ticks::now());
   STAT_IDLE_ONLY(date_enter_wait = util::microtime::now());
  util::worker::controller_t::enter_wait();
}
  
void _private::exit_wait() {
  util::worker::controller_t::exit_wait();
  //STAT_IDLE(add_to_idle_time(ticks::seconds_since(date_enter_wait)));
  //debug: atomic::aprintf("%f\n", microtime::seconds_since(date_enter_wait));
  STAT_IDLE(add_to_idle_time(util::microtime::seconds_since(date_enter_wait)));
  LOG_BASIC(EXIT_WAIT);
}

/*---------------------------------------------------------------------*/
/* Thread management */

// later: move somewhere else
template <class Pointer>
void pasl_delete(Pointer p) {
  delete p;
  //  util::atomic::aprintf("%lld deleting p=%p\n",util::worker::get_my_id(),p);
}

void _private::exec(thread_p t) {
  assert(t != nullptr);
  LOG_THREAD(THREAD_EXEC, t);
  STAT_COUNT(THREAD_EXEC);
#ifdef TRACK_LOCALITY
  LOG_EVENT(LOCALITY, new util::logging::locality_event_t(logging::LOCALITY_START, t->locality.low));
#endif
  bool should_not_deallocate = t->should_not_deallocate;
  reuse_thread_requested = false;
  current_thread = t;
  current_outstrategy = t->out;
  t->out = nullptr; // optional
  interrupt_was_blocked = false;
  if (interrupt_was_blocked)
    check_on_interrupt();
  allow_interrupt = true;
  t->exec();
  allow_interrupt = false;
#ifdef TRACK_LOCALITY
  LOG_EVENT(LOCALITY, new util::logging::locality_event_t(logging::LOCALITY_STOP, t->locality.hi));
#endif
  //! \todo could handle interrupt_was_blocked
  if (should_not_deallocate || reuse_thread_requested)
    t->reset_caches();
  else
    pasl_delete(t);
  LOG_THREAD(THREAD_FINISH, t);
  outstrategy::finished(t, current_outstrategy);
  current_outstrategy = nullptr; // optional
  current_thread = nullptr; // would probably be optional when assertions are disabled
}

void _private::add_thread(thread_p t) {
  instrategy::init(t->in, t);
  LOG_THREAD(THREAD_CREATE, t);
  STAT_COUNT(THREAD_CREATE);
}

void _private::add_dependency(thread_p t1, thread_p t2) {
  assert (t1->out != nullptr);
  assert (t2->in != nullptr);
  outstrategy::add(t1->out, t2);
  instrategy::delta(t2->in, t2, +1l);
}

outstrategy_p _private::capture_outstrategy() {
  assert (current_outstrategy != nullptr);
  outstrategy_p out = current_outstrategy;
  current_outstrategy = outstrategy::noop_new();
  return out;
}

void _private::decr_dependencies(thread_p t) {
  instrategy::delta(t->in, t, -1l);
}

void _private::reuse_calling_thread() {
  reuse_thread_requested = true;
}

thread_p _private::get_current_thread() const {
  return current_thread;
}

void _private::schedule(thread_p t) {
  t->in = nullptr;
  assert (t->out != nullptr);
  LOG_THREAD(THREAD_SCHEDULE, t);
  if (! allow_interrupt)
    add_to_pool_of_ready_threads(t);
  else {
    interrupt_was_blocked = false;
    allow_interrupt = false;
    add_to_pool_of_ready_threads(t);
    if (interrupt_was_blocked)
      check_on_interrupt();
    allow_interrupt = true;
  }
}

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace
