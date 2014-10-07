/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file localityrange.hpp
 *
 */

#ifndef _PASL_DATA_LOCALITYRANGE_H_
#define _PASL_DATA_LOCALITYRANGE_H_

/***********************************************************************/

namespace pasl {
namespace data {

//! type locality_t is used for representing positions in the computation DAG
//! Note that the logging depends on this definition.
typedef uint64_t locality_t;

/*! \class locality_range
 *  \brief Range of indices to represent the locality of a task
 *  \ingroup task
 */
class locality_range_t {
public:
  locality_t low;
  locality_t hi;
  
  locality_range_t() {}
  locality_range_t(locality_t low, locality_t hi) : low(low), hi(hi) {}
  
  //! returns the span of the range
  locality_t span() {
    return (hi - low);
  }
  
  //! returns the middle of the range
  locality_t mid() {
    return (low + hi) / 2;
  }
  
  //! returns the lower half of the range
  locality_range_t half_lower() {
    locality_range_t r;
    r.low = low;
    r.hi = mid();
    return r;
  }
  
  //! returns the upper half of the range
  locality_range_t half_upper() {
    locality_range_t r;
    r.low = mid();
    r.hi = hi;
    return r;
  }
  
  void split(locality_range_t* ranges, int nb_ranges) {
    locality_t range_sz = span() / (locality_t)nb_ranges;
    for (int i = 0; i < nb_ranges; i++) {
      locality_t lowi = low + (range_sz * i);
      locality_t hii = std::min(lowi + range_sz, hi);
      ranges[i] = locality_range_t(lowi, hii);
    }
  }
  
  static locality_range_t init() {
    locality_range_t r;
    r.low = 0;
    r.hi = 1l << 60;
    return r;     
  }
  
};

} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_DATA_LOCALITYRANGE_H_ */