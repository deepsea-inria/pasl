/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file atomic.hpp
 * \brief Atomic operations
 *
 */

#ifndef _PASL_UTIL_ATOMIC_H_
#define _PASL_UTIL_ATOMIC_H_

#include <cstdlib>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <assert.h>
#include <atomic>

/***********************************************************************/

namespace pasl {
namespace util {
namespace atomic {
  
extern bool verbose;
  
void init_print_lock();
  
void acquire_print_lock();
  
void release_print_lock();
  
/*! \brief Applies a given function while holding the print lock
 *
 * Acquires print lock; applies `print_fct`; releases print lock.
 */
template <class Print_fct>
void msg(const Print_fct& print_fct) {
  acquire_print_lock();
  print_fct();
  release_print_lock();
}

/*! \brief Applies a given function while holding the print lock, then
 * terminates the program
 *
 * Acquires print lock; applies `print_fct`; releases print lock; 
 * terminates the program by calling std::exit(-1);
 */
template <class Print_fct>
void fatal(const Print_fct& print_fct) {
  msg(print_fct);
  assert(false);
  std::exit(-1);
}
  
  
static inline void compiler_barrier () {
  asm volatile("" ::: "memory");
}
  
  
/*---------------------------------------------------------------------*/
//! \todo replace instances of functions below by instances of functions above
  
/*! \brief Prints a message to stdout and terminates the program. */
void die(const char *fmt, ...);
  
/*! \brief Atomic `fprintf`
 *
 *  Calls to this routine are serialized by a mutex lock.
 */
void afprintf(FILE* stream, const char *fmt, ...);
  
/*! \brief Atomic `printf`
 *
 * Calls to this routine are serialized by a mutex lock.
 */
void aprintf(const char *fmt, ...);
  
/*! \brief Debug printf (requires lock)
 */
void bprintf(const char *fmt, ...);
  
/*! \brief Debug printf (does not require lock)
 */
void xprintf(const char *fmt, ...);
  
/*---------------------------------------------------------------------*/
  

  
} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_UTIL_ATOMIC_H_ */
