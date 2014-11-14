/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file stats.hpp
 * \brief Records statistics about the load balancing algorithm during
 * the execution of the program.
 *
 */

#ifndef _STATS_H_
#define _STATS_H_

#include <string>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <algorithm>

#include "classes.hpp"
#include "perworker.hpp"

namespace pasl {
namespace util {
namespace stats {


/***********************************************************************/

/*---------------------------------------------------------------------*/

typedef enum {
  THREAD_CREATE = 0,
  THREAD_EXEC, // redundant with previous one, on purpose
  THREAD_SEND,
  THREAD_REJECT,
  THREAD_RECOVER,
  THREAD_SPLIT,
  MSG_SEND,
  COMMUNICATE,
  INTERRUPT,
  ENTER_WAIT,
  THREAD_ALLOC,
  WORKER_LOCAL_ALLOC,
  THREAD_SEQUENTIAL,
  MEASURED_RUN,
  ESTIM_UPDATE,
  ESTIM_REPORT,
  // begin fencefree
  RESOLVE_JOIN,
  TRANSFER_ALL,
  ADD_WATCHLIST,
  REMOVE_WATCHLIST,
  RACE_RESOLUTION,
  WAITED_TO_COMPLETE_OFFER,
  WATCH,
  // end fencefree
  NB_STATS,
} stat_type_t;

static inline std::string name_of_type(stat_type_t type) {
  switch(type) {
    case THREAD_CREATE: return std::string("thread_create");
    case THREAD_EXEC: return std::string("thread_exec");
    case THREAD_SEND: return std::string("thread_send");
    case THREAD_REJECT: return std::string("thread_reject");
    case THREAD_RECOVER: return std::string("thread_recover");
    case THREAD_SPLIT: return std::string("thread_split");
    case MSG_SEND: return std::string("msg_send");
    case COMMUNICATE: return std::string("communicate");
    case INTERRUPT: return std::string("interrupt");
    case ENTER_WAIT: return std::string("enter_wait");
    case THREAD_ALLOC: return std::string("thread_alloc");
    case WORKER_LOCAL_ALLOC: return std::string("local_alloc");
    case THREAD_SEQUENTIAL: return std::string("thread_sequential");
    case MEASURED_RUN: return std::string("measured_run");
    case ESTIM_UPDATE: return std::string("estim_update");
    case ESTIM_REPORT: return std::string("estim_report");
    case RESOLVE_JOIN: return std::string("resolve_join");
    case TRANSFER_ALL: return std::string("transfer_all");
    case ADD_WATCHLIST: return std::string("add_watchlist");
    case REMOVE_WATCHLIST: return std::string("remove_watchlist");
    case RACE_RESOLUTION: return std::string("race_resolution");
    case WATCH: return std::string("watch");
    case WAITED_TO_COMPLETE_OFFER: return std::string("waited_to_complete_offer");
    default: return std::string("unknown");
  }
}

/*---------------------------------------------------------------------*/

class stats_data_t {
public:
  uint64_t counters[NB_STATS];
  double waiting_time;
  double sequential_time;
  double spinning_time;

public:
  stats_data_t();
  void reset();
};

/*---------------------------------------------------------------------*/

const microtime_t never = 0;

class stats_t;

class stats_private_t {
friend class stats_t;
private:
  stats_data_t data;
public:
  stats_private_t();
  void count(stat_type_t type);
  void add_to_sequential_time(double elapsed);
  void add_to_idle_time(double elapsed);
  void add_to_spinning_time(double elapsed);
};

/*---------------------------------------------------------------------*/


class stats_t {
private:
  typedef pasl::data::perworker::extra<stats_private_t> wi_stats_t;
  wi_stats_t stats;
  stats_data_t total_data;

  microtime_t launch_enter_time;
  microtime_t launch_exit_time;
  double launch_duration;

  // computed
  double total_idle_time;
  double relative_idle;
  double utilization;
  double relative_non_seq;
  double average_sequentialized;
  double total_spinning_time;

public:
  stats_t();
  ~stats_t();
  void init();
  void reset();
  void sum();
  void print(FILE* f);
  void print_idle(FILE* f);
  void dump(FILE* f);
  stats_private_t& get_my_stats();
  void enter_launch();
  void finished_launch();
  void exit_launch();
  bool is_launched();

  void add_to_idle_time(double elapsed);
  void add_to_spinning_time(double elapsed);

  // TODO: get rid of these functions by having the STAT macros to call get_my_stat
  void count(stat_type_t type);
  void add_to_sequential_time(double elapsed);
};

/*---------------------------------------------------------------------*/

extern stats_t the_stats;

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace


// usage: is STAT(enter_wait())
// usage: is STAT_COUNT(THREAD_SENT)

//! \todo rename macros to make clear that STAT_IDLE* are "or"

#ifdef STATS

#define STAT(call) pasl::util::stats::the_stats.call
#define STAT_COUNT(cst) pasl::util::stats::the_stats.count(pasl::util::stats::cst)
#define STAT_ONLY(code) code

#else

#define STAT(call)
#define STAT_COUNT(cst)
#define STAT_ONLY(code)

#endif

#if defined(STATS) || defined(STATS_IDLE)

#define STAT_IDLE(call) pasl::util::stats::the_stats.call
#define STAT_IDLE_ONLY(code) code

#else

#define STAT_IDLE(call)
#define STAT_IDLE_ONLY(code)

#endif



/***********************************************************************/


#endif /*! _STATS_H_ */
