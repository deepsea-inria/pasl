/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2013 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Unit tests for chunkedseq
 * \file fftree.cpp
 *
 */

/***********************************************************************/

// comment out next line for fewer display
#define BOOTCHUNKEDSEQ_CHECK 1

// comment out next line for fewer display
// #define BOOTCHUNKEDSEQ_PRINT 1


/***********************************************************************/

#include "itemsearch.hpp"
#include "cmdline.hpp"
//#include "bootchunkedseq.hpp"
#include "bootchunkedseqnew.hpp"
#include "test_seq.hpp"

using namespace pasl::data;
using namespace pasl::data::chunkedseq;


/***********************************************************************/
// Specification of integer items

// Structure storing an integer

class IntItem {
public:
  int value;
  IntItem() : value(0) {}
  size_t get_cached() const {
    return 1;
  }
};

// Helper functions to allocate, deallocate, and print items

class IntItemGenerator {
public:
  static IntItem* from_int(int n) {
    IntItem* x = new IntItem();
    x->value = n;
    return x;
  }

  static int to_int(const IntItem*& x) {
    return x->value;
  }
  
  static int to_int(IntItem*& x) {
    return x->value;
  }

  static void print(const IntItem* x) {
    printf("%d", IntItemGenerator::to_int(x));
  }

  static void free(IntItem*& x) { 
    delete x;
  }

};

/***********************************************************************/
// Specialization of the sequence to pointers on integer items

template<int Chunk_capacity>
class IntPointerSeqOf {
public:
  using value_type = IntItem*;
  
  template <class Item, class Size>
  class cache_size {
  public:
    
    using size_type = Size;
    using value_type = Item;
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
  

  using sized_cache_measure = cache_size<value_type, size_t>;
  using seq_type = bootchunkedseq::cdeque<IntItem, Chunk_capacity, sized_cache_measure>;
  using measure_type = typename sized_cache_measure::measure_type;
  using measured_type = typename sized_cache_measure::measured_type;
  using size_type = size_t;
  using self_type = IntPointerSeqOf<Chunk_capacity>;

  measure_type meas;
  seq_type seq;

public:

  inline size_t size() { 
    return seq.get_cached();
  }

  inline void push_front(const value_type& x) {
    seq.push_front(x, meas(x));
  }

  inline void push_back(const value_type& x) {
    seq.push_back(x, meas(x));
  }

  inline value_type pop_front() {
    return seq.pop_front();
  }

  inline value_type pop_back() {
    return seq.pop_back();
  }

  void concat(self_type& other) {
    seq.concat(other.seq);
  }

  // Todo: fix!
  class middle_measured_fields {
  public:
    static size_type& size(measured_type& m) {
      return m;
    }
    static size_type csize(measured_type m) {
      return m;
    }
  };

  /* predicate version
  void split(size_t index, self_type& other) {
    using pred_type = itemsearch::less_than_or_eq_by_position<measured_type, size_t, middle_measured_fields>;
    pred_type p(index);
    value_type v;
    seq.split(p, 0, v, other.seq);
    other.push_front(v);
  }
  */

  void split(size_t index, self_type& other) {
    seq.split(index, other.seq);
    /* value_type v;
    seq.split(index, v, other.seq);
    other.push_front(v);*/
  }

  class CachePrinter {
  public:
    static void print(typename seq_type::measured_type& c) {
      printf("%d", c);
    }
  };

  template <class ItemPrinter>
  void print()  {
    seq.template print<ItemPrinter>();
  }
  
  void check() {
    seq.check();
  }

};



/***********************************************************************/

int main(int argc, char** argv) {
  pasl::util::cmdline::set(argc, argv);

  size_t chunk_capacity = (size_t) pasl::util::cmdline::parse_or_default_int("chunk_capacity", 2);
  if (chunk_capacity == 2)
    TestSeq<IntPointerSeqOf<2>,IntItem*,IntItemGenerator>::execute_test();
  else if (chunk_capacity == 4)
    TestSeq<IntPointerSeqOf<4>,IntItem*,IntItemGenerator>::execute_test();
  else
    pasl::util::cmdline::die("unsupported capacity");

  return 0;
}


/***********************************************************************/
