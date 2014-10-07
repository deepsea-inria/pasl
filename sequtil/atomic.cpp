/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file atomic.cpp
 * \brief Atomic operations
 *
 */

#include <iostream>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "atomic.hpp"

/***********************************************************************/

namespace pasl {
namespace util {
namespace atomic {

bool verbose;
  
pthread_mutex_t print_lock;
  
void init_print_lock() {
  pthread_mutex_init(&print_lock, nullptr);
}

void acquire_print_lock() {
  pthread_mutex_lock (&print_lock);
}

void release_print_lock() {
  pthread_mutex_unlock (&print_lock);
}

void die (const char *fmt, ...) {
  va_list	ap;
  va_start (ap, fmt);
  acquire_print_lock(); {
    fprintf (stderr, "Fatal error -- ");
    vfprintf (stderr, fmt, ap);
    fprintf (stderr, "\n");
    fflush (stderr);
  }
  release_print_lock();
  va_end(ap);
  assert(false);
  exit (-1);
}

void afprintf (FILE* stream, const char *fmt, ...) {
  va_list	ap;
  va_start (ap, fmt);
  acquire_print_lock(); {
    vfprintf (stream, fmt, ap);
    fflush (stream);
  }
  release_print_lock();
  va_end(ap);
}

void aprintf (const char *fmt, ...) {
  va_list	ap;
  va_start (ap, fmt);
  acquire_print_lock(); {
    vfprintf (stdout, fmt, ap);
    fflush (stdout);
  }
  release_print_lock();
  va_end(ap);
}

void bprintf (const char *fmt, ...) {
#ifdef DEBUG
  if (! verbose)
    return;
  va_list	ap;
  va_start (ap, fmt);
  acquire_print_lock(); {
    vfprintf (stdout, fmt, ap);
    fflush (stdout);
  }
  release_print_lock();
  va_end(ap);
#endif
}

void xprintf (const char *fmt, ...) {
#ifdef DEBUG
  if (! verbose)
    return;
  va_list	ap;
  va_start (ap, fmt);
  vfprintf (stdout, fmt, ap);
  fflush (stdout);
#endif
}

}
}
}

/***********************************************************************/
