/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file granularity.hpp
 * \brief Granularity control
 *
 */

#include "native.hpp"
#include "estimator.hpp"

#ifndef _PASL_SCHED_GRANULARITY_H_
#define _PASL_SCHED_GRANULARITY_H_

/***********************************************************************/

namespace pasl {
namespace sched {
namespace granularity {

/*---------------------------------------------------------------------*/
/* Granularity controller */
  
void todo() {
  util::atomic::fatal([] { std::cerr << "todo" << std::endl; });
}

using cmeasure_type = data::estimator::complexity_type;
using estimator_type = data::estimator::distributed;

class control {};

class control_by_force_parallel : public control {
public:
  control_by_force_parallel(std::string) { }
};

class control_by_force_sequential : public control {
public:
  control_by_force_sequential(std::string) { }
};

class control_by_prediction : public control {
public:
  estimator_type estimator;
  
  control_by_prediction(std::string name = ""): estimator(name) { }
  
  estimator_type& get_estimator() {
    return estimator;
  }
  void initialize(double init_cst) {
    estimator.set_init_constant(init_cst);
  }
  void set(std::string policy_arg) {
    ;
  }
};

class control_by_cmdline : public control {
public:
  using policy_type = enum {
    By_force_parallel,
    By_force_sequential,
    By_prediction
  };
  
  estimator_type estimator;
  policy_type policy;
  
  control_by_force_parallel cbfp;
  control_by_force_sequential cbfs;
  control_by_prediction cbp;
  
  control_by_cmdline(std::string name = ""): estimator(name),
  policy(By_prediction),
  cbfp(name), cbfs(name),
  cbp(name) {}
  
  // to be called by a callback in the PASL runtime
  void set(std::string policy_arg) {
    if (policy_arg == "by_force_parallel")
      policy = By_force_parallel;
    else if (policy_arg == "by_force_sequential")
      policy = By_force_sequential;
    else if (policy_arg == "by_prediction")
      policy = By_prediction;
    else
      util::atomic::fatal([&] { std::cout << "bogus policy " << policy_arg << std::endl; });
  }
  
  policy_type get() const {
    return policy;
  }
  
  estimator_type& get_estimator() {
    return estimator;
  }
  
  void initialize(double init_cst) {
    cbp.get_estimator().set_init_constant(init_cst);
  }
};

/*---------------------------------------------------------------------*/
/* Dynamics of granularity-control statements */

template <class Item>
class dynidentifier {
private:
  Item bk;
public:
  dynidentifier() {};
  dynidentifier(Item& bk_) : bk(bk_) {};
  
  Item& back() {
    return bk;
  }
  
