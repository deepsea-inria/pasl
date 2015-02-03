#include <iostream>  
#include <math.h>
#include <vector>
#include <utility>
#include <string>
#include <assert.h>
#include "ticks.hpp"
#include "estimator.hpp"
#include "native.hpp"
#include "benchmark.hpp"

#ifndef _PASL_SCHED_GRANULARITY_H_
#define _PASL_SCHED_GRANULARITY_H_

/***********************************************************************/

namespace pasl {
namespace sched {
namespace granularity {

/*---------------------------------------------------------------------*/
/* PASL routines */

// thread-safe print routine
template <class Print_fct>
void msg(const Print_fct& print_fct) {
//  acquire_print_lock();
  print_fct();
//  release_print_lock();
}

// to be used to terminate the program on a specific error condition
template <class Print_fct>
void fatal(const Print_fct& print_fct) {
  msg(print_fct);
  assert(false);
  std::exit(-1);
}

// to be used to terminate the program on entry to a region of unfinished code
void todo() {
  fatal([] { std::cout << "TODO" << std::endl; });
}

#ifdef LOGGING
static
void log_granularity_control_mismatch() {
  // emit log message
}
#endif

template <class Item>
Item* new_array(long n) {
  Item* p = (Item*)malloc(n * sizeof(Item));
  if (p == nullptr)
    fatal([] { std::cout << "array allocation failed"; });
  return p;
}

/*---------------------------------------------------------------------*/
/* Complexity measure */

// type of integer values returned by complexity measures
using cmeasure_type = data::estimator::complexity_type;

// a tiny complexity measure forces sequential exec
constexpr cmeasure_type tiny      = -1l;
// an undefined complexity measure deactivates the estimator
constexpr cmeasure_type undefined = -2l;

constexpr double average_coefficient      = 8;
constexpr double shared_coefficient = 2;

/*---------------------------------------------------------------------*/
/* Placeholder for the constant estimator data structure */

/* TODO: make the interface of existing estimator structure by the
 * one given below.
 * Find old implementation of estimator in estimator.hpp/cpp
 */

// cost metric expressed in microseconds
using cost_type = double;
using namespace pasl::data;
typedef pasl::data::estimator::distributed estimator_m;
                           
pasl::data::perworker::cell<int> unique_estimator_id;

// cutoff expressed in microseconds
cost_type kappa = 2000.0;

// constant estimator data structure
class constant_estimator {
private:
  static std::atomic<cost_type> shared_constant;// = std::atomic<cost_type>(undefined);
  pasl::data::perworker::array<cost_type> local_constants;

  std::string name;

  static std::string uniqify(std::string name) {
    int x = unique_estimator_id.mine();
    unique_estimator_id.mine() = x+1;
    return name + "<" + std::to_string(x) + ">";
  }

  cost_type get_constant() {
    cost_type result = local_constants.mine();

    if (result == undefined) {
      return shared_constant.load();
    }

    return result;
  }

  static cost_type updated(cost_type estimated, cost_type reported) {
    return (estimated * average_coefficient + reported) / (average_coefficient + 1); 
    // TODO: make better function, which relates on exponents
  }

public:

  constant_estimator(std::string name) {
    this->name = uniqify(name);
  }

  void set_init_constant(cost_type init_constant) {
    shared_constant = init_constant;
    local_constants.init(undefined);
  }

  void report(cmeasure_type m, cost_type elapsed) {
    cost_type reported = elapsed / m;

    cost_type estimated = get_constant();

    pasl::worker_id_t my_id = pasl::util::worker::get_my_id();

    if (estimated == undefined) {
      shared_constant = reported;
      local_constants[my_id] = reported;
    } else {
      double updated_value = updated(estimated, reported);
      local_constants[my_id] = updated_value;
      double value = shared_constant.load();
      if (updated_value * shared_coefficient < value) {
        shared_constant = 2 * value / (1 + shared_coefficient); 
        // TODO: I think, the average will work better, it's like average.
      }
    }
  }

  cost_type predict(cmeasure_type m) {
    if (m == tiny) {
      return tiny;
    }
    cost_type constant = get_constant();
    if (constant == undefined) {
      return undefined;
    }
    return m * constant;
  }                          

  long predict_nb_iterations() {
    cost_type constant = get_constant();
    long nb = (long) (kappa / constant);

    return nb <= 0 ? 1 : nb;
  }

  std::string get_name() const {
    return name;
  }

  cost_type get_shared_constant() const {
    return shared_constant.load();
  }
};

std::atomic<cost_type> constant_estimator::shared_constant(undefined);
/*---------------------------------------------------------------------*/
/* Timer routines */

// TODO: Now, this works in ticks, not seconds| Probably 1000 ticks per second.
static
double now () {
  return (double)getticks();
}

static
double since(double start) {
  return elapsed(getticks(), (pasl::util::ticks::ticks_t)start);
}                               

/*---------------------------------------------------------------------*/
/* Granularity controller */

class control {
public:
  estimator_m estimator;

  control(std::string name = "") : estimator(name) { }

  estimator_m& get_estimator() {
    return estimator;
  }

  bool with_estimator() {
    return false;
  }

  void initialize(double init_cst) {
  }

  void initialize(double init_cst, int estimations_nb) {
  }

  void set(std::string policy_arg) {
  }
};

class control_with_estimator : public control {
public:                                             

  control_with_estimator(std::string name = "" ): control(name) {}

  void initialize(double init_cst) {
    estimator.set_init_constant(init_cst);
  }

