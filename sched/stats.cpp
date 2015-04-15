/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file stats.cpp
 *
 */

#include "stats.hpp"
#include "cmdline.hpp"

namespace pasl {
namespace util {
namespace stats {

/***********************************************************************/

volatile bool launch_finished;

/*---------------------------------------------------------------------*/

stats_data_t::stats_data_t() {
  reset();
}

void stats_data_t::reset() {
  waiting_time = 0.0;
  sequential_time = 0.0;
  spinning_time = 0.0;
  for (int i = 0; i < NB_STATS; i++)
    counters[i] = 0;
}

/*---------------------------------------------------------------------*/

stats_private_t::stats_private_t() { 
}

void stats_private_t::count(stat_type_t type) {
  data.counters[type]++;
}

void stats_private_t::add_to_sequential_time(double value) {
  data.sequential_time += value;
}

void stats_private_t::add_to_idle_time(double elapsed) {
  data.waiting_time += elapsed;
}

void stats_private_t::add_to_spinning_time(double elapsed) {
  data.spinning_time += elapsed;
}

/*---------------------------------------------------------------------*/

stats_t::stats_t() { 
  launch_enter_time = never;
  launch_exit_time = never;
}

stats_t::~stats_t() {
}

void stats_t::init() {

}

void stats_t::reset() {
  int64_t nb_workers = worker::get_nb();
  // later : stats.foreach
  for (int64_t id = worker::undef; id < nb_workers; id++) 
    stats[id].data.reset();
}

void stats_t::sum() {
  total_data.reset();
  int64_t nb_workers = worker::get_nb();
  for (int64_t id = worker::undef; id < nb_workers; id++) {
    stats_data_t& local_data = stats[id].data;
    total_data.waiting_time += local_data.waiting_time;
    total_data.sequential_time += local_data.sequential_time;
    for (/*stat_type_t*/ int stat_type = 0; stat_type < NB_STATS; stat_type++)
      total_data.counters[stat_type] += local_data.counters[stat_type];
    total_data.spinning_time += local_data.spinning_time;
  }
  double cumulated_time = launch_duration * nb_workers;
  total_idle_time = total_data.waiting_time;
  total_spinning_time = total_data.spinning_time;
  relative_idle = total_idle_time / cumulated_time; 
  utilization = 1.0 - relative_idle;
  relative_non_seq = 1.0 - total_data.sequential_time / cumulated_time; 
  uint64_t nb_measured_run = total_data.counters[MEASURED_RUN];
  if (nb_measured_run > 0)
    average_sequentialized = 1000000. * total_data.sequential_time / nb_measured_run; 
  else
    average_sequentialized = -1.;
}

// assumes sums have been computed
void stats_t::print_idle(FILE* f) {
  fprintf(f, "total_idle_time %.3lf\n", total_idle_time);
  fprintf(f, "utilization %.4lf\n", utilization);
}

void stats_t::print(FILE* f) {
  fprintf(f, "launch_duration\t%.3lf\n", launch_duration);
  // fprintf(f, "relative_idle_time\t%.4lf\n", relative_idle);
  fprintf(f, "utilization\t%.4lf\n", utilization);
  bool stats_light = cmdline::parse_or_default_bool("stats_light", true, false);
  if (! stats_light) {
    fprintf(f, "total_sequential\t%.3lf\n", total_data.sequential_time);
    fprintf(f, "average_sequential\t%.3lf\n", average_sequentialized);
    fprintf(f, "relative_non_seq\t%.4lf\n", relative_non_seq);
    fprintf(f, "total_spinning_time\t%lf\n", total_spinning_time);
    for (int i = 0; i < NB_STATS; i++)
      fprintf(f, "%s\t%ld\n", 
              name_of_type((stat_type_t) i).c_str(),
              (long)total_data.counters[i]);
  } else {
    const int nb_selected_stats = 3;
    int selected_stats[nb_selected_stats] = { 
      THREAD_SEND, THREAD_EXEC, THREAD_ALLOC };
    for (int k = 0; k < nb_selected_stats; k++) {
      int i = selected_stats[k];
      fprintf(f, "%s\t%ld\n", 
              name_of_type((stat_type_t) i).c_str(),
              (long)total_data.counters[i]);
    }
  }
}

void stats_t::dump(FILE* f) {
  sum();
  print(f);
}

stats_private_t& stats_t::get_my_stats() {
  return stats[worker::the_group.get_my_id_or_undef ()];
}


void stats_t::enter_launch() {
  launch_finished = false;
  launch_enter_time = microtime::now();
}

void stats_t::finished_launch() {
  launch_finished = true;
}


void stats_t::exit_launch() {
  // todo: might move to finished_launch
  assert (launch_enter_time != never);
  launch_exit_time = microtime::now();
  launch_duration = microtime::seconds(microtime::diff(launch_enter_time, launch_exit_time));
  launch_enter_time = never;
}

//! \todo: deprecated function
bool stats_t::is_launched() { 
  return (launch_enter_time != never);
}

void stats_t::count(stat_type_t type) {
  //if (!is_launched()) return;
  get_my_stats().count(type);
}

void stats_t::add_to_sequential_time(double value) {
  //if (!is_launched()) return;
  get_my_stats().add_to_sequential_time(value);
}

void stats_t::add_to_idle_time(double elapsed) {
  //if (!is_launched()) return;
  if (launch_finished) return; // TODO: should count idle time of processors still in wait phases (they should all be)
  get_my_stats().add_to_idle_time(elapsed);
}

void stats_t::add_to_spinning_time(double elapsed) {
  get_my_stats().add_to_spinning_time(elapsed);
}

/*---------------------------------------------------------------------*/

stats_t the_stats;

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace
