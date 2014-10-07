/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Definitions of a few cached-measurement policies
 * \file cachedmeasure.hpp
 *
 */

#include "algebra.hpp"

#ifndef _PASL_DATA_CACHEDMEASURE_H_
#define _PASL_DATA_CACHEDMEASURE_H_

namespace pasl {
namespace data {
namespace cachedmeasure {

/***********************************************************************/

/*---------------------------------------------------------------------*/
//! [trivial]
template <class Item, class Size>
class trivial {
public:
  
  using size_type = Size;
  using value_type = Item;
  using algebra_type = algebra::trivial;
  using measured_type = typename algebra_type::value_type;
  using measure_type = measure::trivial<value_type, measured_type>;
  
  static void swap(measured_type& x, measured_type& y) {
    // nothing to do
  }
  
};
//! [trivial]
  
/*---------------------------------------------------------------------*/
//! [size]
template <class Item, class Size>
class size {
public:
  
  using size_type = Size;
  using value_type = Item;
  using algebra_type = algebra::int_group_under_addition_and_negation<size_type>;
  using measured_type = typename algebra_type::value_type;
  using measure_type = measure::uniform<value_type, measured_type>;
  
  static void swap(measured_type& x, measured_type& y) {
    std::swap(x, y);
  }
  
};
//! [size]
  
/*---------------------------------------------------------------------*/
//! [weight]
template <class Item, class Weight, class Size, class Measure_environment>
class weight {
public:
  
  using size_type = Size;
  using value_type = Item;
  using algebra_type = algebra::int_group_under_addition_and_negation<Weight>;
  using measured_type = typename algebra_type::value_type; // = Weight
  using measure_env_type = Measure_environment;
  using measure_type = measure::weight<value_type, measured_type, measure_env_type>;
  
  static void swap(measured_type& x, measured_type& y) {
    std::swap(x, y);
  }
  
};
//! [weight]
  
/*---------------------------------------------------------------------*/
//! [combiner]
template <class Cache1, class Cache2>
class combiner {
public:

  using algebra1_type = typename Cache1::algebra_type;
  using algebra2_type = typename Cache2::algebra_type;
  using measure1_type = typename Cache1::measure_type;
  using measure2_type = typename Cache2::measure_type;
  
  using size_type = typename Cache1::size_type;
  using value_type = typename Cache1::value_type;
  using algebra_type = algebra::combiner<algebra1_type, algebra2_type>;
  using measured_type = typename algebra_type::value_type;
  using measure_type = measure::combiner<value_type, measure1_type, measure2_type>;
  
  static void swap(measured_type& x, measured_type& y) {
    Cache1::swap(x.value1, y.value1);
    Cache2::swap(x.value2, y.value2);
  }
  
};
//! [combiner]
  
/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_CACHEDMEASURE_H_ */