  void initialize(double init_cst, int estimations_nb) {
    if (estimations_nb == 0)
     estimator.set_init_constant(init_cst);
    estimator.set_minimal_estimations_nb(estimations_nb);
  }

  bool with_estimator() {
    return true;
  }
};

class control_by_force_parallel : public control {
public:
  control_by_force_parallel(std::string name = "") : control(name) { }
};

class control_by_force_sequential : public control {
public:
  control_by_force_sequential(std::string name = "") : control(name) { }
};

class control_by_cutoff_without_reporting : public control {
public:
  control_by_cutoff_without_reporting(std::string name = "") : control(name) { }
};

class control_by_cutoff_with_reporting : public control_with_estimator {
public:
  control_by_cutoff_with_reporting(std::string name = "") : control_with_estimator(name) { }
};

class control_by_prediction : public control_with_estimator {
public:
  control_by_prediction(std::string name = "") : control_with_estimator(name) { }
};

class control_by_cmdline : public control_with_estimator {
public:
  using policy_type = enum {
    By_force_parallel,
    By_force_sequential,
    By_cutoff_without_reporting,
    By_cutoff_with_reporting,
    By_prediction
  };

  policy_type policy;

  control_by_force_parallel cbfp;
  control_by_force_sequential cbfs;
  control_by_cutoff_without_reporting cbcwor;
  control_by_cutoff_with_reporting cbcwtr;
  control_by_prediction cbp;
                                                                         
  control_by_cmdline(std::string name = ""): control_with_estimator(name),
                                             policy(By_prediction), 
                                             cbfp(name), cbfs(name), 
                                             cbcwor(name), cbcwtr(name), 
                                             cbp(name) {; } 

  // to be called by a callback in the PASL runtime
  void set(std::string policy_arg) {
    if (policy_arg == "by_force_parallel")
      policy = By_force_parallel;
    else if (policy_arg == "by_force_sequential")
      policy = By_force_sequential;
    else if (policy_arg == "by_cutoff_without_reporting")
      policy = By_cutoff_without_reporting;
    else if (policy_arg == "by_cutoff_with_reporting")
      policy = By_cutoff_with_reporting;
    else if (policy_arg == "by_prediction")
      policy = By_prediction;
    else
      fatal([&] { std::cout << "bogus policy " << policy_arg << std::endl; });
  }

  policy_type get() const {
    return policy;
  }

  bool with_estimator() {
    switch (get()) {
      case By_cutoff_with_reporting:
        return true;
      case By_prediction:
        return true;
      default:
        return false;
    }
  }

  estimator_m& get_estimator() {
    switch (get()) {
      case By_cutoff_with_reporting:
        return cbcwtr.get_estimator();
      case By_prediction:
        return cbp.get_estimator();
      default:
        return estimator;
    }
  }

  void initialize(double init_cst) {
    cbcwtr.get_estimator().set_init_constant(init_cst);
    cbp.get_estimator().set_init_constant(init_cst);
  }

  void initialize(double init_cst, int estimations_nb) {
    cbcwtr.get_estimator().set_init_constant(init_cst);
    cbp.get_estimator().set_init_constant(init_cst);
    cbp.get_estimator().set_minimal_estimations_nb(estimations_nb);
  }
};

/*---------------------------------------------------------------------*/
/* Dynamics of granularity-control statements */

// names of configurations supported by the granularity controller
using execmode_type = enum {
  Force_parallel,
  Force_sequential,
  Sequential,
  Parallel,
  Unknown
};

class dynidentifier {
private:
  execmode_type bk;
public:
  dynidentifier()
  : bk(Parallel) {};
  
  execmode_type& back() {
    return bk;
  }
  
  template <class Block_fct>
  void block(execmode_type x, const Block_fct& f) {
    execmode_type tmp = bk;
    bk = x;
    f();
    bk = tmp;
  }
};

// `p` configuration of caller; `c` callee
static
execmode_type execmode_combine(execmode_type p, execmode_type c) {
  // callee gives priority to Force*
  if (c == Force_parallel || c == Force_sequential)
    return c;
  // callee gives priority to caller when caller is Sequential
  if (p == Sequential) {
    #ifdef LOGGING
    if (c == Parallel) // report bogus predictions
      log_granularity_control_mismatch();
    #endif
    return Sequential;
  }
  // otherwise, callee takes priority
  return c;
}

// current configuration of the running thread;
// todo: to be stored in perworker memory
perworker::cell<dynidentifier> execmode;
             
execmode_type& my_execmode() {
  return execmode.mine().back();
}

template <class Body_fct>
void cstmt_base(execmode_type c, const Body_fct& body_fct) {
  execmode_type p = my_execmode();
  execmode_type e = execmode_combine(p, c);
  execmode.mine().block(e, body_fct);
}

template <class Seq_body_fct>
void cstmt_base_with_reporting(cmeasure_type m, Seq_body_fct& seq_body_fct, estimator_m& estimator) {
  cost_type start = now();
  execmode.mine().block(Sequential, seq_body_fct);
  cost_type elapsed = since(start);
  estimator.report(m, elapsed);
}

template <class Seq_body_fct>
void cstmt_base_with_reporting_unknown(cmeasure_type m, Seq_body_fct& seq_body_fct, estimator_m& estimator) {
  cost_type start = now();
  execmode.mine().block(Unknown, seq_body_fct);
  cost_type elapsed = since(start);
  if (!estimator.constant_is_known() || estimator.can_predict_unknown()) {
//    "Reported"l
    estimator.report(m, elapsed);
  }
}

template <
  class Control,
  class Complexity_measure_fct,
  class Body_fct
>
void cstmt_report(Control& contr,
           const Complexity_measure_fct& complexity_measure_fct,
           const Body_fct& body_fct) {
/*  execmode_type mode = my_execmode();
  if (contr.with_estimator() && mode != Sequential && mode != Force_sequential) {
    estimator_m& estimator = contr.get_estimator();
    cmeasure_type m = complexity_measure_fct();
    estimator.set_predict_unknown(true);
//    cstmt_base_with_reporting(m, seq_body_fct, estimator);
    cost_type start = now();
    execmode.mine().block(mode, body_fct);
    cost_type elapsed = since(start);
    if (estimator.can_predict_unknown()) {
      estimator.report(m, elapsed);
    }
  } else {
    body_fct();
  }  */
  body_fct();
}


template <
  class Cutoff_fct,
  class Complexity_measure_fct,
  class Par_body_fct
>
void cstmt(control& contr,
           const Cutoff_fct&,
           const Complexity_measure_fct&,
           const Par_body_fct& par_body_fct) {
  cstmt_base(Force_parallel, par_body_fct);
}

template <class Par_body_fct>
void cstmt(control_by_force_parallel&, const Par_body_fct& par_body_fct) {
  cstmt_base(Force_parallel, par_body_fct);
}

// same as above but accepts all arguments to support general case
template <
  class Cutoff_fct,
  class Complexity_measure_fct,
  class Par_body_fct,
  class Seq_body_fct
>
void cstmt(control_by_force_parallel& contr,
           const Cutoff_fct&,
           const Complexity_measure_fct&,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct&) {
  cstmt(contr, par_body_fct);
}

template <class Seq_body_fct>
void cstmt(control_by_force_sequential&, const Seq_body_fct& seq_body_fct) {
  cstmt_base(Force_sequential, seq_body_fct);
}

// same as above but accepts all arguments to support general case
template <
  class Cutoff_fct,
  class Complexity_measure_fct,
  class Par_body_fct,
  class Seq_body_fct
>
void cstmt(control_by_force_sequential& contr,
           const Cutoff_fct&,
           const Complexity_measure_fct&,
           const Par_body_fct&,
           const Seq_body_fct& seq_body_fct) {
  cstmt(contr, seq_body_fct);
}

template <
  class Cutoff_fct,
  class Par_body_fct
>
void cstmt(control_by_cutoff_without_reporting&,
           const Cutoff_fct& cutoff_fct,
           const Par_body_fct& par_body_fct) {
  execmode_type c = (cutoff_fct()) ? Sequential : Parallel;
  cstmt_base(c, par_body_fct);
}

template <
  class Cutoff_fct,
  class Par_body_fct,
  class Seq_body_fct
>
void cstmt(control_by_cutoff_without_reporting&,
           const Cutoff_fct& cutoff_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
  execmode_type c = (cutoff_fct()) ? Sequential : Parallel;
  if (c == Sequential)
    cstmt_base(Sequential, seq_body_fct);
  else
    cstmt_base(Parallel, par_body_fct);
}

// same as above but accepts all arguments to support general case
template <
  class Cutoff_fct,
  class Complexity_measure_fct,
  class Par_body_fct,
  class Seq_body_fct
>
void cstmt(control_by_cutoff_without_reporting& contr,
           const Cutoff_fct& cutoff_fct,
           const Complexity_measure_fct&,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
  cstmt(contr, cutoff_fct, par_body_fct, seq_body_fct);
}

template <
  class Cutoff_fct,
  class Complexity_measure_fct,
  class Par_body_fct,
  class Seq_body_fct
>
void cstmt(control_by_cutoff_with_reporting& contr,
           const Cutoff_fct& cutoff_fct,
           const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
  estimator_m& estimator = contr.get_estimator();
  execmode_type c = (cutoff_fct()) ? Sequential : Parallel;
  if (c == Sequential) {
    cmeasure_type m = complexity_measure_fct();
    cstmt_base_with_reporting(m, seq_body_fct, estimator);
  } else {
    cstmt_base(Parallel, par_body_fct);
  }
}

template <
  class Complexity_measure_fct,
  class Par_body_fct,
  class Seq_body_fct
>
void cstmt(control_by_prediction& contr,
           const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
  execmode_type mode = my_execmode();  if (mode == Sequential || mode == Force_sequential) {
    cstmt_base(Force_sequential, seq_body_fct);
    return;
  }

  estimator_m& estimator = contr.get_estimator();
  cmeasure_type m = complexity_measure_fct();

  execmode_type c;
//  if (my_execmode() == Sequential || my_execmode() == Force_sequential)
  if (m == tiny)
    c = Sequential;
  else if (m == undefined)
    c = Parallel;
  else
    c = estimator.constant_is_known() ? ((estimator.predict(m) <= kappa) ? Sequential : Parallel) : Unknown;
  if (c == Sequential) {
//    if (my_execmode() != Sequential && my_execmode() != Force_sequential)
    cstmt_base_with_reporting(m, seq_body_fct, estimator);
//    else
//      cstmt_base(Force_sequential, seq_body_fct);
//    cstmt_base(Sequential, seq_body_fct);
  } else if (c == Unknown) {
    cstmt_base_with_reporting_unknown(m, par_body_fct, estimator);
  } else if (c == Parallel) {
    estimator.set_predict_unknown(false);
    cstmt_base(Parallel, par_body_fct);
  }
}

template <
  class Complexity_measure_fct,
  class Par_body_fct
>
void cstmt(control_by_prediction& contr,
           const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct) {
  cstmt(contr, complexity_measure_fct, par_body_fct, par_body_fct);
}

// same as above but accepts all arguments to support general case
template <
  class Cutoff_fct,
  class Complexity_measure_fct,
  class Par_body_fct,
  class Seq_body_fct
>
void cstmt(control_by_prediction& contr,
           const Cutoff_fct&,
           const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
  cstmt(contr, complexity_measure_fct, par_body_fct, seq_body_fct);
}

template <
  class Cutoff_fct,
  class Complexity_measure_fct,
  class Par_body_fct,
  class Seq_body_fct
>
void cstmt(control_by_cmdline& contr,
           const Cutoff_fct& cutoff_fct,
           const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
  using cmd = control_by_cmdline;
  switch (contr.get()) {
    case control_by_cmdline::policy_type::By_force_parallel:
      cstmt(contr.cbfp, par_body_fct);
      break;
    case control_by_cmdline::policy_type::By_force_sequential:
      cstmt(contr.cbfs, seq_body_fct);
      break;
    case control_by_cmdline::policy_type::By_cutoff_without_reporting:
      cstmt(contr.cbcwor, cutoff_fct, par_body_fct, seq_body_fct);
      break;
    case control_by_cmdline::policy_type::By_cutoff_with_reporting:
      cstmt(contr.cbcwtr, cutoff_fct, complexity_measure_fct, 
            par_body_fct, seq_body_fct);
      break;
    case control_by_cmdline::policy_type::By_prediction:
      cstmt(contr.cbp, complexity_measure_fct, par_body_fct, seq_body_fct);
      break;
    default:
      std::cerr << "Wrong policy\n";
  }
  //cmd::policy_type policy = contr.get();
  // todo
}

/*---------------------------------------------------------------------*/
/* Granularity-control enriched thread DAG */

/* Currently, the `native` namespace lives in `native.hpp`.
 */

/* TODO: We will move this `fork2` and the various `cstmt`
 * classes to a new namespace called `pasl::controlled`.
 */

// spawn parallel threads; with granularity control
template <class Body_fct1, class Body_fct2>
void fork2(const Body_fct1& f1, const Body_fct2& f2) {
  execmode_type mode = my_execmode();
  if (mode == Sequential ||  
      mode == Force_sequential || mode == Unknown) {
    f1();  // sequentialize
    f2();
  } else {
    pasl::sched::native::fork2([&mode,&f1] { execmode.mine().block(mode, f1); },
                               [&mode,&f2] { execmode.mine().block(mode, f2); });
  }
}


/*---------------------------------------------------------------------*/
/* Parallel for */

template <class Granularity_control_policy>
class loop_control {
public:
  Granularity_control_policy* gcpolicy;

