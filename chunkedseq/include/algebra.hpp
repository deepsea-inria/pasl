/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Definitions of a few algebras
 * \file algebra.hpp
 *
 */

#include "measure.hpp"

#ifndef _PASL_DATA_ALGEBRA_H_
#define _PASL_DATA_ALGEBRA_H_

namespace pasl {
namespace data {
namespace algebra {
      
/***********************************************************************/

/*---------------------------------------------------------------------*/
//! [trivial]
/*!
 * \class trivial
 * \ingroup algebras
 * \brief Trivial algebraic group whose sole element is a zero-byte structure
 *
 */
class trivial {
public:
  
  using value_type = struct { };
  
  static constexpr bool has_inverse = true;
  
  static value_type identity() {
    return value_type();
  }
  
  static value_type combine(value_type, value_type) {
    return identity();
  }
  
  static value_type inverse(value_type) {
    return identity();
  }
  
};
//! [trivial]
  
/*---------------------------------------------------------------------*/
//! [int_group_under_addition_and_negation]
/*!
 * \class int_group_under_addition_and_negation
 * \ingroup algebras
 * \brief Constructor for algebraic groups formed by integers along with sum and inverse operations
 * \tparam Int Type of the set of integers treated by the algebra
 */
template <class Int>
class int_group_under_addition_and_negation {
public:
  
  using value_type = Int;
  
  static constexpr bool has_inverse = true;

  static value_type identity() {
    return value_type(0);
  }
  
  static value_type combine(value_type x, value_type y) {
    return x + y;
  }
  
  static value_type inverse(value_type x) {
    return value_type(-1) * x;
  }
  
};
//! [int_group_under_addition_and_negation]
  
/*---------------------------------------------------------------------*/
//! [combiner]
/*!
 * \class combiner
 * \ingroup algebras
 * \brief Combiner of two algebras
 *
 * Combines two algebras to make a new algebra that pairs together
 * values of the two given algebras. The resulting algebra combines
 * together the operations of the given algebras to operate pointwise
 * on the values of the pairs.
 *
 * The resulting algebra has an inverse operator only if both of its
 * subalgebras has inverse operators and otherwise does not.
 *
 * \tparam Algebra1 an algebra
 * \tparam Algebra2 an algebra
 *
 */
template <class Algebra1, class Algebra2>
class combiner {
public:
  
  using algebra1_type = Algebra1;
  using algebra2_type = Algebra2;
  
  using value1_type = typename Algebra1::value_type;
  using value2_type = typename Algebra2::value_type;
  
  using value_type = measure::measured_pair<value1_type, value2_type>;
  
  static constexpr bool has_inverse =
                   algebra1_type::has_inverse
                && algebra2_type::has_inverse;
  
  static value_type identity() {
    return measure::make_measured_pair(algebra1_type::identity(),
                                       algebra2_type::identity());
  }
  
  static value_type combine(value_type x, value_type y) {
    return measure::make_measured_pair(algebra1_type::combine(x.value1, y.value1),
                                       algebra2_type::combine(x.value2, y.value2));
  }
  
  static value_type inverse(value_type x) {
    return measure::make_measured_pair(algebra1_type::inverse(x.value1),
                                       algebra2_type::inverse(x.value2));
  }

};
//! [combiner]
  
/*!
 * \function subtract
 * \ingroup algebras
 * \brief Subtraction operator
 * \pre the algebra defines an inverse operator (i.e., `Algebra::has_inverse == true`)
 * \tparam Algebra an algebra
 *
 * Returns `combine(x, inverse(y))`.
 */
template <class Algebra>
typename Algebra::value_type subtract(typename Algebra::value_type x,
                                      typename Algebra::value_type y) {
  assert(Algebra::has_inverse);
  return combine(x, Algebra::inverse(y));
}

/***********************************************************************/
  
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_ALGEBRA_H_ */