  template <class Block_fct>
  void block(Item x, const Block_fct& f) {
    Item tmp = bk;
    bk = x;
    f();
    bk = tmp;
  }
};

// names of configurations supported by the granularity controller
using execmode_type = enum {
  Force_parallel,
  Force_sequential,
  Sequential,
  Parallel
};

// `p` configuration of caller; `c` callee
static inline
execmode_type execmode_combine(execmode_type p, execmode_type c) {
  // callee gives priority to Force*
  if (c == Force_parallel || c == Force_sequential)
    return c;
  // callee gives priority to caller when caller is Sequential
  if (p == Sequential) {
    return Sequential;
  }
  // otherwise, callee takes priority
  return c;
}

// current configuration of the running thread;
data::perworker::cell<dynidentifier<execmode_type>> execmode;

static inline
execmode_type& my_execmode() {
  return execmode.mine().back();
}
  
data::perworker::cell<dynidentifier<cost_type>> samplecost;
  
static inline
cost_type& my_samplecost() {
  return samplecost.mine().back();
}
  
static inline
void report_sample(cost_type elapsed) {
  cost_type& sample = my_samplecost();
  if (&sample == nullptr)
    return;
  sample += elapsed;
}

template <class Body_fct>
void cstmt_sequential(execmode_type c, const Body_fct& body_fct) {
  execmode_type p = my_execmode();
  execmode_type e = execmode_combine(p, c);
  execmode.mine().block(e, body_fct);
}
  
template <class Body_fct>
void cstmt_parallel(execmode_type c, const Body_fct& body_fct) {
  execmode_type p = my_execmode();
  execmode_type e = execmode_combine(p, c);
  execmode.mine().block(e, body_fct);
}
  
template <class Par_body_fct>
void cstmt_parallel_with_sampling(cmeasure_type m,
                                  Par_body_fct& par_body_fct,
                                  estimator_type& estimator) {
  samplecost.mine().block(0.0, [&] {
    execmode.mine().block(Parallel, par_body_fct);
  });
  cost_type& sample = samplecost.mine().back();
  if (sample == 0.0)
    return;
  estimator.report(m, sample);
}

template <class Seq_body_fct>
void cstmt_sequential_with_reporting(cmeasure_type m,
                                     Seq_body_fct& seq_body_fct,
                                     estimator_type& estimator) {
  cost_type start = util::ticks::now();
  execmode.mine().block(Sequential, seq_body_fct);
  cost_type elapsed = util::ticks::since(start);
  estimator.report(m, elapsed);
  STAT_COUNT(MEASURED_RUN);
  report_sample(elapsed);
}
template <
class Complexity_measure_fct,
class Par_body_fct
>
void cstmt(control& contr,
           const Complexity_measure_fct&,
           const Par_body_fct& par_body_fct) {
  cstmt_sequential(Force_parallel, par_body_fct);
}

template <class Par_body_fct>
void cstmt(control_by_force_parallel&, const Par_body_fct& par_body_fct) {
  cstmt_parallel(Force_parallel, par_body_fct);
}

// same as above but accepts all arguments to support general case
template <
class Complexity_measure_fct,
class Par_body_fct,
class Seq_body_fct
>
void cstmt(control_by_force_parallel& contr,
           const Complexity_measure_fct&,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct&) {
  cstmt(contr, par_body_fct);
}

template <class Seq_body_fct>
void cstmt(control_by_force_sequential&, const Seq_body_fct& seq_body_fct) {
  cstmt_sequential(Force_sequential, seq_body_fct);
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
class Complexity_measure_fct,
class Par_body_fct,
class Seq_body_fct
>
void cstmt(control_by_prediction& contr,
           const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
#ifdef SEQUENTIAL_BASELINE
  seq_body_fct();
  return;
#endif
  estimator_type& estimator = contr.get_estimator();
  cmeasure_type m = complexity_measure_fct();
  execmode_type c;
  if (m == data::estimator::complexity::tiny)
    c = Sequential;
  else if (m == data::estimator::complexity::undefined)
    c = Parallel;
  else
    c = (estimator.predict(m) <= kappa) ? Sequential : Parallel;
  if (c == Sequential)
    cstmt_sequential_with_reporting(m, seq_body_fct, estimator);
  else
    cstmt_parallel_with_sampling(m, par_body_fct, estimator);
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

// spawn parallel threads; with granularity control
template <class Body_fct1, class Body_fct2>
void fork2(const Body_fct1& f1, const Body_fct2& f2) {
  execmode_type mode = my_execmode();
#ifdef SEQUENTIAL_ELISION
  mode = Sequential;
#endif
  if (mode == Sequential ||
      mode == Force_sequential ) {
    f1();
    f2();
  } else {
    cost_type sample1 = 0.0, sample2 = 0.0;
    native::fork2([&] {
      samplecost.mine().block(sample1, [&] {
        execmode.mine().block(mode, f1);
      });
    }, [&] {
      samplecost.mine().block(sample2, [&] {
        execmode.mine().block(mode, f2);
      });
    });
    samplecost.mine().back() += sample1 + sample2;
  }
}
  
/*---------------------------------------------------------------------*/
/* Parallel for loop */

template <class Granularity_control_policy>
class loop_by_eager_binary_splitting {
public:
  Granularity_control_policy gcpolicy;
  
  loop_by_eager_binary_splitting(std::string name = "anonloop"): gcpolicy(name) {}
  
  void initialize(double init_cst) {
    gcpolicy.initialize(init_cst);
  }
  void set(std::string policy_arg) {
    gcpolicy.set(policy_arg);
  }
};

template <
  class Granularity_control_policy,
  class Loop_complexity_measure_fct,
  class Number,
  class Body
>
void parallel_for(loop_by_eager_binary_splitting<Granularity_control_policy>& lpalgo,
                  const Loop_complexity_measure_fct& loop_compl_fct,
                  Number lo, Number hi, const Body& body) {
  auto seq_fct = [&] {
    for (Number i = lo; i < hi; i++)
      body(i);
  };
  if (hi - lo < 2) {
    seq_fct();
  } else {
    auto compl_fct = [&] {
      return loop_compl_fct(lo, hi);
    };
    Number mid = (lo + hi) / 2;
    cstmt(lpalgo.gcpolicy, compl_fct,
          [&]{fork2([&] {parallel_for(lpalgo, loop_compl_fct, lo, mid, body);},
                    [&] {parallel_for(lpalgo, loop_compl_fct, mid, hi, body);} );},
          seq_fct);
  }
}
  
template <
  class Granularity_control_policy,
  class Number,
  class Body
>
void parallel_for(loop_by_eager_binary_splitting<Granularity_control_policy>& lpalgo,
                  Number lo, Number hi, const Body& body) {
  auto loop_compl_fct = [] (Number lo, Number hi) { return hi-lo; };
  parallel_for(lpalgo, loop_compl_fct, lo, hi, body);
}
 
  //! \todo find a better place for this function
template <class T>
std::string string_of_template_arg() {
  return std::string(typeid(T).name());
}

} // namespace
} // namespace
} // namespace

/***********************************************************************/

#endif /*! _PASL_SCHED_GRANULARITY_H_ */