  loop_control(std::string name = "anonloop") {
      gcpolicy = new Granularity_control_policy(name);
  }

  loop_control(Granularity_control_policy* _gcpolicy) {
    gcpolicy = _gcpolicy;
}

  void initialize(double init_cst) {
    gcpolicy->initialize(init_cst);
  }

  void initialize(double init_cst, int estimations_nb) {
    gcpolicy->initialize(init_cst, estimations_nb);
  }

  void set(std::string policy_arg) {
    gcpolicy->set(policy_arg);
  }

};

template <class Granularity_control_policy>
class loop_by_eager_binary_splitting : public loop_control<Granularity_control_policy> {
public:
  /*loop_by_eager_binary_splitting(std::string name = "anonloop"): loop_control<Granularity_control_policy>(name) {}

  loop_by_eager_binary_splitting(Granularity_control_policy* gcpolicy): loop_control<Granularity_control_policy>(gcpolicy) {}*/
    using loop_control<Granularity_control_policy>::loop_control;

};

template <class Granularity_control_policy>
class loop_by_lazy_binary_splitting : public loop_control<Granularity_control_policy> {
public:
    using loop_control<Granularity_control_policy>::loop_control;
};

template <class Granularity_control_policy>
class loop_by_lazy_binary_splitting_scheduling : public loop_control<Granularity_control_policy> {
public:
    using loop_control<Granularity_control_policy>::loop_control;
};

template <class Granularity_control_policy>
class loop_by_binary_search_splitting : public loop_control<Granularity_control_policy> {
public:
  loop_by_binary_search_splitting(std::string name = "anonloop"): loop_control<Granularity_control_policy>(name) {}

  loop_by_binary_search_splitting(Granularity_control_policy* gcpolicy): loop_control<Granularity_control_policy>(gcpolicy) {}
};

template <class Granularity_control_policy>
class loop_by_lazy_binary_search_splitting : public loop_control<Granularity_control_policy> {
public:
  loop_by_lazy_binary_search_splitting(std::string name = "anonloop"): loop_control<Granularity_control_policy>(name) {}

  loop_by_lazy_binary_search_splitting(Granularity_control_policy* gcpolicy): loop_control<Granularity_control_policy>(gcpolicy) {}
};

template <class Loop_control>
class loop_control_with_sampling {
public:
  Loop_control lcontrol;

  loop_control_with_sampling(std::string name = "anonloop"): lcontrol(name) {}

  void initialize(double init_cst) {
    lcontrol->initialize(init_cst);
  }

  void initialize(double init_cst, int estimations_nb) {
    lcontrol->initialize(init_cst, estimations_nb);
  }

void set(std::string policy_arg) {
    lcontrol->set(policy_arg);
  }
};

template <
  class Loop_control_policy,
  class Number,
  class Body
>
void parallel_for(Loop_control_policy& lpalgo,
                  Number lo, Number hi, const Body& body) {
  auto loop_compl_fct = [] (Number lo, Number hi) { return hi-lo; };
  auto loop_cutoff_fct = [] (Number lo, Number hi) {
    todo();
    return 0;
  };                                 
  parallel_for(lpalgo, loop_cutoff_fct, loop_compl_fct, lo, hi, body);
}

template <
  class Granularity_control_policy,
  class Loop_cutoff_fct,
  class Loop_complexity_measure_fct,
  class Number,
  class Body
>
void parallel_for(loop_by_eager_binary_splitting<Granularity_control_policy>& lpalgo,
                  const Loop_cutoff_fct& loop_cutoff_fct,
                  const Loop_complexity_measure_fct& loop_compl_fct,
                  Number lo, Number hi, const Body& body) {
  auto seq_fct = [&] {
    for (Number i = lo; i < hi; i++)
      body(i);
  };
  if (hi - lo < 2
     || (!lpalgo.gcpolicy->with_estimator() && loop_cutoff_fct(lo, hi))
     || (lpalgo.gcpolicy->with_estimator() && lpalgo.gcpolicy->get_estimator().constant_is_known()
         && lpalgo.gcpolicy->get_estimator().predict(loop_compl_fct(lo, hi)) <= kappa)) {
    cstmt_report(*lpalgo.gcpolicy, [&] {return loop_compl_fct(lo, hi);}, seq_fct);
//    cstmt(*lpalgo.gcpolicy, [&] {return loop_cutoff_fct(lo, hi); }, [&] { return loop_compl_fct(lo, hi); }, seq_fct, seq_fct);
  }else {
    auto cutoff_fct = [&] {
      return loop_cutoff_fct(lo, hi);
    };
    auto compl_fct = [&] {
      return loop_compl_fct(lo, hi);
    };

    Number mid = (lo + hi) / 2;
    cstmt(*lpalgo.gcpolicy, cutoff_fct, compl_fct, 
          [&]{fork2(
            [&] {parallel_for(lpalgo, loop_cutoff_fct, loop_compl_fct, lo, mid, body);},
            [&] {parallel_for(lpalgo, loop_cutoff_fct, loop_compl_fct, mid, hi, body);}
          );},
          seq_fct
    );
  }
}

template <
  class Loop_cutoff_fct,
  class Number,
  class Body
>
void parallel_for(loop_by_eager_binary_splitting<control_by_cutoff_without_reporting>& lpalgo,
                  const Loop_cutoff_fct& loop_cutoff_fct,
                  Number lo, Number hi, const Body& body) {
  auto loop_compl_fct = [] (Number lo, Number hi) {
    todo();
    return 0;
  };

  parallel_for(lpalgo, loop_cutoff_fct, loop_compl_fct, lo, hi, body);}

template <
  class Loop_complexity_measure_fct,
  class Number,
  class Body
>
void parallel_for(loop_by_eager_binary_splitting<control_by_prediction>& lpalgo,
                  const Loop_complexity_measure_fct& loop_compl_fct,
                  Number lo, Number hi, const Body& body) {
  auto loop_cutoff_fct = [] (Number lo, Number hi) {
    todo();
    return false;
  };

  parallel_for(lpalgo, loop_cutoff_fct, loop_compl_fct, lo, hi, body);
}

template<
  class Granularity_control_policy,
  class Loop_cutoff_fct,
  class Loop_complexity_measure_fct,
  class Number
>
Number binary_search_estimated(Granularity_control_policy& gcpolicy,
                  const Loop_cutoff_fct& loop_cutoff_fct,
                  const Loop_complexity_measure_fct& loop_compl_fct,
                  Number lo, Number hi) {
    Number l = lo + 1;

    if (gcpolicy.with_estimator()) {
      estimator_m& estimator = gcpolicy.get_estimator();

      if (estimator.constant_is_known()) {	
        Number r = hi + 1;
        while (l < r - 1) {
          Number mid = (l + r) / 2;
          if (estimator.predict(loop_compl_fct(lo, mid)) <= kappa) {
            l = mid;
          } else {
            r = mid;
          }
        }
      }
    } else {
      Number r = hi;
      while (l < r - 1) {
        Number mid = (l + r) / 2;
        if (loop_cutoff_fct(lo, mid)) {
          l = mid;
        } else {
          r = mid;
        }
      }
    }

  return l;
}

template <
  class Granularity_control_policy,
  class Loop_cutoff_fct,
  class Loop_complexity_measure_fct,
  class Number,
  class Body
>
void parallel_for_binary_search(loop_by_binary_search_splitting<Granularity_control_policy>& lpalgo,
                  const Loop_cutoff_fct& loop_cutoff_fct,
                  const Loop_complexity_measure_fct& loop_compl_fct,
                  Number lo, Number hi, const Body& body) {
  auto seq_fct = [&] {
    for (Number i = lo; i < hi; i++)
      body(i);
  };
  if (hi - lo < 2
     || (!lpalgo.gcpolicy->with_estimator() && loop_cutoff_fct(lo, hi))
     || (lpalgo.gcpolicy->with_estimator() && lpalgo.gcpolicy->get_estimator().constant_is_known() && lpalgo.gcpolicy->get_estimator().predict(loop_compl_fct(lo, hi)) <= kappa)) {
    cstmt_report(*lpalgo.gcpolicy, [&] {return loop_compl_fct(lo, hi);}, seq_fct);
//    cstmt(*lpalgo.gcpolicy, [&] {return loop_cutoff_fct(lo, hi); }, [&] { return loop_compl_fct(lo, hi); }, seq_fct, seq_fct);
  } else {
    auto cutoff_fct = [&] {
      return loop_cutoff_fct(lo, hi);
    };
    auto compl_fct = [&] {
      return loop_compl_fct(lo, hi);
    };

    Number l = binary_search_estimated(*lpalgo.gcpolicy, loop_cutoff_fct, loop_compl_fct, lo, hi);

    auto inner_seq_fct = [&] {
      for (Number i = lo; i < l; i++)
        body(i);
    };

    if (l != hi) {
     cstmt(*lpalgo.gcpolicy, cutoff_fct, compl_fct, 
       [&]{fork2(
          [&] {cstmt(*lpalgo.gcpolicy, [&] { return true; }, [&] {return loop_compl_fct(lo, l);}, inner_seq_fct, inner_seq_fct);},
          [&] {parallel_for_binary_search(lpalgo, loop_cutoff_fct, loop_compl_fct, l, hi, body);}
        );},
        seq_fct
      );
    } else {
      cstmt_report(*lpalgo.gcpolicy, [&] {return loop_compl_fct(lo, l);}, inner_seq_fct);
    }
  }
}

template <
  class Granularity_control_policy,
  class Loop_cutoff_fct,
  class Loop_complexity_measure_fct,
  class Number,
  class Body
>
void parallel_for(loop_by_binary_search_splitting<Granularity_control_policy>& lpalgo,
                  const Loop_cutoff_fct& loop_cutoff_fct,
                  const Loop_complexity_measure_fct& loop_compl_fct,
                  Number lo, Number hi, const Body& body) {
  parallel_for_binary_search(lpalgo, loop_cutoff_fct, loop_compl_fct, lo, hi, body);
}

template <
  class Loop_cutoff_fct,
  class Loop_compl_fct,
  class Number,
  class Body
>
void parallel_for(loop_by_binary_search_splitting<control_by_prediction>& lpalgo,
                  const Loop_cutoff_fct& loop_cutoff_fct,
                  const Loop_compl_fct& loop_compl_fct,
                  Number lo, Number hi, const Body& body) {
  estimator_m& estimator = lpalgo.gcpolicy->get_estimator();
  auto loop_cutoff_new = [&] (Number l, Number r) {
    return estimator.predict(loop_compl_fct(l, r)) <= kappa;
  };

  parallel_for_binary_search(lpalgo, loop_cutoff_new, loop_compl_fct, lo, hi, body);
}

template <
  class Loop_control,
  class Loop_cutoff_fct,
  class Loop_complexity_measure_fct,
  class Number,
  class Body
>
void parallel_for(loop_control_with_sampling<Loop_control>& lpalgo,
                  const Loop_cutoff_fct& loop_cutoff_fct,
                  const Loop_complexity_measure_fct& loop_compl_fct,
                  Number lo, Number hi, const Body& body) {
  parallel_for(lpalgo.loop_control, loop_cutoff_fct, loop_compl_fct, lo, hi, body);
}

template <
  class Loop_control,
  class Loop_cutoff_fct,
  class Number,
  class Body
>
void parallel_for(loop_control_with_sampling<Loop_control>& lpalgo,
                  const Loop_cutoff_fct& loop_cutoff_fct,
                  Number lo, Number hi, const Body& body) {
  parallel_for(lpalgo.loop_control, loop_cutoff_fct, lo, hi, body);
}

template <
  class Loop_control,
  class Loop_complexity_measure_fct,
  class Number,
  class Body
>
void parallel_for(loop_control_with_sampling<Loop_control>& lpalgo,
                  const Loop_complexity_measure_fct& loop_compl_fct,
                  Number lo, Number hi, const Body& body, int sample_number) {
  auto loop_cutoff_fct = [] (Number lo, Number hi) {
    todo();
    return false;
  };

  estimator_m& estimator = lpalgo.loop_control.gcpolicy.get_estimator();

  for (int i = lo; i < std::min(lo + sample_number, hi); i++) {
    cmeasure_type m = loop_compl_fct(i, i + 1);
    auto iter_fct = [&] { body(i); };
    cstmt_base_with_reporting(m, iter_fct, estimator);
  }

  if (lo + sample_number < hi)
    parallel_for(lpalgo.loop_control, loop_cutoff_fct, loop_compl_fct, lo + sample_number, hi, body);
} 

//Parallel for lazy binary search splitting with control_by_cmdline
template <
 class Granularity_control_policy,
  class Loop_cutoff_fct,
  class Loop_complexity_measure_fct,
  class Number,
  class Body
>
void parallel_for_lazy_binary_search(loop_by_lazy_binary_search_splitting<Granularity_control_policy>& lpalgo,
                  const Loop_cutoff_fct& loop_cutoff_fct,
                  const Loop_complexity_measure_fct& loop_compl_fct,
                  std::vector<std::pair<Number, Number>>& split_positions, const Body& body) {
  std::pair<Number, Number> split = split_positions.back();
  split_positions.pop_back();

//  std::cerr << "Range: " << split.first << " " << split.second << " " << split_positions.size() << std::endl;

  auto inner_seq_fct = [&] {
    for (Number i = split.first; i < split.second; i++)
      body(i);
  };

  if (split_positions.size() != 0) {
    fork2(
      [&] {cstmt(*lpalgo.gcpolicy, [&] { return true; }, [&] {return loop_compl_fct(split.first, split.second);}, inner_seq_fct, inner_seq_fct);},
      [&] {parallel_for_lazy_binary_search(lpalgo, loop_cutoff_fct, loop_compl_fct, split_positions, body);}
    );
  } else {
    cstmt(*lpalgo.gcpolicy, [&] { return true; }, [&] {return loop_compl_fct(split.first, split.second);}, inner_seq_fct, inner_seq_fct);
  }
}

template <
 class Granularity_control_policy,
  class Loop_cutoff_fct,
  class Loop_complexity_measure_fct,
  class Number,
  class Body
>
void parallel_for_lazy_binary_search_bs(loop_by_lazy_binary_search_splitting<Granularity_control_policy>& lpalgo,
                  const Loop_cutoff_fct& loop_cutoff_fct,
                  const Loop_complexity_measure_fct& loop_compl_fct,
                  std::vector<std::pair<Number, Number>>& split_positions, Number l, Number h, const Body& body) {
  if (l != h - 1) {
//    cstmt(*lpalgo.gcpolicy, [&] { return true; }, [&] {return loop_compl_fct(split_positions[l].first, split_positions[h - 1].second);},
//      [&] {
        fork2(
          [&] {parallel_for_lazy_binary_search_bs(lpalgo, loop_cutoff_fct, loop_compl_fct, split_positions, l, (l + h) / 2, body);},
          [&] {parallel_for_lazy_binary_search_bs(lpalgo, loop_cutoff_fct, loop_compl_fct, split_positions, (l + h) / 2, h, body);}
        );
//      },
//      [&] {
//        for (Number i = split_positions[l].first; i < split_positions[h - 1].second; i++)
//          body(i);
//      }
//    );
  } else {
    auto inner_seq_fct = [&] {
      for (Number i = split_positions[l].first; i < split_positions[l].second; i++)
        body(i);
    };

    cstmt_report(*lpalgo.gcpolicy, [&] {return loop_compl_fct(split_positions[l].first, split_positions[l].second);}, inner_seq_fct);
  }
}


template <
  class Granularity_control_policy,
  class Loop_cutoff_fct,
  class Loop_complexity_measure_fct,
  class Number,
  class Body
>
void parallel_for(loop_by_lazy_binary_search_splitting<Granularity_control_policy>& lpalgo,
                  const Loop_cutoff_fct& loop_cutoff_fct,
                  const Loop_complexity_measure_fct& loop_compl_fct,
                  Number lo, Number hi, const Body& body) {

  estimator_m& estimator = lpalgo.gcpolicy->get_estimator();

  int k = 0;
  int x = estimator.get_minimal_estimations_nb_left();
  while (x > 0) {
    x >>= 1;
    k++;
  }

  auto cutoff_fct = [&] {
    return loop_cutoff_fct(lo, hi);
  };
  auto compl_fct = [&] {
    return loop_compl_fct(lo, hi);
  };

  if (k != 0) {
    k = (1 << k);
    loop_by_eager_binary_splitting<Granularity_control_policy> lpa(lpalgo.gcpolicy); 

    parallel_for(lpa,
      loop_cutoff_fct, loop_compl_fct, lo, std::min(hi, lo + k), body);

//    std::cerr << k << " " << estimator.get_minimal_estimations_nb_left() << std::endl;

    if (lo + k >= hi) {
      return;
    }

    lo += k;
  }

  std::vector<std::pair<Number, Number> > split_positions;

  Number l = lo;
  while (l < hi) {
    Number p = l;
    l = binary_search_estimated(*lpalgo.gcpolicy, loop_cutoff_fct, loop_compl_fct, p, hi);

    split_positions.push_back(std::make_pair(p, l));
  }
                                                                                                     
  parallel_for_lazy_binary_search_bs(lpalgo, loop_cutoff_fct, loop_compl_fct, split_positions, (Number)0, (Number)split_positions.size(), body);
}

//Parallel for by lazy binary splitt 

template <
  class Granularity_control_policy,
  class Loop_cutoff_fct,
  class Loop_complexity_measure_fct,
  class Number,
  class Body
>
void parallel_for(loop_by_lazy_binary_splitting<Granularity_control_policy>& lpalgo,
                  const Loop_cutoff_fct& loop_cutoff_fct,
                  const Loop_complexity_measure_fct& loop_compl_fct,
                  Number lo, Number hi, const Body& body) {
//  std::cerr << lo << " " << hi << std::endl;
  auto seq_fct = [&] {
    for (Number i = lo; i < hi; i++)
      body(i);
  };
  if (hi - lo < 2
     || (!lpalgo.gcpolicy->with_estimator() && loop_cutoff_fct(lo, hi))
     || (lpalgo.gcpolicy->with_estimator() && lpalgo.gcpolicy->get_estimator().constant_is_known() && lpalgo.gcpolicy->get_estimator().predict(loop_compl_fct(lo, hi)) <= kappa)) {
    cstmt_report(*lpalgo.gcpolicy, [&] {return loop_compl_fct(lo, hi);}, seq_fct);
  } else {                                                                                              
    auto cutoff_fct = [&] {
      return loop_cutoff_fct(lo, hi);
    };
    auto compl_fct = [&] {
      return loop_compl_fct(lo, hi);
    };

    Number l = binary_search_estimated(*lpalgo.gcpolicy, loop_cutoff_fct, loop_compl_fct, lo, hi);
    auto seq_fct1 = [&] {
      for (Number i = lo; i < l; i++)
        body(i);
    };                                                                  
//    seq_fct1();                                                       
    if (lpalgo.gcpolicy->get_estimator().predict(loop_compl_fct(lo, l)) <= kappa) {
      cstmt_report(*lpalgo.gcpolicy, [&] { return loop_compl_fct(lo, l); }, seq_fct1);
    } else {
      l = lo;
    }

    auto seq_fct2 = [&] {
      for (Number i = l; i < hi; i++)
        body(i);
    };

    Number mid = (l + hi) / 2;
    cstmt(*lpalgo.gcpolicy, cutoff_fct, compl_fct, 
          [&]{fork2(
            [&] {parallel_for(lpalgo, loop_cutoff_fct, loop_compl_fct, l, mid, body);},
            [&] {parallel_for(lpalgo, loop_cutoff_fct, loop_compl_fct, mid, hi, body);}
          );},
          seq_fct2
    );
  }
}
  
//Lazy binary splitting scheduling

template <
  class Granularity_control_policy,
  class Loop_cutoff_fct,
  class Loop_complexity_measure_fct,
  class Number,
  class Body
>
class parallel_for_base : public pasl::sched::native::multishot {
public:
  Granularity_control_policy* gcpolicy;
  Loop_cutoff_fct loop_cutoff_fct;
  Loop_complexity_measure_fct loop_compl_fct;
  Number l, r;
  Body body;
  pasl::sched::native::multishot* join;
  
