/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file threaddag.hpp
 * \brief All the high-level functions that can be used for 
 * scheduling DAG computations.
 *
 */

#ifndef _THREADDAG_H_
#define _THREADDAG_H_

#include <utility>

#include "scheduler.hpp"

namespace pasl {
namespace sched {
namespace threaddag {

/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/** \defgroup aux Auxilliary functions
 * \ingroup threaddag
 *  @{
 */
  
//! Returns a pointer to the scheduler of the calling thread.
scheduler_p my_sched();
//! Returns the number of worker threads.
int get_nb_workers();
/*! \brief Returns the unique id of the worker thread that is executing
 *  the calling thread.
 */
int get_my_id();

/** @} */
/*---------------------------------------------------------------------*/
/** \defgroup entry Initialization and teardown
 * \ingroup threaddag
 *  @{
 */
  
void init();  
void launch(thread_p thread);
void destroy();

/** @} */
/*---------------------------------------------------------------------*/
/** \defgroup basic Basic operations for building the DAG
 * \ingroup threaddag
 *  @{
 */

/*! 
 * \brief Adds a thread to the set of ready threads.
 *
 *  \pre Assumes that the instrategy and outstrategy have
 *  been set appropriately beforehand, and that at
 *  least one dependency on the thread has been added if 
 *  the thread is not ready.
 */
void add_thread(thread_p t);

/*! 
 * \brief Adds a dependency edge so that `t2` cannot start 
 *  before `t1` has completed.
 *
 *  \pre Assumes that the instrategy and outstrategy have
 *  been set appropriately beforehand.
 */
void add_dependency(thread_p t1, thread_p t2);

/*! \brief Returns the outstrategy of the current thread.
 *
 *  Should be called at most once during the execution of a thread.
 */
outstrategy_p capture_outstrategy();

/*! \brief Ensures that the scheduler does not deallocate the
 *  calling thread after the thread completes.
 */
void reuse_calling_thread();

/** @} */
/*---------------------------------------------------------------------*/
/** \defgroup derived Derived operations for building the DAG
 * \ingroup threaddag
 *  @{
 */
  
typedef int branch_t;
const branch_t UNDEFINED = -1l;
const branch_t LEFT = 0;
const branch_t RIGHT = 1;
const branch_t SINGLE = 2;

/** \defgroup basicderived Basic
 *  @{
 */
  
/*!
 * \brief Assigns the given instrategy `in` to the thread and
 * assigns the captured current outstrategy to the thread.
 * However, does not add the thread to the DAG (so that 
 * dependencies can be added first).
 */
void join_with(thread_p thread, instrategy_p in);

/*!
 * Schedules the thread `cont` after setting its instrategy 
 * to `ready` and its outstrategy to the capture of the
 * current outstrategy. Same as `join_with` using `ready`
 * followed with `add_thread`.
 */
void continue_with(thread_p thread);

/*!
 * \brief Prepares to schedule the thread `thread` after setting its
 * instrategy  to `ready` and its outstrategy to `unary` pointing to
 * `join`. Assumes the instrategy of `join` to be already set. Adds
 * the thread `thread` to the set of ready threads.
 */
void fork(thread_p thread, thread_p join);
void fork(thread_p thread, thread_p join, branch_t branch);
  
/** @} */

/*---------------------------------------------------------------------*/
/** \defgroup fixed Fixed-arity fork join
 *  @{
 */

/*!
 * \defgroup unary Unary
 * @{
 * Schedules `thread` before `cont` before current continuation.
 * 
 * More precisely,
 * 1. instrategy of `thread` is `ready`
 * 2. outstrategy of `thread` is `unary` pointing on `cont`
 * 3. instrategy of `cont` is `unary` (or as specified by `in`)
 * 4. outstrategy of `cont` is a capture of current outstrategy
 * 
 */
void unary_fork_join(thread_p thread, thread_p cont, instrategy_p in);
void unary_fork_join(thread_p thread, thread_p cont);
/** @} */

/*!
 * \defgroup binary Binary
 * @{
 * Schedules `thread1` and `thread2` before `cont` before current 
 * continuation. Here, `thread2` is scheduled first, so that on
 * LIFO order the `thread1` gets executed first.
 * 
 * More precisely,
 * 1. instrategy of `thread_i` is `ready`
 * 2. outstrategy of `thread_i` is `unary` pointing on `cont`
 * 3. instrategy of `cont` is as specified by `in` or else
 *     by the result of calling `new_forkjoin_instrategy()`
 * 4. outstrategy of `cont` is a capture of current outstrategy
 * 
 */

void binary_fork_join(thread_p thread1, thread_p thread2, thread_p cont, 
                                                  instrategy_p in);
void binary_fork_join(thread_p thread1, thread_p thread2, thread_p cont);
/**! @} */

/** @} */
/*---------------------------------------------------------------------*/
/** \defgroup futures Futures 
 *  @{
 *
 * In PASL, a future is a type of outstrategy that admits any
 * number of threads as dependencies. Furthermore, these
 * dependencies can be added before, during, or after the thread
 * associtated with the future completes.
 *
 * PASL supports the eager and lazy semantics for futures. In eager,
 * the future is ready to execute just after allocation, whereas, in
 * lazy, the future has a single dependency that is satisfied only
 * after another thread adds itself as a dependency on the future.
 *
 * Futures must be deallocated manually. The operation to use is
 * delete_future.
 *
 * More precisely,
 * 1. instrategy of `thread` is `ready` if `lazy == false` and `unary` 
 * otherwise
 * 2. outstrategy of `thread` is `future` initially containing zero out 
 * edges
 * 3. instrategy of `cont` is `unary`
 * 4. outstrategy of `cont` is a capture of current outstrategy
 */

future_p create_future(thread_p thread, bool lazy);
void force_future(future_p future, thread_p cont, instrategy_p in);
void force_future(future_p future, thread_p cont);
void delete_future(future_p future);
  
/** @} */

/*---------------------------------------------------------------------*/
/** \defgroup asyncfinish Async/finish
 *  @{
 *
 * Operators `async` and `finish` provide a mechanism whereby a 
 * computation may spawn in a nested fashion a number of threads whose
 * completion is handled by a barrier synchronization.
 *
 * More precisely,
 * 1. instrategy of `thread` is `ready`
 * 2. outstrategy of `thread` is `unary` pointing on `cont`
 * 2. instrategy of `cont` is as specified by `in` or else `distributed`
 */

void async(thread_p thread, thread_p cont);
void finish(thread_p thread, thread_p cont, instrategy_p in);
void finish(thread_p thread, thread_p cont);

/** @} */

/** @} */
/*---------------------------------------------------------------------*/
  
instrategy_p new_forkjoin_instrategy();
outstrategy_p new_forkjoin_outstrategy(branch_t branch);
  
void change_factory(util::worker::controller_factory_t* factory);
  
/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif // _THREADDAG_H_
 
