#include <iostream>  
#include <math.h>
#include <string>
#include <assert.h>
#include "ticks.hpp"
#include "perworker.hpp"
#include "native.hpp"

/***********************************************************************/

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
using cmeasure_type = long;

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

perworker::base<int> unique_estimator_id;

// constant estimator data structure
class constant_estimator {
private:
  static std::atomic<cost_type> shared_constant;
  pasl::data::perworker::base<cost_type> local_constants;

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
    //this->name = uniqify(name);
    name = uniqify(name);
  }

  void init(cost_type init_constant) {
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
  return elapsed((ticks_t)start, getticks());
}

/*---------------------------------------------------------------------*/
/* Granularity controller */

class control_by_force_parallel {
public:
  control_by_force_parallel(std::string) { }
};

class control_by_force_sequential {
public:
  control_by_force_sequential(std::string) { }
};

class control_by_cutoff_without_reporting {
public:
  control_by_cutoff_without_reporting(std::string) { }
};

class control_by_cutoff_with_reporting {
public:
  constant_estimator estimator;
  control_by_cutoff_with_reporting(std::string name = ""): estimator(name) {}

  constant_estimator& get_estimator() {
    return estimator;
  }
};

class control_by_prediction {
public:
  constant_estimator estimator;

  control_by_prediction(std::string name = ""): estimator(name) {}

  constant_estimator& get_estimator() {
    return estimator;
  }
};

class control_by_cmdline {
public:
  using policy_type = enum {
    By_force_parallel,
    By_force_sequential,
    By_cutoff_without_prediction,
    By_cutoff_with_prediction,
    By_prediction
  };

  constant_estimator estimator;
  policy_type policy;

  control_by_cmdline(std::string name = ""): estimator(name), policy(By_prediction) {}

  // to be called by a callback in the PASL runtime
  void set(std::string policy_arg) {
    if (policy_arg == "by_force_parallel")
      policy = By_force_parallel;
    else if (policy_arg == "by_force_sequential")
      policy = By_force_sequential;
    else if (policy_arg == "by_cutoff_without_prediction")
      policy = By_cutoff_without_prediction;
    else if (policy_arg == "by_cutoff_with_prediction")
      policy = By_cutoff_with_prediction;
    else if (policy_arg == "by_prediction")
      policy = By_prediction;
    else
      fatal([&] { std::cout << "bogus policy " << policy_arg << std::endl; });
  }

  policy_type get() const {
    return policy;
  }

  constant_estimator& get_estimator() {
    return estimator;
  }
};

/*---------------------------------------------------------------------*/
/* Dynamics of granularity-control statements */

template <class Item>
class dynidentifier {
private:
  Item bk;
public:
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
perworker::base<dynidentifier<execmode_type>> execmode;

execmode_type& my_execmode() {
  return execmode.mine().back();
}

// cutoff expressed in microseconds
cost_type kappa = 256.0;

template <class Body_fct>
void cstmt_base(execmode_type c, const Body_fct& body_fct) {
  execmode_type p = my_execmode();
  execmode_type e = execmode_combine(p, c);
  execmode.mine().block(e, body_fct);
}

template <class Seq_body_fct>
void cstmt_base_with_reporting(cmeasure_type m, Seq_body_fct& seq_body_fct, 
                               constant_estimator& estimator) {
  cost_type start = now();
  execmode.mine().block(Sequential, seq_body_fct);
  cost_type elapsed = since(start);
  estimator.report(m. elapsed);
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
  cstms_base(Force_sequential, seq_body_fct);
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
  constant_estimator& estimator = contr.get_estimator();
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
  constant_estimator& estimator = contr.get_estimator();
  cmeasure_type m = complexity_measure_fct();
  execmode_type c;
  if (m == tiny)
    c = Sequential;
  else if (m == undefined)
    c = Parallel;
  else
    c = (estimator.predict(m) <= kappa) ? Sequential : Parallel;
  if (c == Sequential)
    cstmt_base_with_reporting(m, seq_body_fct, estimator);
  else
    cstmt_base(Parallel, par_body_fct);
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
      mode == Force_sequential ) {
    f1();  // sequentialize
    f2();
  } else {
    pasl::sched::native::fork2([mode,&f1] { execmode.mine().block(mode, f1); },
                               [mode,&f2] { execmode.mine().block(mode, f2); });
  }
}

