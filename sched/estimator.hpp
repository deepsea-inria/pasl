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

/***********************************************************************/

namespace pasl {
namespace data {
namespace estimator {
  
/**
 * @ingroup granularity
 * \defgroup estimator Estimator
 * @{
 * An estimator is a mechanism that can be used to predict
 * task execution times, using asymptotic complexity functions provided
 * by the user and runtime measures.
 * @}
 */

void init();
void destroy();
  
/*---------------------------------------------------------------------*/
/* Complexity representation */
  
typedef int64_t complexity_t;

/**
 * \namespace complexity
 * \brief Complexity representation
 */
namespace complexity {
  
  //! A `tiny` complexity forces sequential execution
  const complexity_t tiny = (complexity_t) (-1l);
  
  //! An `undefined` complexity indicates that the value hasn't been computed yet
  const complexity_t undefined = (complexity_t) (-2l);
  
}  // end namespace
  
/*---------------------------------------------------------------------*/
/* Complexity annotation */

namespace annotation {
  /*! \todo make it portable */
  static inline complexity_t lgn(complexity_t n) {
#ifdef __GNUC__
    return sizeof(complexity_t)*8l - 1l - __builtin_clzll(n);
#else
#error "need to implement lgn for the general case"
#endif
  }
  static inline complexity_t lglgn(complexity_t n) {
    return lgn(lgn(n));
  }
  static inline complexity_t mul(complexity_t n, complexity_t m) {
    return n * m;
  }
  static inline complexity_t nlgn(complexity_t n) {
    return n * lgn(n);
  }
  static inline complexity_t nsq(complexity_t n) {
    return n * n;
  }
  static inline complexity_t ncub(complexity_t n) {
    return n * n * n;
  }
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Cost representation */

typedef double cost_t;

/**
 * \namespace cost
 * \brief Cost representation
 */
namespace cost {
  //! an `undefined` execution time indicates that the value hasn't been computed yet
  const cost_t undefined = -1.0;
  
  //! an `unknown` execution time forces parallel execution
  const cost_t unknown = -2.0;
  
  //! a `tiny` execution time forces sequential execution, and skips time measures
  const cost_t tiny = -3.0;
  
  //! a `pessimistic` cost is 1 microsecond per unit of complexity
  const cost_t pessimistic = 1.0;
  
  bool regular(cost_t cost);
  
}  // end namespace
  
/*---------------------------------------------------------------------*/

/*! \class signature
 *  \brief The basic interface of an estimator.
 *  \ingroup estimator
 */
class signature {
public:
  
  //! Sets an initial value for the constant (optional).
  virtual void set_init_constant(cost_t init_cst) = 0;
  
  //! Tests whether an initial value for the constant was provided
  virtual bool init_constant_provided() = 0;
  
  //! Sets a string identifier for the estimator (optional).
  virtual void set_name(std::string name) = 0;
  
  //! Returns the string identifier of the estimator.
  virtual std::string get_name() = 0;
  
  /*! \brief Adds to the running estimate a measurement of a task execution.
   *  \param comp the number of operations executed by the task
   *  \param elapsed_ticks the number of ticks taken by the execution
   */
  virtual void report(complexity_t comp, double elapsed_ticks) = 0;
  
  //! Tests whether the estimator already has an estimate of the constant
  virtual bool constant_is_known() = 0;
  
  //! Read the local value of the constant
  virtual cost_t get_constant() = 0;
  
  /*! \brief Predicts the wall-clock time required to execute a task
   *  \param comp the asymptotic complexity
   */
  virtual cost_t predict(complexity_t comp) = 0;
  
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
private:
  //! Stores the name of the estimator.
  std::string name;
  
private:
  //! Predicts the cost associated with an asymptotic complexity
  cost_t predict_impl(complexity_t comp);
  
protected:
  
  //! Read the local value of the constant, or pessimistic if unknown
  cost_t get_constant_or_pessimistic();
  
  //! Take into account a measure for updating the constant
  virtual void analyse(cost_t measured_cst) = 0;
  
  //! Log the update to the value of a constant
  void log_update(cost_t new_cst);
  
public:
  void init(std::string name);
  void output();
  void destroy();
  
  /*! Implements `report` using function `analyse`;
   *  Assumes the complexity is not `tiny`.
   */
  void report(complexity_t comp, double elapsed_ticks);
  
  //! Implements `predict` using function `get_constant`
  cost_t predict(complexity_t comp);
  
  /*! Implements predict_iterations using the value of the constant
   *  or a pessimistic value in case it is unknown
   */
  uint64_t predict_nb_iterations();
  
  // Implements other auxiliary functions
  
  std::string get_name();
  void set_name(std::string name);
  
};

/*---------------------------------------------------------------------*/

/*! \class distributed
 *  \brief A distributed implementation of the estimator which
 *  uses both a shared value and thread-local values.
 *
 * @ingroup estimator
 */

class distributed : public common {
private:
  constexpr static const double min_report_shared_factor = 2.0;
  constexpr static const double max_decrease_factor = 4.0;
  constexpr static const double ignored_outlier_factor = 20.0;
  constexpr static const double weighted_average_factor = 8.0;
  
  bool init_constant_provided_flg;
  
public: //! \todo find a better way to avoid false sharing
  volatile int padding1[64*2];
  cost_t shared_cst;
  perworker::base<cost_t> private_csts;
  
protected:
  void update(worker_id_t my_id, cost_t new_cst);
  void analyse(cost_t measured_cst);
  cost_t get_constant();
  void update_shared(cost_t new_cst);
  
public:
  distributed() : init_constant_provided_flg(false) { }
  void init(std::string name);
  void destroy();
  void set_init_constant(cost_t init_cst);
  bool init_constant_provided();
  bool constant_is_known();
};
  
} // end namespace
} // end namespace
  
typedef data::estimator::cost_t cost_t;
typedef data::estimator::complexity_t complexity_t;
typedef data::estimator::signature estimator_t;
typedef estimator_t* estimator_p;
  
} // end namespace

/***********************************************************************/

#endif /*! _PASL_DATA_ESTIMATOR_H_ */