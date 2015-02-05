/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file estimator.cpp
 * \brief Constant-estimator data structure
 *
 */

#include <fstream>
#include <map>
#ifndef NDEBUG
#include <unordered_set>
#endif

#include "logging.hpp"
#include "stats.hpp"
#include "cmdline.hpp"
#include "estimator.hpp"

/***********************************************************************/

namespace pasl {
namespace data {
namespace estimator {
  
static double local_ticks_per_microsec;

static void try_write_constants_to_file();
static void try_read_constants_from_file();

void init() {
  local_ticks_per_microsec = util::machine::cpu_frequency_ghz * 1000.;
  try_read_constants_from_file();
}

void destroy() {
  try_write_constants_to_file();
}
  
#if 1
void check_estimator_name(std::string name) {
}
#else
std::unordered_set<std::string> estimator_names;

void check_estimator_name(std::string name) {
  util::atomic::msg([&] {
    if (estimator_names.find(name) != estimator_names.end()) {
      std::cerr << "Warning: using duplicate name for estimator: " << name << std::endl;
    } else {
      estimator_names.insert(name);
    }
  });
}
#endif

/*---------------------------------------------------------------------*/
/* Reading and writing constants to file */

typedef std::map<std::string, double> constant_map_t;

// values of constants which are read from a file
static constant_map_t preloaded_constants;
// values of constants which are to be written to a file
static constant_map_t recorded_constants;

static void print_constant(FILE* out, std::string name, double cst) {
  fprintf(out,         "%s %lf\n", name.c_str(), cst);
}

static void parse_constant(char* buf, double& cst, std::string line) {
  sscanf(line.c_str(), "%s %lf", buf, &cst);
}

static std::string get_dflt_constant_path() {
  std::string executable = util::cmdline::name_of_my_executable();
  return executable + ".cst";
}

static std::string get_path_to_constants_file_from_cmdline(std::string flag) {
  std::string outfile;
  if (util::cmdline::parse_or_default_bool(flag, false, false))
    return get_dflt_constant_path();
  else
    return util::cmdline::parse_or_default_string(flag + "_in", "", false);
}

static void try_read_constants_from_file() {
  std::string infile_path = get_path_to_constants_file_from_cmdline("read_csts");
  if (infile_path == "")
    return;
  std::string cst_str;
  std::ifstream infile;
  infile.open (infile_path.c_str());
  while(! infile.eof()) {
    getline(infile, cst_str);
    if (cst_str == "")
      continue; // ignore trailing whitespace
    char buf[4096];
    double cst;
    parse_constant(buf, cst, cst_str);
    std::string name(buf);
    preloaded_constants[name] = cst;
  }
}

static void try_write_constants_to_file() {
  std::string outfile_path = get_path_to_constants_file_from_cmdline("write_csts");
  if (outfile_path == "")
    return;
  static FILE* outfile;
  outfile = fopen(outfile_path.c_str(), "w");
  constant_map_t::iterator it;
  for (it = recorded_constants.begin(); it != recorded_constants.end(); it++)
    print_constant(outfile, it->first, it->second);
  fclose(outfile);
}

/*---------------------------------------------------------------------*/
// common

void common::init() {
  LOG_ESTIM(new util::logging::estim_name_event_t(this, name));
}

void common::output() {
  recorded_constants[name] = get_constant();
}

void common::destroy() {
  
}

std::string common::get_name() {
  return name;
}

cost_type common::get_constant_or_pessimistic() {
  cost_type cst = get_constant();
  assert (cst != 0.);
  if (cst == cost::undefined)
    return cost::pessimistic;
  else
    return cst;
}

cost_type common::predict_impl(complexity_type comp) {
  // tiny complexity leads to tiny cost
  if (comp == complexity::tiny)
    return cost::tiny;
  
  // complexity shouldn't be undefined
  assert (comp >= 0);
  
  // compute the constant multiplied by the complexity
  cost_type cst = get_constant_or_pessimistic();
  return cst * ((double) comp);
}

cost_type common::predict(complexity_type comp) {
  cost_type t = predict_impl(comp);
  LOG_ESTIM(new util::logging::estim_predict_event_t(this, comp, t));
  return t;
}

uint64_t common::predict_nb_iterations() {
  double cst = get_constant_or_pessimistic();
  assert (cst != 0);
  double nb = (uint64_t) (sched::kappa / cst);
  if (nb <= 0)
    nb = (uint64_t) 1;
  return nb;
}

void common::log_update(cost_type new_cst) {
  LOG_CSTS(new util::logging::estim_update_event_t(this, new_cst));
}
  
void common::check() {
  check_estimator_name(name);
}

void common::report(complexity_type comp, cost_type elapsed_ticks) {
  double elapsed_time = elapsed_ticks / (double) local_ticks_per_microsec;
  cost_type measured_cst = elapsed_time / comp;
  LOG_ESTIM(new util::logging::estim_report_event_t(this, comp, elapsed_time, measured_cst));
  STAT_COUNT(ESTIM_REPORT);
  analyse(measured_cst);
}

/*---------------------------------------------------------------------*/
// disbtributed

void distributed::init() {
  common::init();
  shared_cst = cost::undefined;
  constant_map_t::iterator preloaded = preloaded_constants.find(common::name);
  if (preloaded != preloaded_constants.end())
    set_init_constant(preloaded->second);
}

void distributed::destroy() {
  common::destroy();
}
  
void distributed::output() {
  common::output();
}

cost_type distributed::get_constant() {
  cost_type cst = private_csts.mine();
  
  // if local constant is undefined, use shared cst
  if (cst == cost::undefined)
    return shared_cst;
  
  // (optional) if local constant is way above the shared cst, use shared cst
  // if (cst > max_decrease_factor * shared_cst)
  //   return shared_cst;
  
  // else return local constant
  return cst;
}

void distributed::set_init_constant(cost_type init_cst) {
  shared_cst = init_cst;
  init_constant_provided_flg = true;
}

bool distributed::init_constant_provided() {
  return init_constant_provided_flg;
}

void distributed::update_shared(cost_type new_cst) {
  shared_cst = new_cst;
  LOG_ONLY(log_update(new_cst));
  STAT_COUNT(ESTIM_UPDATE);
}

void distributed::update(cost_type new_cst) {
  // if decrease is significant, report it to the shared constant;
  // (note that the shared constant never increases)
  cost_type shared = shared_cst;
  if (shared == cost::undefined) {
    update_shared(new_cst);
  } else {
    cost_type min_shared_cst = shared / min_report_shared_factor;
    if (new_cst < min_shared_cst)
      update_shared(min_shared_cst);
  }
  // store the new constant locally in any case
  private_csts.mine() = new_cst;
}

void distributed::analyse(cost_type measured_cst) {
  cost_type cst = get_constant();
  if (cst == cost::undefined) {
    // handle the first measure without average
    update(measured_cst);
  } else {
    // compute weighted average
    update(((weighted_average_factor * cst) + measured_cst)
           / (weighted_average_factor + 1.0));
  }
}

bool distributed::constant_is_known() {
  return (shared_cst != cost::unknown);
}

  
} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/
