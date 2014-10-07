/* COPYRIGHT (c) 2011 Arthur Chargueraud and Michael Rainey
 * All rights reserved.
 *
 * Barriers (implemented using pthread_barrier or spinning)
 * 
 */

#ifndef _PASL_UTIL_BARRIER_H_
#define _PASL_UTIL_BARRIER_H_

#include <atomic>
#include <iostream>

#include "atomic.hpp"
#include "microtime.hpp"

/*! \defgroup barrier Barrier
 * \ingroup sync
 * @{
 */

namespace pasl {
namespace util {
namespace barrier {

/***********************************************************************/

/*---------------------------------------------------------------------*/
/**
 * \class signature
 * \brief Barrier interface
 * \ingroup barrier
 */
class signature {
public:
  virtual void init(int nb_workers) = 0;
  virtual void wait() = 0;
};

/*---------------------------------------------------------------------*/

#ifdef HAVE_PTHREAD_BARRIER
/**
 * \class pthread
 * \brief Implementation based on `pthread` barriers
 * \ingroup barrier
 */
class pthread : public signature {  
private:
  pthread_barrier_t init_bar;

public:
  
  virtual void init(int nb_workers) {
    pthread_barrier_init (&init_bar, NULL, nb_workers);
  }
  
  virtual void wait() {
    pthread_barrier_wait (&init_bar);
  }
};
#endif
  
/*---------------------------------------------------------------------*/
/**
 * \class spin
 * \brief Implementation based on spin locks
 * \ingroup barrier
 */
class spin : public signature { 
private:

  std::atomic<int> nb_left;
  std::atomic<bool> is_ready;

public:

  void init(int nb_workers) {
    nb_left.store(nb_workers);
    is_ready.store(false);
  }
  
  void wait() {
    wait([] { });
  }
  
  template <class Poll_fct>
  void wait(const Poll_fct& poll_fct) {
    int nb = nb_left.fetch_sub(1);
    if (nb == 1)
      is_ready.store(true);
    while (! is_ready.load()) {
      poll_fct();
      microtime::microsleep(0.1); // LATER: improve this
    }
  }
  
};

/*---------------------------------------------------------------------*/
/**
 * \class barrier_t
 * \brief Barrier
 * \ingroup barrier
 */
#ifdef HAVE_PTHREAD_BARRIER
class barrier_t : public pthread { };
#else
class barrier_t : public spin { };
#endif

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

/*! @} */

#endif /*! _PASL_UTIL_BARRIER_H_ */