/*---------------------------------------------------------------------*/
/* Granularity-control enriched loops */

template <class Granularity_control_policy>
class loop_by_eager_binary_splitting {
public:
  Granularity_control_policy gcpolicy;

  loop_by_eager_binary_splitting(std::string name = "anonloop"): gcpolicy(name) {}
};

template <class Granularity_control_policy>
class loop_by_lazy_binary_splitting {
public:
  Granularity_control_policy gcpolicy;

  loop_by_lazy_binary_splitting(std::string name = "anonloop"): gcpolicy(name) {}
};

template <class Granularity_control_policy>
class loop_by_blocking {
public:
  Granularity_control_policy gcpolicy;

  loop_by_blocking(std::string name = "anonloop"): gcpolicy(name) {}
};

template <class Granularity_control_policy>
class loop_by_multiple_of_nb_proc {
public:
  Granularity_control_policy gcpolicy;

  loop_by_multiple_of_nb_proc(std::string name = "anonloop"): gcpolicy(name) {}
};

template <class Granularity_control_policy>
class loop_by_cmdline {
public:
  // names of supported loop-decomposition algorithms
  using loop_algo_type = enum {
    By_eager_binary_splitting,
    By_lazy_binary_splitting,
    By_blocking,
    By_multiple_of_nb_proc,
    By_inheritance
  };

  Granularity_control_policy gcpolicy;
  loop_algo_type algo;

  loop_by_cmdline(std::string name = ""): gcpolicy(name) {}

  void set(std::string loop_algo_name) {
    if (loop_algo_name == "by_eager_binary_splitting")
      algo = By_eager_binary_splitting;
    else if (loop_algo_name == "by_lazy_binary_splitting")
      algo = By_lazy_binary_splitting;
    else if (loop_algo_name == "by_blocking")
      algo = By_blocking;
    else if (loop_algo_name == "by_multiple_of_nb_proc")
      algo = By_multiple_of_nb_proc;
    else
      fatal([&] {std::cout << "bogus loop algorithm: " 
                           << loop_algo_name << '\n'; });
  }
};

// parallel_for loop using eager binary splitting
/* todo: this function has some redundancy with the `forkjoin` routine below; implement
 * `parallel_for` to replace the recursive call by a call to `forkjoin`
 */
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
  if (hi - lo < 2) {
    seq_fct();
  } else {
    auto cutoff_fct = [&] {
      return loop_cutoff_fct(lo, hi);
    };
    auto compl_fct = [&] {
      return loop_compl_fct(lo, hi);
    };
    Number mid = (lo + hi) / 2;
    cstmt(lpalgo.gcpolicy, cutoff_fct, compl_fct, 
          [&]{fork2(
            [&] {parallel_for(lpalgo, loop_cutoff_fct, loop_compl_fct, lo, mid, body);},
            [&] {parallel_for(lpalgo, loop_cutoff_fct, loop_compl_fct, mid, hi, body);}
          );,
          seq_fct
    );
  }
}

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

// fork join parallel skeleton using eager binary splitting
template <
  class Granularity_control_policy,
  class Skel_cutoff_fct,
  class Skel_complexity_measure_fct,
  class Input,
  class Output,
  class Input_empty_fct,
  class Fork_input_fct,
  class Join_output_fct,
  class Body
>
void forkjoin(loop_by_eager_binary_splitting<Granularity_control_policy>& lpalgo,
             const Skel_cutoff_fct& skel_cutoff_fct,
             const Skel_complexity_measure_fct& skel_compl_fct,
             Input& input,
             Output& output,
             const Input_empty_fct& input_empty_fct,
             const Fork_input_fct& fork_input_fct,
             const Join_output_fct& join_output_fct,
             const Body& body) {
  auto seq_fct = [&] {
    body(input, output);
  };

  if (input_empty_fct(input)) {
    seq_fct();
  } else {
    auto cutoff_fct = [&] {
      return skel_cutoff_fct(input);
    };

    auto compl_fct = [&] {
      return skel_compl_fct(input);
    };

    Input input2;
    Output output2;
    fork_input_fct(input, input2);
    cstmt(lpalgo.gcpolicy, cutoff_fct, compl_fct, 
          [&] {fork2(
                [&] {forkjoin(lpalgo, skel_cutoff_fct, skel_compl_fct, input, output,
                              input_empty_fct, fork_input_fct, join_output_fct, body); },
                [&] {forkjoin(lpalgo, skel_cutoff_fct, skel_compl_fct, input2, output2,
                              input_empty_fct, fork_input_fct, join_output_fct, body); }
          );},
          seq_fct
    );
    join_output_fct(output, output2);
  }
}

