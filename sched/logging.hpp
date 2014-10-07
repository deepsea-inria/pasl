/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file logging.hpp
 * \brief Record logs about the load balancing algorithm during
 * the execution of the program, and dumps them at the end
 * in text format or in binary format.
 *
 */

#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <string>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <assert.h>

#include "perworker.hpp"
#include "classes.hpp"
#include "localityrange.hpp"

namespace pasl {
namespace util {
namespace logging {

/***********************************************************************/


/*---------------------------------------------------------------------*/

typedef enum {
  PHASES = 0,
  LOCALITY,
  COMM,
  THREADS,
  CSTS,
  ESTIMS,
  TRANSFER,
  STDWS,
  NUM_KIND_IDS,
} event_kind_t;

/*---------------------------------------------------------------------*/

typedef enum {
  ENTER_LAUNCH = 0,
  EXIT_LAUNCH,
  ENTER_ALGO,
  EXIT_ALGO,
  ENTER_WAIT,
  EXIT_WAIT,
  COMMUNICATE,
  INTERRUPT,
  ALGO_PHASE,
  LOCALITY_START,
  LOCALITY_STOP,
  THREAD_START,
  THREAD_STOP,
  THREAD_CREATE,
  THREAD_SCHEDULE,
  THREAD_POP,
  THREAD_SEND,
  THREAD_EXEC,
  THREAD_FINISH,
  THREAD_FORK,
  ESTIM_NAME,
  ESTIM_PREDICT,
  ESTIM_REPORT,
  ESTIM_UPDATE,
  STEAL_SUCCESS,
  STEAL_FAIL,
  STEAL_ABORT,
  NUM_TYPE_IDS,
} event_type_t;

static inline std::string name_of(event_type_t type) {
  switch(type) {
    case ENTER_LAUNCH:  return std::string("enter_launch ");
    case EXIT_LAUNCH:   return std::string("exit_launch  ");
    case ENTER_ALGO:  return std::string("enter_algo ");
    case EXIT_ALGO:   return std::string("exit_algo  ");
    case ENTER_WAIT:    return std::string("enter_wait   ");
    case EXIT_WAIT:     return std::string("exit_wait    ");
    case COMMUNICATE:   return std::string("communicate  ");
    case INTERRUPT:     return std::string("interrupt    ");
    case ALGO_PHASE:    return std::string("algo_phase   ");
    case LOCALITY_START:return std::string("local_start  ");
    case LOCALITY_STOP: return std::string("local_stop   ");
    case THREAD_CREATE:   return std::string("thread_create  ");
    case THREAD_POP:      return std::string("thread_pop     ");
    case THREAD_SCHEDULE: return std::string("thread_schedule");
    case THREAD_SEND:     return std::string("thread_send    ");
    case THREAD_EXEC:     return std::string("thread_exec    ");
    case THREAD_FINISH:   return std::string("thread_finish  ");
    case THREAD_FORK:   return std::string("thread_fork  ");
    case ESTIM_NAME:    return std::string("estim_name   ");
    case ESTIM_PREDICT: return std::string("estim_predict");
    case ESTIM_REPORT:  return std::string("estim_report ");
    case ESTIM_UPDATE:  return std::string("estim_update ");
    case STEAL_SUCCESS: return std::string("steal_success");
    case STEAL_FAIL:    return std::string("steal_fail   ");
    case STEAL_ABORT:   return std::string("steal_abort  ");
    default: assert(false);
  }
  return "noname"; // never happens
}

static inline event_kind_t kind_of_type(event_type_t type) {
  switch(type) {
    case ENTER_LAUNCH: return PHASES;
    case EXIT_LAUNCH: return PHASES;
    case ENTER_ALGO: return PHASES;
    case EXIT_ALGO: return PHASES;
    case ENTER_WAIT: return PHASES;
    case EXIT_WAIT: return PHASES;
    case COMMUNICATE: return COMM;
    case INTERRUPT: return COMM;
    case ALGO_PHASE: return PHASES;
    case LOCALITY_START:return LOCALITY;
    case LOCALITY_STOP: return LOCALITY;
    case THREAD_CREATE: return THREADS;
    case THREAD_POP: return THREADS;
    case THREAD_SCHEDULE: return THREADS;
    case THREAD_SEND: return TRANSFER;
    case THREAD_EXEC: return THREADS;
    case THREAD_FINISH: return THREADS;
    case THREAD_FORK: return THREADS;
    case ESTIM_NAME: return ESTIMS;
    case ESTIM_PREDICT: return ESTIMS;
    case ESTIM_REPORT: return ESTIMS;
    case ESTIM_UPDATE: return CSTS;
    case STEAL_SUCCESS: return STDWS;
    case STEAL_FAIL: return STDWS;
    case STEAL_ABORT: return STDWS;
    default: assert(false);
  }
  return PHASES; // never happens
}

/*---------------------------------------------------------------------*/
class event_t_comparison_t;

class event_t {
private:
  int64_t id;
  double time;

public:

  event_t();

  virtual event_type_t get_type () = 0;

  virtual std::string get_name () = 0;

  virtual void print_byte_descr(FILE* f);

  virtual void print_text_descr(FILE* f);

  virtual void record (microtime_t basetime);

  virtual void print_byte_header (FILE* f);

  virtual void print_byte (FILE* f);

  virtual void print_text_header (FILE* f);

  virtual void print_text (FILE* f);

  double get_time();
};

typedef event_t* event_p;

typedef std::vector<event_p> events_t;

/*---------------------------------------------------------------------*/

class recorder_t {
private:
  bool real_time;
  bool text_mode;
  bool tracking[NUM_KIND_IDS];

  // LATER: this vector should use the local_allocator
  
  typedef data::perworker::extra<events_t> wi_events_t;
  wi_events_t events_for; 
  events_t all_events;
  microtime_t basetime;

private:
  events_t& get_my_events();

public:

  recorder_t();
  
  ~ recorder_t();
  
  void init();
  
  void destroy();

  void set_tracking_all(bool state);

  bool is_tracked_kind(event_kind_t kind);

  bool is_tracked(event_type_t type);

  void add_nocheck(event_p event);

  void add(event_p event);

  void merge_and_sort();

  void dump_byte_to (FILE* f);

  void dump_text_to (FILE* f);

  void dump_byte ();

  void dump_text ();

