/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file thread.hpp
 *
 */

#include "estimator.hpp"
#include "classes.hpp"
#include "localityrange.hpp"
#include "stats.hpp"
#include "atomic.hpp"

#ifndef _PASL_SCHED_THREAD_H_
#define _PASL_SCHED_THREAD_H_

/***********************************************************************/

namespace pasl {
namespace sched {
  
/*! \class signature
 *  \brief The basic interface of a thread.
 *  \ingroup thread
 */
class thread {
public: //! \todo ideally, would be protected
  
  //! instrategy for detecting readiness of the thread
  instrategy_p in;
  
  //! outstrategy for representing the continuation of the thread
  outstrategy_p out;
  
  //! true, if this thread should not be deallocated
  bool should_not_deallocate;
  
#ifdef TRACK_LOCALITY
  //! index representing the locality of the thread in the DAG
  data::locality_range_t locality;
#endif
  
public:
  
  /** @name Basic components */
  
  ///@{
  
  thread(bool should_not_deallocate = false)
  : in(NULL), out(NULL),
  should_not_deallocate(should_not_deallocate) { }
  
  virtual ~thread() { }
  
  /*! \brief Run-time prediction
   *
   * Returns the execution time predicted for this thread by
   * the estimator mechanism.
   */
  virtual cost_type get_cost() = 0;
  
  /*! \brief To be called by the scheduler to run the thread
   */
  virtual void exec() {
    run();
  }
  
  //! The body of the thread
  virtual void run() = 0;
  ///@}
  
  /** @name thread-splitting components (optional) */
  
  ///@{
  /*!
   * \brief Returns size
   *
   * Returns the number of splittable work items contained in the thread.
   */
  virtual size_t size() {
    return 1;
  }
  
  /*!
   * \brief Splits a thread
   * \pre 0 < `nb_items` < `size()`
   *
   * Leaves the first `nb_items` work items items in this thread.
   * Creates a new thread that contains the rest of the work items.
   */
  virtual thread_p split(size_t nb_items) {
    util::atomic::die("split unsupported for this thread");
    return NULL;
  }
  ///@}
  
  /** @name In- and out-strategies  */
  
  ///@{
  //! Assigns an instrategy to the thread
  void set_instrategy(instrategy_p in) {
    this->in = in;
  }
  
  //! Assigns an outstrategy to the thread
  void set_outstrategy(outstrategy_p out) {
    this->out = out;
    
  }
  ///@}
  
  /** @name Miscellaneous  */
  
  ///@{
  //! Resets cached members
  virtual void reset_caches() { }
  
  //! Replaces the default "new" operator with ours
  void* operator new (size_t size) {
    STAT_COUNT(THREAD_ALLOC);
    return ::operator new(size);
  }
  
  virtual void set_should_not_deallocate(bool should_not_deallocate) {
    this->should_not_deallocate = should_not_deallocate;
  }
  ///@}
};
  
/*---------------------------------------------------------------------*/

/*! \class noop
 *  \brief Thread which just returns, doing nothing.
 *  \ingroup thread
 */
class noop : public thread {
public:
  cost_type get_cost() { return data::estimator::cost::tiny; }
  void run() {}
};

/*---------------------------------------------------------------------*/
  
#define THREAD_COST_UNKNOWN \
cost_type get_cost() { return pasl::data::estimator::cost::unknown; }
  
} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_SCHED_THREAD_H_ */