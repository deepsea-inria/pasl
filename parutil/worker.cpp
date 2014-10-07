/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file worker.cpp
 * 
 */

#include <time.h>
#include <sys/time.h>

#include "worker.hpp"
#include "machine.hpp"
#include "cmdline.hpp"
//#include "logging.hpp"
//#include "stats.hpp"

namespace pasl {
namespace util {
namespace worker {
    
/***********************************************************************/

tls_global_declare(worker_id_t, worker_id)
group_t the_group;
double delta;
// if true, interrupts are enabled
static bool interrupts;
    
/*---------------------------------------------------------------------*/

group_t::group_t() {
  state = NOT_INIT;
}

void group_t::set_factory(controller_factory_p factory) {
  this->factory = factory;
}

void group_t::init(int nb_workers, machine::binding_policy_p bindpolicy) {
  this->nb_workers = nb_workers;
  this->bindpolicy = bindpolicy;
  state = PASSIVE;
  factory = NULL;
  controllers = new controller_p[nb_workers];
  tls_alloc(worker_id_t, worker_id);
  tls_setter(worker_id_t, worker_id, undef);
  interrupts = cmdline::parse_or_default_bool("interrupts", false, false);
}

/*---------------------------------------------------------------------*/
  
void controller_t::init() {
  my_id = get_my_id();
  nb_workers = get_nb();
  allow_interrupt = false;
  date_of_last_interrupt = ticks::now();
  last_check_periodic = ticks::now();
  interrupt_init();
  mysrand((unsigned) (((uint64_t) ticks::now()) +my_id+1));
}

void controller_t::destroy() {
  sa.sa_sigaction = controller_t::dummy_sighandler;
  sigaction(SIGUSR1, &sa, 0);
}
  
void controller_t::enter_wait() {
  
}

void controller_t::exit_wait() {
  
}

void controller_t::yield() {
  check_periodic();
}

/*---------------------------------------------------------------------*/
/* Random worker selection
 */

/*
 * Pseudo-random generator defined by the congruence S' = 69070 * S
 * mod (2^32 - 5).  Marsaglia (CACM July 1993) says on page 107 that
 * this is a ``good one''.  There you go.
 *
 * The literature makes a big fuss about avoiding the division, but
 * for us it is not worth the hassle.
 */
static const unsigned RNGMOD = ((1ULL << 32) - 5);
static const unsigned RNGMUL = 69070U;

unsigned controller_t::myrand() {
  unsigned state = rand_seed;
  state = (unsigned)((RNGMUL * (unsigned long long)state) % RNGMOD);
  /*
  if (state == 0)
    state++;
  state += (unsigned) ticks::now();
  */
  rand_seed = state;
  return state;
}

void controller_t::mysrand(unsigned seed) {
  seed %= RNGMOD;
  seed += (seed == 0); /* 0 does not belong to the multiplicative
			  group.  Use 1 instead */
  rand_seed = seed;
}

worker_id_t controller_t::random_other() {
  int nb_workers = get_nb();
  assert(nb_workers > 1);
  worker_id_t id = (worker_id_t)myrand() % (nb_workers-1);
  if (id >= get_my_id())
    id++;
  return id;
}
    
/*---------------------------------------------------------------------*/
/* Interrupts 
 *
 * Note: at most one signal handler may be active for a given worker
 * at a time. This property follows from the default behavior of
 * `sigaction`, which is to block signal which triggered the handler.
 */

#define POSIX_INTERRUPT_SIGNAL SIGUSR1
  
void controller_t::interrupt_init() {
  sa.sa_sigaction = controller_t::controller_sighandler;
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigfillset (&(sa.sa_mask));
  sigaction (POSIX_INTERRUPT_SIGNAL, &sa, 0);
}

void controller_t::dummy_sighandler(int sig, siginfo_t *si, void *uc) {

}

void controller_t::interrupt_handled() {
  the_group.ping_received[my_id] = true;
  //STAT_COUNT(INTERRUPT);
#if 0
  /*! \warning this logging event is bogus in a signal handler because
   *  of the fact that log events are allocated by the freelist
   *  allocator and the freelist allocator is not signal safe.
   */
  double elapsed = ticks::microseconds_since(controller->date_of_last_interrupt);
  LOG_EVENT(COMM, new util::logging::interrupt_event_t(elapsed));
#endif
}

void controller_t::controller_sighandler(int sig, siginfo_t *si, void *uc) {
 //atomic::aprintf("receive int\n");
  worker_id_t my_id = get_my_id();
  controller_p controller = the_group.get_controller(my_id);
  controller->date_of_last_interrupt = ticks::now();
  if (! controller->allow_interrupt) {
    controller->interrupt_was_blocked = true;
    //! \todo we may need to put this call instead in the post action of check_on_interrupt
    controller->interrupt_handled();
    return;
  }
  controller->check_on_interrupt();
  controller->interrupt_handled();
}
  
#ifndef DISABLE_INTERRUPTS

/*
static struct timespec timespec_from_ns(long nsec) {
  struct timespec tq;
  tq.tv_sec = 0;
  while (nsec >= 1000000000) {
    nsec -= 1000000000;
    tq.tv_sec++;
  }
  tq.tv_nsec = nsec;
  return tq;
}
*/

void* controller_t::ping_loop(void* arg) {
  group_p group = (group_p)arg;
  int nb_workers = get_nb();
  long ping_delay_nsec = 1000 * (long)cmdline::parse_or_default_int("ping", 2000);
  ping_delay_nsec /= nb_workers;
  worker_id_t tgt = 0;
  while (! group->ping_loop_should_exit) {
    if (   group->ping_received[tgt] 
	&& (ticks::nanoseconds_since(group->last_ping_date[tgt]) > ping_delay_nsec
     || group->get_controller(tgt)->should_be_interrupted())) {
      group->ping_received[tgt] = false;
      group->last_ping_date[tgt] = ticks::now();
      the_group.send_interrupt(tgt);
    }
    tgt = (tgt + 1) % nb_workers;
  }
  return NULL;
}
 
#endif

void group_t::ping_loop_create() {
#ifndef DISABLE_INTERRUPTS
  if (! interrupts)
    return;
  ping_loop_should_exit = false;
  ping_received = new bool[nb_workers];
  for (worker_id_t id = 0; id < nb_workers; id++)
    ping_received[id] = true;
  last_ping_date = new ticks_t[nb_workers];
  for (worker_id_t id = 0; id < nb_workers; id++)
    last_ping_date[id] = ticks::now();    
  pthread_create(&ping_loop_thread, NULL, controller_t::ping_loop, this);
#endif
}

void group_t::ping_loop_destroy() {
  if (! interrupts)
    return;
  ping_loop_should_exit = true;
  pthread_join(ping_loop_thread, NULL);
  delete [] ping_received;
  delete [] last_ping_date;
}

void group_t::send_interrupt(worker_id_t id) {
  if (! interrupts)
    return;
  pthread_kill(pthreads[id], POSIX_INTERRUPT_SIGNAL);
}


/*---------------------------------------------------------------------*/
/* Periodic checks */

void controller_t::add_periodic(periodic_p p) {
  assert(my_id != undef);
  periodic_set.push_back(p);
}

void controller_t::rem_periodic(periodic_p p) {
  bool found = false;
  periodic_set_t::iterator iter;
  for (iter = periodic_set.begin(); iter < periodic_set.end(); iter++)
    if (*iter == p) {
      found = true;
      break;
    }
  if (!found) atomic::die("failed to remove periodic check\n");
  periodic_set.erase(iter);
}

void controller_t::check_periodic() {
  double delay = ticks::microseconds_since(last_check_periodic);
  if (delay > delta) { 
    last_check_periodic = ticks::now();
    for (periodic_set_t::iterator iter = periodic_set.begin();
         iter < periodic_set.end();
         iter++) {
      periodic_p p = *iter;
      p->check();
    }
  }
}
  
/*---------------------------------------------------------------------*/

bool group_t::is_active() {
  return (state == ACTIVE);
}

int group_t::get_nb () const {
  return nb_workers; 
}

controller_p group_t::get_controller(worker_id_t id) {
  assert(state == ACTIVE);
  return controllers[id];
}

controller_p group_t::get_controller0() {
  return get_controller(0);
}

machine::binding_policy_p group_t::get_bindpolicy() {
  return bindpolicy;
}

worker_id_t group_t::get_my_id() {
  assert(state != NOT_INIT);
  assert(state != PASSIVE);
  return worker::get_my_id();
}

worker_id_t group_t::get_my_id_or_undef() {
  assert(state != NOT_INIT);
  if (state == PASSIVE) {
    return undef;
  } else {
    return get_my_id();
  }
}

/*---------------------------------------------------------------------*/

bool group_t::exit_controller() {
  assert(state == ACTIVE);
  if (all_workers_should_exit)
    return true;
  else if (get_my_id() == 0 && worker0_should_exit)
    return true;
  else
    return false;
}
  
void group_t::run_worker0() {
  assert(state == ACTIVE);
  assert(! worker0_running);
  // notify of the new launch
  for (worker_id_t id = 0; id < nb_workers; id++) 
    controllers[id]->new_launch();
  worker0_running = true;
  worker0_should_exit = false;
  controllers[0]->run();
  worker0_running = false;
}
  
void group_t::request_exit_worker0() {
  worker0_should_exit = true;
}

/*---------------------------------------------------------------------*/

void* group_t::build_thread(void* arg) {
  std::pair<worker_id_t, group_p>* worker_init = 
    (std::pair<worker_id_t, group_p>*)arg;
  worker_id_t my_id = worker_init->first;
  group_p group = worker_init->second;
  delete worker_init;
  tls_setter(worker_id_t, worker_id, my_id);
  group->get_bindpolicy()->pin_calling_thread(my_id);
  controller_p controller = group->factory->create_controller();
  group->controllers[my_id] = controller;
  if (my_id != 0) {
    controller->init();
    group->creation_barrier.wait();
    controller->run();
    controller->destroy();
    group->destruction_barrier.wait();
  }
  return NULL;
}

/*---------------------------------------------------------------------*/

void group_t::create_threads() {
  assert(state == PASSIVE);
  assert(factory != NULL);
  state = ACTIVE;
  all_workers_should_exit = false;
  factory->create_shared_state();
  pthreads = new pthread_t[nb_workers];
  creation_barrier.init(nb_workers);
  destruction_barrier.init(nb_workers);
  for (worker_id_t id = 1; id < nb_workers; id++) {
    std::pair<worker_id_t, group_p>* worker_init = 
      new std::pair<worker_id_t, group_p>;
    worker_init->first = id;
    worker_init->second = this;
    pthread_create(&(pthreads[id]), NULL, build_thread, worker_init);
  }
  pthreads[0] = pthread_self();
  std::pair<worker_id_t, group_p>* worker0_init = 
    new std::pair<worker_id_t, group_p>;
  worker0_init->first = 0;
  worker0_init->second = this;
  build_thread(worker0_init);
  creation_barrier.wait();
  controllers[0]->init();
#ifndef DISABLE_INTERRUPTS
  ping_loop_create();
#endif
}

void group_t::destroy_threads() {
  assert(state == ACTIVE);
  all_workers_should_exit = true;
#ifndef DISABLE_INTERRUPTS
  ping_loop_destroy();
#endif
  controllers[0]->destroy();
  destruction_barrier.wait();
  factory->delete_shared_state();
  delete [] pthreads;
  for (worker_id_t id = 0; id < nb_workers; id++) {
    factory->destroy_controller(controllers[id]);
  }
  delete [] controllers;
  state = PASSIVE;
}

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

