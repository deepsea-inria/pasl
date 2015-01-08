/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file threaddag.cpp
 *
 */

#include <deque>

#include "cmdline.hpp"
#include "callback.hpp"
#include "threaddag.hpp"
#include "machine.hpp"
#include "scheduler.hpp"
#include "workstealing.hpp"
#include "native.hpp"
#include "instrategy.hpp"
#include "outstrategy.hpp"


/***********************************************************************/

namespace pasl {

/*---------------------------------------------------------------------*/
/* Native calling convention */

namespace util {
namespace control {

class context_wrapper_type {
public:
  util::control::context::context_type cxt;
};
data::perworker::array<context_wrapper_type> cxts;

util::control::context::context_pointer my_cxt() {
  return context::addr(cxts.mine().cxt);
}

} // end namespace
} // end namespace

namespace sched {
namespace native {
  int loop_cutoff;

char multishot::dummy1;
char multishot::dummy2;

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

namespace sched {

double kappa;

namespace threaddag {

/*---------------------------------------------------------------------*/
/* Defaults for in- and out-strategies */

typedef enum { FETCH_ADD, OPTIMISIC, MESSAGE /*, FENCEFREE_INSTRATEGY */ } instrategy_class_t;
instrategy_class_t instrategy_class_forkjoin;

instrategy_p new_forkjoin_instrategy() {
  instrategy_p in = NULL;
  switch (instrategy_class_forkjoin) {
    //case OPTIMISIC: in = new instrategy::optimistic(); break;
    case FETCH_ADD: in = instrategy::fetch_add_new(); break;
    case MESSAGE: in = new instrategy::message(); break;
    //case FENCEFREE_INSTRATEGY: in = fencefree::select_instrategy(); break;
    default: util::atomic::die("bogus instrategy");
  }
  return in;
}

typedef enum { UNARY, FENCEFREE_OUTSTRATEGY } outstrategy_class_t;
static outstrategy_class_t outstrategy_class_forkjoin;

outstrategy_p new_forkjoin_outstrategy(branch_t branch = UNDEFINED) {
  outstrategy_p out = NULL;
  switch (outstrategy_class_forkjoin) {
    case UNARY: out = outstrategy::unary_new(); break;
    //case FENCEFREE_OUTSTRATEGY: out = fencefree::select_outstrategy(branch); break;
    default: util::atomic::die("bogus outstrategy");
  }
  return out;
}

// all parameters that are not specific to pasl should be initialized here
static int init_general_purpose() {
  util::atomic::verbose = util::cmdline::parse_or_default_bool("verbose", false, false);
#ifdef SEQUENTIAL_ELISION
  int nb_workers = util::cmdline::parse_or_default_int("proc", 1, false);
  if (nb_workers > 1)
    util::atomic::die("Tried to use > 1 processors in sequential-elision mode");
#elif defined(USE_CILK_RUNTIME)
  int nb_workers = util::cmdline::parse_or_default_int("proc", 1, false);
  std::string nb_workers_str = std::to_string(nb_workers);
  __cilkrts_set_param("nworkers", nb_workers_str.c_str());
#else
  int nb_workers = util::cmdline::parse_or_default_int("proc", 1, true);
#endif
  native::loop_cutoff = util::cmdline::parse_or_default_int("loop_cutoff", 10000);
  std::string htmodestr =
    util::cmdline::parse_or_default_string("hyperthreading", "useall", false);
  util::machine::hyperthreading_mode_t htmode = util::machine::htmode_of_string(htmodestr);
  util::machine::init(htmode);
  data::estimator::init();
  return nb_workers;
}

static void init_basic(int nb_workers) {
  util::atomic::init_print_lock();
  kappa = 1.33 * util::cmdline::parse_or_default_double("kappa", 500.0, false); //LATER: improve
#ifdef USE_CILK_RUNTIME
  util::worker::the_group.set_nb(nb_workers);
  return;
#endif
  util::worker::delta = util::cmdline::parse_or_default_double("delta", kappa/2.0, false);
  if (nb_workers == 0)
    nb_workers = 1;
  assert (nb_workers >= 1);
  std::string nbpstr =
    util::cmdline::parse_or_default_string("numa_binding_policy", "none", false);
  util::machine::binding_policy::policy_t nbpe =
    util::machine::binding_policy::policy_of_string(nbpstr);
  bool no0 = util::cmdline::parse_or_default_bool("no0", false, false);
  util::machine::the_bindpolicy.init(nbpe, no0, nb_workers);
  util::machine::the_numa.init(nb_workers);
  util::worker::the_group.init(nb_workers, &util::machine::the_bindpolicy);
  LOG_ONLY(util::logging::the_recorder.init());
  STAT_IDLE_ONLY(util::stats::the_stats.init());
}

static void destroy_basic() {
  LOG_ONLY(util::logging::output());
  LOG_ONLY(util::logging::the_recorder.destroy());
  data::estimator::destroy();
  util::machine::the_bindpolicy.destroy();
  util::machine::destroy();
}

static void init_scheduler() {
  std::string schedulerstr =
  util::cmdline::parse_or_default_string("scheduler", "workstealing", false);
  instrategy_class_forkjoin = FETCH_ADD;
  outstrategy_class_forkjoin = UNARY;
  if (schedulerstr.compare("workstealing") == 0) {
    std::string tsetstr = util::cmdline::parse_or_default_string("threadset", "cas_ri", false);
    if (tsetstr.compare("cas_si") == 0) {
      scheduler::the_factory =
        new scheduler::factory<workstealing::cas_si_shared,
                               workstealing::cas_si_private>();
    } else if (tsetstr.compare("cas_ri") == 0) {
      scheduler::the_factory =
        new scheduler::factory<workstealing::cas_ri_shared,
                               workstealing::cas_ri_private>();
    } else if (tsetstr.compare("cas_ri_interrupt") == 0) {
      scheduler::the_factory =
        new scheduler::factory<workstealing::cas_ri_interrupt_shared,
                              workstealing::cas_ri_interrupt_private>();
    } else if (tsetstr.compare("shared_deques") == 0) {
      scheduler::the_factory =
        new scheduler::factory<workstealing::shared_deques_shared,
                               workstealing::shared_deques_private>();
    } else
      util::atomic::die("bogus work stealing scheduler %s\n", tsetstr.c_str());
  } else
    util::atomic::die("bogus scheduler %s\n", schedulerstr.c_str());
}

static void destroy_scheduler() {
  delete scheduler::the_factory;
}

static void init_messagestrategy() {
  messagestrategy::the_messagestrategy = new messagestrategy::pcb();
  messagestrategy::the_messagestrategy->init();
}

static void destroy_messagestrategy() {
  messagestrategy::the_messagestrategy->destroy();
  delete messagestrategy::the_messagestrategy;
}

void init() {
  int nb_workers = init_general_purpose();
  init_basic(nb_workers);
#ifndef USE_CILK_RUNTIME
  init_scheduler();
#endif
  util::callback::init();
#ifdef USE_CILK_RUNTIME
  return;
#endif
  util::worker::the_group.set_factory(scheduler::the_factory);
  util::worker::the_group.create_threads();
}

void change_factory(util::worker::controller_factory_t* factory) {
  util::worker::the_group.destroy_threads();
  destroy_scheduler();
  util::worker::the_group.set_factory(factory);
  util::worker::the_group.create_threads();
}

void launch(thread_p t) {
#ifdef USE_CILK_RUNTIME
  t->run();
  delete t;
  return;
#endif
  STAT_IDLE(reset());
  // TODO: LOG_ONLY(reset());
  LOG_BASIC(ENTER_LAUNCH);
  STAT_IDLE(enter_launch());
#ifdef TRACK_LOCALITY
  t->locality = thread::locality_range_t::init();
#endif
  t->set_instrategy(instrategy::ready_new());
  t->set_outstrategy(new outstrategy::end());
  add_thread(t);
  util::worker::the_group.run_worker0();
  STAT_IDLE(exit_launch());
  LOG_BASIC(EXIT_LAUNCH);
}

void destroy() {
  util::callback::output();
#ifndef USE_CILK_RUNTIME
  util::worker::the_group.destroy_threads();
#endif
  util::callback::destroy();
#ifdef USE_CILK_RUNTIME
  return;
#endif
  destroy_scheduler();
  destroy_basic();
}

/*---------------------------------------------------------------------*/
/* Auxiliary functions */

scheduler_p my_sched() {
  return scheduler::get_mine();
}

int get_nb_workers() {
  return util::worker::get_nb();
}

int get_my_id() {
  return (int) util::worker::get_my_id();
}

/*---------------------------------------------------------------------*/
/* Basic operations */

void add_thread(thread_p thread) {
  util::atomic::compiler_barrier(); // needed for message-based strategies
  my_sched()->add_thread(thread);
}

void add_dependency(thread_p thread1, thread_p thread2) {
  my_sched()->add_dependency(thread1, thread2);
}

outstrategy_p capture_outstrategy() {
  return my_sched()->capture_outstrategy();
}

void reuse_calling_thread() {
  my_sched()->reuse_calling_thread();
}

/*---------------------------------------------------------------------*/
/* Derived operations */

void join_with(thread_p thread, instrategy_p in) {
  thread->set_instrategy(in);
  thread->set_outstrategy(capture_outstrategy());
}

void continue_with(thread_p thread) {
  join_with(thread, instrategy::ready_new());
  add_thread(thread);
}

static void fork(thread_p thread, thread_p cont, instrategy_p in, outstrategy_p out) {
  thread->set_instrategy(in);
  thread->set_outstrategy(out);
  add_dependency(thread, cont);
  add_thread(thread);
}

void fork(thread_p thread, thread_p cont, branch_t branch) {
  fork(thread, cont, instrategy::ready_new(), new_forkjoin_outstrategy(branch));
}

void fork(thread_p thread, thread_p cont) {
  fork(thread, cont, UNDEFINED);
}

/*---------------------------------------------------------------------*/
/* Fixed-arity fork join */

void unary_fork_join(thread_p thread, thread_p cont, instrategy_p in) {
  join_with(cont, in);
  fork(thread, cont, SINGLE);
  add_thread(cont);
}

void unary_fork_join(thread_p thread, thread_p cont) {
  unary_fork_join(thread, cont, instrategy::unary_new());
}

void binary_fork_join(thread_p thread1, thread_p thread2, thread_p cont,
                                                  instrategy_p in) {
  join_with(cont, in);
  fork(thread2, cont, RIGHT);
  fork(thread1, cont, LEFT);
  add_thread(cont);
}

void binary_fork_join(thread_p thread1, thread_p thread2, thread_p cont) {
  binary_fork_join(thread1, thread2, cont, new_forkjoin_instrategy());
}

/*---------------------------------------------------------------------*/
/* Futures */

future_p create_future(thread_p thread, bool lazy) {
  if (lazy)
    thread->set_instrategy(instrategy::unary_new());
  else
    thread->set_instrategy(instrategy::ready_new());
  future_p future = new outstrategy::future_message(lazy);
  thread->set_outstrategy(future);
  add_thread(thread);
  return future;
}

void force_future(future_p future, thread_p cont, instrategy_p in) {
  if (future->thread_finished()) {
    continue_with(cont);
  } else {
    join_with(cont, in);
    future->add(cont);
    instrategy::delta(cont->in, cont, +1l);
  }
}

void force_future(future_p future, thread_p cont) {
  force_future(future, cont, instrategy::unary_new());
}

void delete_future(future_p future) {
  delete future;
}

/*---------------------------------------------------------------------*/
/* Async/finish */

void async(thread_p thread, thread_p cont) {
  fork(thread, cont);
}

void finish(thread_p thread, thread_p cont, instrategy_p in) {
  unary_fork_join(thread, cont, in);
}

void finish(thread_p thread, thread_p cont) {
  instrategy_p in = new instrategy::distributed(cont);
  finish(thread, cont, in);
}

/*---------------------------------------------------------------------*/

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace
