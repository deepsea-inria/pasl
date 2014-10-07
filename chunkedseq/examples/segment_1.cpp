/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Example use of the chunked sequence
 * \file segment.cpp
 * \example segment.cpp
 * \ingroup chunkedseq
 * \ingroup cached_measurement
 *
 */

//! [segment_example]
#include <iostream>
#include <string>
#include <assert.h>

#include "chunkedseq.hpp"

int main(int argc, const char * argv[]) {
  
  const int chunk_size = 2;
  using mydeque_type = pasl::data::chunkedseq::bootstrapped::deque<int, chunk_size>;
  
  mydeque_type mydeque = { 0, 1, 2, 3, 4, 5 };
  
  std::cout << "mydeque contains:";
  // iterate over the segments in mydeque
  mydeque.for_each_segment([&] (int* begin, int* end) {
    // iterate over the items in the current segment
    int* p = begin;
    while (p != end)
      std::cout << " " << *p++;
  });
  std::cout << std::endl;
  
  using iterator = typename mydeque_type::iterator;
  using segment_type = typename mydeque_type::segment_type;
  
  // iterate over the items in the segment which contains the item at position 3
  iterator it = mydeque.begin() + 3;
  segment_type seg = it.get_segment();
  
  std::cout << "the segment which contains mydeque[3] contains:";
  int* p = seg.begin;
  while (p != seg.end)
    std::cout << " " << *p++;
  std::cout << std::endl;
  
  std::cout << "mydeque[3]=" << *seg.middle << std::endl;
  
  return 0;
  
}
//! [segment_example]