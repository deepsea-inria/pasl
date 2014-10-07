/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Example use of the chunked sequence
 * \file chunkedseq_6.cpp
 * \example chunkedseq_6.cpp
 * \ingroup chunkedseq
 * \ingroup cached_measurement
 *
 */

//! [pcopy_example]
#include <iostream>
#include <string>
#include <assert.h>

#include "chunkedseq.hpp"

template <class Chunkedseq>
void pcopy(typename Chunkedseq::iterator first,
           typename Chunkedseq::iterator last,
           Chunkedseq& destination) {
  using iterator = typename Chunkedseq::iterator;
  using ptr = typename Chunkedseq::const_pointer;
  
  const long cutoff = 8192;
  
  long sz = last.size() - first.size();
  
  if (sz <= cutoff) {
    
    // compute result in a sequential fashion
    Chunkedseq::for_each_segment(first, last, [&] (ptr lo, ptr hi) {
      destination.pushn_back(lo, hi-lo);
    });
    
  } else {
    
    // select split position to be the median
    iterator mid = first + (sz/2);
    
    Chunkedseq destination2;
    
    // recurse on subproblems
    // calls can execute in parallel
    pcopy(first, mid,  destination);
    pcopy(mid,   last, destination2);
    
    // combine results
    destination.concat(destination2);
    
  }
}

int main(int argc, const char * argv[]) {
  
  const int chunk_size = 2;
  using mydeque_type = pasl::data::chunkedseq::bootstrapped::deque<int, chunk_size>;
  
  mydeque_type mydeque = { 0, 1, 2, 3, 4, 5 };
  mydeque_type mydeque2;
  
  pcopy(mydeque.begin(), mydeque.end(), mydeque2);
  
  std::cout << "mydeque2 contains:";
  auto p = mydeque2.begin();
  while (p != mydeque2.end())
    std::cout << " " << *p++;
  std::cout << std::endl;
  
  return 0;
  
}
//! [pcopy_example]