// combine the items of a given sequence using a given associative combining operator
template <
  class Loop_algo,
  class Range_cutoff_fct,
  class Range_complexity_measure_fct,
  class Number,
  class Output,
  class Assoc_comb_op,
  class Body
>
Output combine(Loop_algo& lpalgo,
              const Range_cutoff_fct& range_cutoff_fct,
              const Range_complexity_measure_fct& range_compl_fct,
              Number lo, Number hi,
              Output id,
              const Assoc_comb_op& assoc_comb_op,
              const Body& body) {
  using index_range_type = std::pair<Number, Number>;
  index_range_type input(lo, hi);
  Output output = id;
  auto input_empty_fct = [] (index_range_type r) {
    return r.second - r.first < 2;
  };
  auto fork_input_fct = [] (index_range_type& src, index_range_type& dst) {
    Number mid = (src.first + src.second) / 2;
    dst.first = mid;
    dst.second = src.second;
    src.second = mid;
  };
  auto body2 = [&] (index_range_type r, Output& output) {
    Number lo = r.first;
    Number hi = r.second;
    Output tmp = output;
    for (Number i = lo; i < hi; i++)
      tmp = assoc_comb_op(tmp, body(i));
    output = tmp;
  };
  auto join_output_fct = [&] (Output& output1, Output& output2) {
    output1 = assoc_comb_op(output1, output2);
    output2 = id;
  };
  auto pair_cutoff_fct = [&] (index_range_type input) {
    return range_cutoff_fct(input.first, input.second);
  };
  auto pair_compl_fct = [&] (index_range_type input) {
    return range_cutoff_fct(input.first, input.second);
  };
  forkjoin(lpalgo, pair_cutoff_fct, pair_compl_fct, input, output,
           input_empty_fct, fork_input_fct, join_output_fct, body2);

  return output;
}

// same as above, only specifically configured to use prediction-based granularity control
template <
  class Skel_complexity_measure_fct,
  class Number,
  class Output,
  class Assoc_comb_op,
  class Body
>
Output combine(loop_by_eager_binary_splitting<control_by_prediction>& lpalgo,
              const Skel_complexity_measure_fct& skel_compl_fct,
              Number lo, Number hi,
              Output id,
              const Assoc_comb_op& assoc_comb_op,
              const Body& body) {
  auto skel_cutoff_fct = [] (Number lo, Number hi) { return false; };

  return combine(lpalgo, skel_cutoff_fct, skel_compl_fct, lo, hi, id, assoc_comb_op, body);
}

/*---------------------------------------------------------------------*/
/* Various client-code examples using naive recursive fib */

// sequential fib
static
long fib(long n) {
  if (n < 2)
    return n;
  long a,b;
  a = fib(n-1);
  b = fib(n-2);
  return a+b;
}

// complexity function for fib(n)
static
long phi_to_pow(long n) {
  constexpr double phi = 1.61803399;
  return (long)pow(phi, (double)n);
}

control_by_prediction cfib("fib");

// parallel fib with complexity function
static
long pfib1(long n) {
  if (n < 2)
    return n;
  long a,b;
  cstmt(cfib, [&] { return phi_to_pow(n); }, [&] {
  fork2([&] { a = pfib1(n-1); },
        [&] { b = pfib1(n-2); }); });
  return a + b;
}

// parallel fib with complexity function and sequential body alternative
static
long pfib2(long n) {
  if (n < 2)
    return n;
  long a,b;
  cstmt(cfib, [&] { return phi_to_pow(n); }, [&] {
  fork2([&] { a = pfib2(n-1); },
        [&] { b = pfib2(n-2); }); },
  [&] { a = fib(n-1);
        b = fib(n-2); });
  return a + b;
}

control_by_cutoff_without_reporting cfib2("fib");

