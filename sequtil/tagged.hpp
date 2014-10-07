/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved. 
 *
 * \file tagged.hpp
 *
 */

#ifndef _TAGGED_H_
#define _TAGGED_H_

#include <stdint.h>
#include <atomic>

namespace pasl {
namespace data {
namespace tagged {
  
/***********************************************************************/

/**
 * \defgroup tagged Tagged values
 * @ingroup data_structures
 * @{
 *
 * \brief A tagged value is a word which consists of a small integer
 * tag and a large integer value. 
 *
 * The size in bits of the value is just large enough to contain a 
 * (properly aligned) pointer.
 *
 * The operations typically use three types:
 *   - The type of the tag, value pair is named `Tagged`.
 *   - The type of the value is named `Value`.
 *   - The tag has type `long`.
 *
 * \note Currently this module supports only 64-bit words. As such,
 * the tag is a 3 bit integer and the value a 61 bit integer.
 *
 */

//! Size in bits of the tag 
const long NUM_TAG_BITS = 3;
//! Bit mask for the tag
const long TAG_MASK = (1<<NUM_TAG_BITS)-1;

/*! \brief Returns a tagged value consisting of the pair `v` and
 *  `bits`.
 *  \pre `0 <= bits < 8`
 */
template <typename Value, typename Tagged>
static inline Tagged create(Value v, long bits) {
  assert(sizeof(Value) == sizeof(int64_t));
  assert(sizeof(Tagged) == sizeof(int64_t));
  assert(bits <= TAG_MASK);
  union enc {
    Value         v;
    Tagged        t;
    uint64_t      bits;
  };
  enc e;
  e.v = v;
  assert((e.bits & TAG_MASK) == 0l);
  e.bits |= (uint64_t)bits;
  return e.t;
}

/*! \brief Returns the value component of the pair */
template <typename Value, typename Tagged>
static inline Value extract_value(Tagged t) {
  assert(sizeof(Value) == sizeof(int64_t));
  assert(sizeof(Tagged) == sizeof(int64_t));
  union enc {
    Value         v;
    Tagged        t;
    uint64_t      bits;
  };
  enc e;
  e.t = t;
  e.bits &= ~ TAG_MASK;
  return e.v;
}

/*! \brief Returns the tag component of the pair */
template <typename Value, typename Tagged>
static inline long extract_tag(Tagged t) {
  assert(sizeof(Value) == sizeof(int64_t));
  assert(sizeof(Tagged) == sizeof(int64_t));
  union enc {
    Value         v;
    Tagged        t;
    uint64_t      bits;
  };
  enc e;
  e.t = t;
  e.bits &= TAG_MASK;
  return (long)e.bits;
}

/*! \brief Atomic fetch and add on the value component of a tagged value
 *  \param tp pointer to the counter
 *  \param d the value to add to the value component
 *  \returns the old content of the value component
 *  \pre the sum `d + extract_value(t)` fits in 61 bits
 *  \post `extract_value(t)` fits in 61 bits
 *
 */
template <typename Tagged>
static inline int64_t atomic_fetch_and_add(Tagged* tp, int64_t d) {
  assert(sizeof(Tagged) == sizeof(int64_t));
  union enc {
    Tagged* tp;
    std::atomic<int64_t>* counter;
  };
  enc e;
  e.tp = tp;
  int64_t old = e.counter->fetch_add(d << NUM_TAG_BITS);
  return extract_value<int64_t, int64_t>(old) >> NUM_TAG_BITS;
}

/**
 * @}
 */
    
/***********************************************************************/
  
} // end namespace
} // end namespace
} // end namespace

#endif