  using self_type = parallel_for_base<Granularity_control_policy, Loop_cutoff_fct, Loop_complexity_measure_fct, Number, Body>;
  
  parallel_for_base(                                          
                      Granularity_control_policy* _gcpolicy,
                      const Loop_cutoff_fct& loop_cutoff_fct,
                      const Loop_complexity_measure_fct& loop_compl_fct,
                      const Number& l,
                      const Number& r,
                      const Body& body,
                      pasl::sched::native::multishot* join)                                                
  : loop_cutoff_fct(loop_cutoff_fct), loop_compl_fct(loop_compl_fct), l(l), r(r), body(body), join(join) {
    gcpolicy = _gcpolicy;
  }
              
  parallel_for_base(const self_type& other)
  : loop_cutoff_fct(other.loop_cutoff_fct), loop_compl_fct(other.loop_compl_fct), l(other.l), r(other.r), body(other.body), join(other.join) {
    gcpolicy = other.gcpolicy;
  }
  
  void run() {
    while (size() > 0) {
      auto cutoff_fct = [&] {
        return loop_cutoff_fct(l, r);
      };
      auto compl_fct = [&] {
        return loop_compl_fct(l, r);
      };
                                            
      Number m = binary_search_estimated(*gcpolicy, loop_cutoff_fct, loop_compl_fct, l, r);
      auto seq_fct = [&] {
        for (Number i = l; i < m; i++)
          body(i);
      };

      if (l == m) {
        std::cerr << "Fuck up " << l << " " << m << " " << r << std::endl;
      }

      auto small_compl_fct = [&] {          
        return loop_compl_fct(l, m);
      };

//      if (small_compl_fct() < 1) {
//        std::cerr << "Fuck up " << l << " " << m << " " << r << " " << small_compl_fct() << std::endl;   
//      }

      cstmt_report(*gcpolicy, small_compl_fct, seq_fct);
//      seq_fct();
      l = m;
      pasl::sched::native::yield();
    }
  }
  