constexpr long fib_cutoff = 20;

// parallel fib with manual cutoff and sequential body alternative
static
long pfib3(long n) {
  if (n < 2)
    return n;
  long a,b;
  cstmt(cfib2, [&] { return n <= fib_cutoff; }, [&] {
  fork2([&] { a = pfib3(n-1); },
        [&] { b = pfib3(n-2); }); },
  [&] { a = fib(n-1);
        b = fib(n-2); });
  return a + b;
}

/*---------------------------------------------------------------------*/
/* Various exammples using parallel-for loops */

loop_by_eager_binary_splitting<control_by_prediction> lpcontr1;

auto loop_compl_const_fct = [] (long lo, long hi) {
  return hi - lo;
};

// allocate and initialize a vector; each cell in the vector contains a
// copy of `x`
template <class Item>
Item* create_vector(long n, Item x) {
  Item* vector = new_array<Item>(n);
  parallel_for(lpcontr1, loop_compl_const_fct, 0l, n, [&] (long i) {
    vector[i] = x;
  });
  return vector;
}

// allocate and initialize a matrix with `r` rows and `c` columns; each cell
// in the array contains a copy of `x`
template <class Item>
Item* create_matrix(long r, long c, Item x) {
  return create_vector(r * c, x);
}

loop_by_eager_binary_splitting<control_by_prediction> outerlp;
loop_by_eager_binary_splitting<control_by_prediction> innerlp;

// returns the sum of the numbers in the given vector
template <
  class Loop_algo,
  class Index,
  class Number
>
Number sum_vector(Loop_algo& lpalgo, Index n, Number id, const Number* vec) {
  auto sum_fct = [] (Number x, Number y) {
    return x + y;
  };
  auto ith = [&] (Index i) {
    return vec[i];
  };
  return combine(lpalgo, loop_compl_const_fct, Index(0), n, id, sum_fct, ith);
}

// returns the dotproduct of the items in the two dense vectors,
// given by `vec` and `vec2`
template <
  class Loop_algo,
  class Index,
  class Number
>
Number ddotprod(Loop_algo& lpalgo, Index n, Number id,
               const Number* vec1, const Number* vec2) {
  auto sum_fct = [] (Number x, Number y) {
    return x + y;
  };
  auto ith = [vec1,vec2] (Index i) {
    return vec1[i] * vec2[i];
  };
  return combine(lpalgo, loop_compl_const_fct, Index(0), n, id, sum_fct, ith);
}

// dense matrix by dense vector multiplication: d := m * v
// r: # rows in m; c # columns in m
static
void dmdvmult(long r, long c, const float* m, const float* v, float* d) {
  auto outer_loop_compl_fct = [c] (long lo, long hi) {
    return (hi - lo) * c;
  };
  parallel_for(outerlp, outer_loop_compl_fct, 0l, r, [&] (long i) {
    const float* row_i = &m[i*c];
    d[i] = ddotprod(innerlp, r, 0.0f, row_i, v);
  });
}

/*---------------------------------------------------------------------*/

void initialization() {
  pasl::util::ticks::set_ticks_per_second(1000);
  local_constants.init(undefined);
  current_execmode.init(Sequential);
}

int main(int argc, const char * argv[]) {
  intialization();

  // fib examples
  std::cout << "fib(10)=" << fib(10) << std::endl;
  std::cout << "pfib1(10)=" << pfib1(10) << std::endl;
  std::cout << "pfib2(10)=" << pfib2(10) << std::endl;
  std::cout << "pfib3(10)=" << pfib3(10) << std::endl;

  // vector sum
  loop_by_eager_binary_splitting<control_by_prediction> smvec;
  long vecsz = 100;
  float* vec = create_vector(vecsz, 1.2f);
  float vecsum = sum_vector(smvec, vecsz, 0.0f, vec);
  std::cout << "vecsum=" << vecsum << std::endl;

  // matrix by vector multiplication
  long r = 100;
  long c = 50;
  float* m = create_matrix(r, c, 0.01f);
  float* v = create_vector(r, 0.2f);
  float* d = create_vector(r, 0.0f);
  dmdvmult(r, c, m, v, d);
  std::cout << d[20] << std::endl;
  free(m);
  free(v);
  free(d);

  return 0;
}

/***********************************************************************/
