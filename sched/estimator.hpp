/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file estimator.hpp
 * \brief Constant-estimator data structure
 *
 */

#ifndef _PASL_DATA_ESTIMATOR_H_
#define _PASL_DATA_ESTIMATOR_H_

#include <string>

#include "perworker.hpp"
#include "callback.hpp"

/***********************************************************************/

namespace pasl {
namespace data {
namespace estimator {
  
/**
 * @ingroup granularity
 * \defgroup estimator Estimator
 * @{
 * An estimator is a mechanism that can be used to predict
 * thread execution times, using asymptotic complexity functions provided
 * by the user and runtime measures.
 * @}
 */

void init();
void destroy();
  
/*---------------------------------------------------------------------*/
/* Complexity representation */
  
using complexity_type = long;

/**
 * \namespace complexity
 * \brief Complexity representation
 */
namespace complexity {
  
  //! A `tiny` complexity forces sequential execution
  const complexity_type tiny = (complexity_type) (-1l);
  
  //! An `undefined` complexity indicates that the value hasn't been computed yet
  const complexity_type undefined = (complexity_type) (-2l);
  
}  // end namespace
  
/*---------------------------------------------------------------------*/
/* Complexity annotation */

namespace annotation {
  static inline complexity_type lgn(complexity_type n) {
#ifdef __GNUC__
    return sizeof(complexity_type)*8l - 1l - __builtin_clzll(n);
#else
#error "need to implement lgn for the general case"
#endif
  }
  static inline complexity_type lglgn(complexity_type n) {
    return lgn(lgn(n));
  }
  static inline complexity_type mul(complexity_type n, complexity_type m) {
    return n * m;
  }
  static inline complexity_type nlgn(complexity_type n) {
    return n * lgn(n);
  }
  static inline complexity_type nsq(complexity_type n) {
    return n * n;
  }
  static inline complexity_type ncub(complexity_type n) {
    return n * n * n;
  }
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Cost representation */

using cost_type = double;

/**
 * \namespace cost
 * \brief Cost representation
 */
namespace cost {
  //! an `undefined` execution time indicates that the value hasn't been computed yet
  const cost_type undefined = -1.0;
  
  //! an `unknown` execution time forces parallel execution
  const cost_type unknown = -2.0;
  
  //! a `tiny` execution time forces sequential execution, and skips time measures
  const cost_type tiny = -3.0;
  
  //! a `pessimistic` cost is 1 microsecond per unit of complexity
  const cost_type pessimistic = 1.0;
  
  bool regular(cost_type cost);
  
}  // end namespace
  
/*---------------------------------------------------------------------*/

/*! \class signature
 *  \brief The basic interface of an estimator.
 *  \ingroup estimator
 */
class signature {
public:
  
  //! Sets an initial value for the constant (optional).
  virtual void set_init_constant(cost_type init_cst) = 0;
  
  //! Tests whether an initial value for the constant was provided
  virtual bool init_constant_provided() = 0;
  
  //! Returns the string identifier of the estimator.
  virtual std::string get_name() = 0;
  
  /*! \brief Adds to the running estimate a measurement of a task execution.
   *  \param comp the number of operations executed by the task
   *  \param elapsed_ticks the number of ticks taken by the execution
   */
  virtual void report(complexity_type comp, double elapsed_ticks) = 0;
  
  //! Tests whether the estimator already has an estimate of the constant
  virtual bool constant_is_known() = 0;
  
  //! Read the local value of the constant
  virtual cost_type get_constant() = 0;
  
  /*! \brief Predicts the wall-clock time required to execute a task
   *  \param comp the asymptotic complexity
   */
  virtual cost_type predict(complexity_type comp) = 0;
  
  /*! \brief Predicts the number of iterations that can execute in `kappa`
   *  seconds. To be used only for loops with constant time body.
   */
  virtual uint64_t predict_nb_iterations() = 0;
  
  //! Outputs the value of the constant as it is at the end of the program
  virtual void output() = 0;
};

/*---------------------------------------------------------------------*/

/*! \class common
 *  \brief Contains the code shared by our implementations of estimators
 * @ingroup estimator
 */
class common : public signature {
protected:
  //! Stores the name of the estimator.
  std::string name;
  
  //! Predicts the cost associated with an asymptotic complexity
  cost_type predict_impl(complexity_type comp);
  
  //! Read the local value of the constant, or pessimistic if unknown
  cost_type get_constant_or_pessimistic();
  
  //! Take into account a measure for updating the constant
  virtual void analyse(cost_type measured_cst) = 0;
  
  //! Log the update to the value of a constant
  void log_update(cost_type new_cst);
  
  void check();
  
public:
  
  common(std::string name)
  : name(name) {
    check();
  }
  
  virtual void init();
  virtual void output();
  virtual void destroy();
  
  /*! Implements `report` using function `analyse`;
   *  Assumes the complexity is not `tiny`.
   */
  void report(complexity_type comp, double elapsed_ticks);
  
  //! Implements `predict` using function `get_constant`
  cost_type predict(complexity_type comp);
  
  /*! Implements predict_iterations using the value of the constant
   *  or a pessimistic value in case it is unknown
   */
  uint64_t predict_nb_iterations();
  
  // Implements other auxiliary functions
  
  std::string get_name();
  
};

/*---------------------------------------------------------------------*/

/*! \class distributed
 *  \brief A distributed implementation of the estimator which
 *  uses both a shared value and thread-local values.
 *
 * @ingroup estimator
 */

class distributed : public common, util::callback::client {
private:
  constexpr static const double min_report_shared_factor = 2.0;
  constexpr static const double max_decrease_factor = 4.0;
  constexpr static const double ignored_outlier_factor = 20.0;
  constexpr static const double weighted_average_factor = 8.0;
  
  bool init_constant_provided_flg;
  
public: //! \todo find a better way to avoid false sharing
  volatile int padding1[64*2];
  cost_type shared_cst;
  perworker::cell<cost_type> private_csts;
  
protected:
  void update(cost_type new_cst);
  void analyse(cost_type measured_cst);
  cost_type get_constant();
  void update_shared(cost_type new_cst);
  
public:
  distributed(std::string name)
  : init_constant_provided_flg(false), private_csts(cost::undefined),
    common(name) {
    util::callback::register_client(this);
  }
  void init();
  void destroy();
  void output();
  void set_init_constant(cost_type init_cst);
  bool init_constant_provided();
  bool constant_is_known();
};
  
} // end namespace
} // end namespace
  
typedef data::estimator::cost_type cost_type;
typedef data::estimator::complexity_type complexity_type;
typedef data::estimator::signature estimator_t;
typedef estimator_t* estimator_p;
  
} // end namespace

/***********************************************************************/

#endif /*! _PASL_DATA_ESTIMATOR_H_ */