  size_t size()  {
    return r < l ? 0:r-l;// + 1;
  }
  
  pasl::sched::thread_p split(size_t nb_items) {
    self_type* t = new self_type(*this);
    Number m = (l + r) / 2;
    r = m;
    t->l = m;
//    std::cerr << l << " " << r << " " << t->l << " " << t->r << " " << loop_compl_fct(l, r) << " " << loop_compl_fct(t->l, t->r) << std::endl;
    t->set_instrategy(pasl::sched::instrategy::ready_new());
    t->set_outstrategy(pasl::sched::outstrategy::unary_new());
    pasl::sched::threaddag::add_dependency(t, join);
    return t;
  }
  
//  THREAD_COST_UNKNOWN
  double get_cost() { return pasl::data::estimator::cost::unknown; }

};
  
template <
  class Granularity_control_policy,
  class Loop_cutoff_fct,
  class Loop_complexity_measure_fct,
  class Number,
  class Body
>
void parallel_for(loop_by_lazy_binary_splitting_scheduling<Granularity_control_policy>& lpalgo,
                  const Loop_cutoff_fct& loop_cutoff_fct,
                  const Loop_complexity_measure_fct& loop_compl_fct,
                  Number lo, Number hi, const Body& body) {                                                                  
  using thread_type = parallel_for_base<Granularity_control_policy, Loop_cutoff_fct, Loop_complexity_measure_fct, Number, Body>;
  pasl::sched::native::multishot* join = pasl::sched::native::my_thread();
  thread_type* thread = new thread_type(lpalgo.gcpolicy, loop_cutoff_fct, loop_compl_fct, lo, hi, body, join);
  join->finish(thread);
} 

}
}
}

#endif /*! _PASL_SCHED_GRANULARITY_H_ */
