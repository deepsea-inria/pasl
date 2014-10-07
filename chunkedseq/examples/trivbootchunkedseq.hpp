/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Trivial instantiation of bootstrapped chunked sequence
 * \file trivbootchunkedseq.hpp
 *
 */

#include "cachedmeasure.hpp"
#include "bootchunkedseq.hpp"

#ifndef _PASL_DATA_TRIVBOOTCHUNKEDSEQ_H_
#define _PASL_DATA_TRIVBOOTCHUNKEDSEQ_H_

namespace pasl {
namespace data {
namespace chunkedseq {
namespace bootchunkedseq {

/***********************************************************************/

// basic instantiation of bootchunkedseq
template <class Item, int Chunk_capacity=32>
class triv {
private:
  
  class top_item_type {
  public:
    Item value;
    top_item_type() { }
    top_item_type(const Item& value)
    : value(value) { }
    size_t get_cached() const {
      return 1;
    }
  };
  
  class sized_cached_measure {
  public:
    
    using size_type = size_t;
    using value_type = top_item_type*;
    using algebra_type = algebra::int_group_under_addition_and_negation<size_type>;
    using measured_type = typename algebra_type::value_type;
    
    class measure_type {
    public:
      
      measured_type operator()(const value_type& v) const {
        return v->get_cached();
      }
      
      measured_type operator()(const value_type* lo, const value_type* hi) const {
        measured_type m = algebra_type::identity();
        for (auto p = lo; p < hi; p++)
          m = algebra_type::combine(m, operator()(*p));
        return m;
      }
      
    };
    
    static void swap(measured_type& x, measured_type y) {
      std::swap(x, y);
    }
    
  };
  
  using measure_type = typename sized_cached_measure::measure_type;
  
  using seq_type = cdeque<top_item_type, Chunk_capacity, sized_cached_measure>;
  
  measure_type meas_fct;
  seq_type seq;

public:
  
  using size_type = size_t;
  
  using value_type = Item;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  
  using self_type = triv<Item, Chunk_capacity>;
  
  bool empty() const {
    return seq.empty();
  }
  
  size_type size() const {
    return seq.get_cached();
  }
  
  value_type& front() {
    top_item_type& x = seq.front();
    return x.value;
  }
  
  value_type& back() {
    top_item_type& x = seq.back();
    return x.value;
  }
  
  void push_front(const value_type& v) {
    seq.push_front(meas_fct, new top_item_type(v));
  }
  
  void push_back(const value_type& v) {
    seq.push_back(meas_fct, new top_item_type(v));
  }
  
  value_type pop_front() {
    top_item_type x = seq.pop_front();
    value_type v = x.value;
    delete x;
    return v;
  }
  
  value_type pop_back() {
    top_item_type x = seq.pop_back();
    value_type v = x.value;
    delete x;
    return v;
  }
  
  void split(size_type n, self_type& other) {
    if (n == 0)
      return;
    size_type index = n - 1;
    size_type sz = size();
    assert(index < sz);
    top_item_type* v;
    seq.split(index, v, other.seq);
    assert(size() == index);
    seq.push_back(meas_fct, v);
  }
  
  void concat(self_type& other) {
    seq.concat(other.seq);
  }
  
  template <class Body>
  void for_each(const Body& f) const {
    seq.for_each([&] (top_item_type* v) {
      f(v->value);
    });
  }
  
};
  
/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_TRIVBOOTCHUNKEDSEQ_H_ */