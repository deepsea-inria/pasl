/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file graph.hpp
 *
 */

#ifndef _PASL_GRAPH_H_
#define _PASL_GRAPH_H_

#include <assert.h>
#include <algorithm>

#include "atomic.hpp"
#include "container.hpp"
#include "native.hpp"

/***********************************************************************/

namespace pasl {
namespace graph {
  
using edgeid_type = size_t;

template <class Vertex_id>
class graph_constants {
public:
  static constexpr Vertex_id unknown_vtxid = Vertex_id(-1);
};

template <class Vertex_id>
void check_vertex(Vertex_id v, Vertex_id nb_vertices) {
  assert(v >= 0);
  assert(v < nb_vertices);
}

template <class Number, class Size>
void fill_array_seq(Number* array, Size sz, Number val) {
  for (Size i = Size(0); i < sz; i++)
    array[i] = val;
}
 
template <class Number, class Size>
void fill_array_par_seq(std::atomic<Number>* array, Size sz, Number val) {
  for (Size i = Size(0); i < sz; i++) {
    array[i].store(val, std::memory_order_relaxed);
  }
}

template <class Number, class Size>
void fill_array_par(std::atomic<Number>* array, Size sz, Number val) {
#ifdef FILL_ARRAY_PAR_SEQ
  fill_array_par_seq(array, sz, val);
#else
  sched::native::parallel_for(Size(0), sz, [&] (Size i) {
    array[i].store(val, std::memory_order_relaxed);
  });
#endif
}


template <class Vertex_id>
class MarksInArray {
public:
  int* marks;

  MarksInArray() {
    marks = NULL;
  }
  /*
  ~ MarksInArray() {
    if (marks != NULL) {
      data:myfree(marks);
    }
  }
  */
  void init(Vertex_id nb) {
    marks = data::mynew_array<int>(nb);
  }

  bool operator[] (Vertex_id i) {
    return (marks[i] != 0);
  }

  void mark(Vertex_id i) {
    marks[i] = 1;
  }

};


template <class Vertex_id>
class MarksInBitVector {
public:  
  uint64_t* marks;

  MarksInBitVector() {
    marks = NULL;
  }
  /*
  ~ MarksInBitVector() {
    if (marks != NULL) {
      data:myfree(marks);
    }
  }
  */
  void init(Vertex_id nb) {
    marks = data::mynew_array<uint64_t>(nb / 64);
  }

  bool operator[] (Vertex_id i) {
    uint64_t v = marks[i / 64];
    uint64_t m = 1 << (i % 64);
    return ((v & m) != 0);
  }

  void mark(Vertex_id i) {
    uint64_t m = 1 << (i % 64);
    marks[i / 64] |= m;
  }

};


template <class Vertex_id>
class MarksInArrayAtomic {
public:  
  std::atomic<int>* marks;

  MarksInArrayAtomic() {
    marks = NULL;
  }
  /*
  ~ MarksInArrayAtomic() {
    if (marks != NULL) {
      data:myfree(marks);
    }
  }
  */
  void init(Vertex_id nb) {
    marks = data::mynew_array<int>(nb);
  }

  bool operator[] (Vertex_id i) {
    return (marks[i].load(std::memory_order_relaxed) != 0);
  }

  void mark(Vertex_id i) {
    return marks[i].store(1, std::memory_order_relaxed);
  }

  bool testAndMark(Vertex_id i) {
    return (! marks[i].compare_exchange_strong(0, 1));
  } 

};


template <class Vertex_id>
class MarksInBitVectorAtomic {
public:  
  //  std::atomic<uint64_t>* marks;
  unsigned* marks;

  int bits = 32;

  MarksInBitVectorAtomic() {
    marks = nullptr;
  }
  /*
  ~ MarksInBitVectorAtomic() {
    if (marks != NULL) {
      data:myfree(marks);
    }
  }
  */
  void init(Vertex_id nb) {
    marks = data::mynew_array<unsigned>((nb+1) / bits);
  }

  bool operator[] (Vertex_id i) {
    //    unsigned v = marks[i / bits].load(std::memory_order_relaxed);
    unsigned v = marks[i/bits];
    unsigned m = 1 << (i % bits);
    return ((v & m) != 0);
  }
  
  void mark(Vertex_id i) {
    unsigned* v = &marks[i / bits];
    unsigned m = 1 << (i % bits);
    __sync_fetch_and_or(v, m);
  }

  // returns true if already visited; false otherwise
  bool testAndMark(Vertex_id i) {
    volatile unsigned* v = &marks[i / bits];
    unsigned m = 1 << (i % bits);
    if ((*v & m) != 0)
      return true;
    unsigned vold = __sync_fetch_and_or(v, m);
    return ((vold & m) != 0);
  }

};
  


} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_GRAPH_H_ */
