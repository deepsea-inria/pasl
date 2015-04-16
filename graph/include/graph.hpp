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
#include <cstring>

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
  memset(array, val, sz*sizeof(Number));
  // for (Size i = Size(0); i < sz; i++)
  //   array[i] = val;
}
 
template <class Number, class Size>
void fill_array_par_seq(std::atomic<Number>* array, Size sz, Number val) {
  memset(array, val, sz*sizeof(Number));
  // for (Size i = Size(0); i < sz; i++) {
  //   array[i].store(val, std::memory_order_relaxed);
  // }
}

static inline void pmemset(char * ptr, int value, size_t num) {
  const size_t cutoff = 100000;
  if (num <= cutoff) {
    memset(ptr, value, num);
  } else {
    long m = num/2;
    sched::native::fork2([&] {
      pmemset(ptr, value, m);
    }, [&] {
      pmemset(ptr+m, value, num-m);
    });
  }
}
  

template <class Number, class Size>
void fill_array_par(std::atomic<Number>* array, Size sz, Number val) {
  pmemset((char*)array, val, sz*sizeof(Number));
}
/*
#ifdef FILL_ARRAY_PAR_SEQ
  fill_array_par_seq(array, sz, val);
#else
  sched::native::parallel_for(Size(0), sz, [&] (Size i) {
    array[i].store(val, std::memory_order_relaxed);
  });
#endif
*/

} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_GRAPH_H_ */
