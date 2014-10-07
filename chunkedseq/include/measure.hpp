/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Definitions of a few standard measure functors
 * \file measure.hpp
 *
 */

#include <assert.h>
#include <utility>

#ifndef _PASL_DATA_MEASURE_H_
#define _PASL_DATA_MEASURE_H_

namespace pasl {
namespace data {
namespace measure {

/***********************************************************************/

/*---------------------------------------------------------------------*/
//! [trivial]
template <class Item, class Measured>
class trivial {
public:
  
  using value_type = Item;
  using measured_type = Measured;

  measured_type operator()(const value_type& v) const {
    return measured_type();
  }
  
  measured_type operator()(const value_type* lo, const value_type* hi) const {
    return measured_type();
  }
    
};
//! [trivial]

/*---------------------------------------------------------------------*/
//! [uniform]
template <class Item, class Measured, int Item_weight=1>
class uniform {
public:
  
  using value_type = Item;
  using measured_type = Measured;
  const int item_weight = Item_weight;
  
  measured_type operator()(const value_type& v) const {
    return measured_type(item_weight);
  }
    
  measured_type operator()(const value_type* lo, const value_type* hi) const {
    return measured_type(hi - lo);
  }
};
//! [uniform]

/*---------------------------------------------------------------------*/
//! [weight]
template <class Item, class Weight, class Client_weight_fct>
class weight {
public:
  
  using value_type = Item;
  using measured_type = Weight;
  using client_weight_fct_type = Client_weight_fct;
  
private:
  
  client_weight_fct_type client_weight_fct;
  
  // for debugging purposes
  bool initialized;
  
public:
  
  weight() : initialized(false) { }
  
  weight(const client_weight_fct_type& env)
  : client_weight_fct(env), initialized(true) { }
  
  measured_type operator()(const value_type& v) const {
    return client_weight_fct(v);
  }
  
  measured_type operator()(const value_type* lo, const value_type* hi) const {
    measured_type m = 0;
    for (auto p = lo; p < hi; p++)
      m += operator()(*p);
    return m;
  }
  
  client_weight_fct_type get_env() const {
    assert(initialized);
    return client_weight_fct;
  }
  
  void set_env(client_weight_fct_type wf) {
    client_weight_fct = wf;
    initialized = true;
  }
  
};
//! [weight]
  
/*---------------------------------------------------------------------*/
//! [measured_pair]
template <class Measured1, class Measured2>
class measured_pair {
public:
  Measured1 value1;
  Measured2 value2;
  measured_pair() { }
  measured_pair(const Measured1& value1, const Measured2& value2)
  : value1(value1), value2(value2) { }
};

template <class Measured1, class Measured2>
measured_pair<Measured1,Measured2> make_measured_pair(Measured1 m1, Measured2 m2) {
  measured_pair<Measured1,Measured2> m(m1, m2);
  return m;
}
//! [measured_pair]

/*---------------------------------------------------------------------*/
//! [combiner]
template <class Item, class Measure1, class Measure2>
class combiner {
public:
  
  using measure1_type = Measure1;
  using measure2_type = Measure2;
  
  using value_type = Item;
  using measured_type = measured_pair<measure1_type, measure2_type>;
  
  measure1_type meas1;
  measure2_type meas2;
  
  combiner() { }
  
  combiner(const measure1_type meas1)
  : meas1(meas1) { }
  
  combiner(const measure2_type meas2)
  : meas2(meas2) { }
  
  combiner(const measure1_type meas1, const measure2_type meas2)
  : meas1(meas1), meas2(meas2) { }
  
  measured_type operator()(const value_type& v) const {
    return make_measured_pair(meas1(v), meas2(v));
  }
  
  measured_type operator()(const value_type* lo, const value_type* hi) const {
    return make_measured_pair(meas1(lo, hi), meas2(lo, hi));
  }
  
};
//! [combiner]

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_MEASURE_H_ */