  void output ();

};


/*---------------------------------------------------------------------*/
// LATER: move definitions below into the CPP file.

class basic_event_t : public event_t {
protected:
  event_type_t type;
public:
  basic_event_t() {} // need to initialize type
  basic_event_t(event_type_t type) : type(type) {}
  virtual event_type_t get_type () { return type; }
  virtual std::string get_name () { return name_of(type); }
};

/*---------------------------------------------------------------------*/

class thread_event_t : public basic_event_t {
protected:
  sched::thread_p thread;
public:
  thread_event_t(event_type_t type, sched::thread_p thread) 
    : basic_event_t(type), thread(thread) {}
  void print_byte_descr(FILE* f);
  virtual void print_text_descr(FILE* f);
};

/*---------------------------------------------------------------------*/

class thread_fork_event_t : public thread_event_t {
protected:
  sched::thread_p threadL, threadR;
public:
  thread_fork_event_t(event_type_t type, sched::thread_p thread, sched::thread_p threadL, sched::thread_p threadR) 
    : thread_event_t(type, thread), threadL(threadL), threadR(threadR) {}
  void print_byte_descr(FILE* f);
  virtual void print_text_descr(FILE* f);
};

/*---------------------------------------------------------------------*/

class locality_event_t : public basic_event_t {
protected:
  pasl::data::locality_t pos;
public:
  //! type should be LOCALITY_START or LOCALITY_STOP
  locality_event_t(event_type_t type, pasl::data::locality_t pos)
    : basic_event_t(type), pos(pos) {}
  void print_byte_descr(FILE* f);
  virtual void print_text_descr(FILE* f);
};

/*---------------------------------------------------------------------*/

class interrupt_event_t : public basic_event_t {
protected:
  double elapsed;
public:
  interrupt_event_t(double elapsed) 
    : basic_event_t(INTERRUPT), elapsed(elapsed) {}
  void print_byte_descr(FILE* f);
  void print_text_descr(FILE* f);
};

/*---------------------------------------------------------------------*/

class estim_event_t : public basic_event_t {
protected:
  void* estim;
public:
  estim_event_t(void* estim, event_type_t type) 
    : // basic_event_t(type),  // TODO: why are we not setting the type here??
    estim(estim) {
  }
};

class estim_name_event_t : public estim_event_t {
private:
  std::string name;
public:
  estim_name_event_t(void* estim, std::string name) 
    : estim_event_t(estim, ESTIM_NAME), name(name) {
    this->type = ESTIM_NAME; }
  void print_byte_descr(FILE* f);
  void print_text_descr(FILE* f);
};

class estim_report_event_t : public estim_event_t {
private:
  uint64_t comp;
  double elapsed;
  double newcst;
public:
  estim_report_event_t(void* estim, uint64_t comp, double elapsed, double newcst) 
    : estim_event_t(estim, ESTIM_REPORT), 
      comp(comp), elapsed(elapsed), newcst(newcst) {
    this->type = ESTIM_REPORT; }
  void print_byte_descr(FILE* f);
  void print_text_descr(FILE* f);
};

class estim_update_event_t : public estim_event_t {
private:
  double newcst;
public:
  estim_update_event_t(void* estim, double newcst) 
    : estim_event_t(estim, ESTIM_UPDATE), newcst(newcst) {
      this->type = ESTIM_UPDATE;}
  void print_byte_descr(FILE* f);
  void print_text_descr(FILE* f);
};

class estim_predict_event_t : public estim_event_t {
private:
  int64_t comp;
  double time;
public:
  estim_predict_event_t(void* estim, int64_t comp, double time) 
    : estim_event_t(estim, ESTIM_PREDICT), comp(comp), time(time) {
    this->type = ESTIM_PREDICT;}
  void print_byte_descr(FILE* f);
  void print_text_descr(FILE* f);
};


/*---------------------------------------------------------------------*/

extern recorder_t the_recorder;

void output ();

void log_event(event_p event);

bool is_tracked_kind(event_kind_t kind);

void log_basic(event_type_t type);

void log_thread(event_type_t type, sched::thread_p thread);

void log_thread_fork(event_type_t type, sched::thread_p threadP, sched::thread_p threadL, sched::thread_p threadR);

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#ifdef LOGGING

#define LOG_EVENT(event_kind, event) if (pasl::util::logging::is_tracked_kind(pasl::util::logging::event_kind)) pasl::util::logging::log_event(event)
#define LOG_BASIC(type) pasl::util::logging::log_basic(pasl::util::logging::type)
#define LOG_THREAD(type, thread) pasl::util::logging::log_thread(pasl::util::logging::type, thread)
#define LOG_THREAD_FORK(thread, threadL, threadR) pasl::util::logging::log_thread_fork(pasl::util::logging::THREAD_FORK, thread, threadL, threadR)
#define LOG_ESTIM(event) LOG_EVENT(ESTIMS, event)
#define LOG_CSTS(event) LOG_EVENT(CSTS, event)
#define LOG_STDWS(event) LOG_EVENT(STDWS, event)
#define LOG_ONLY(code) code

#else

#define LOG_EVENT(event_kind, event) 
#define LOG_BASIC(event_type) 
#define LOG_THREAD(event_type, thread) 
#define LOG_THREAD_FORK(thread, threadL, threadR) 
#define LOG_ESTIM(event) 
#define LOG_CSTS(event) 
#define LOG_STDWS(event) 
#define LOG_ONLY(code)

#endif 



/***********************************************************************/


#endif /*! _LOGGING_H_ */


