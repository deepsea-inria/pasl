/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Example use of the chunked sequence
 * \file chunkedseq_1.cpp
 * \example chunkedseq_1.cpp
 * \ingroup chunkedseq
 *
 */

//! [chunkedseq_example1]
#include <iostream>

#include "chunkedseq.hpp"

using cbdeque = pasl::data::chunkedseq::bootstrapped::deque<long>;

// moves items which satisfy a given predicate p from src to dst
// preserving original order of items in src
template <class Predicate_function>
void pkeep_if(cbdeque& dst, cbdeque& src, const Predicate_function& p) {

  const int cutoff = 8096;
  
  long sz = src.size();

  if (sz <= cutoff) {

    // compute result in a sequential fashion
    while (sz-- > 0) {
      long item = src.pop_back();
      if (p(item))
        dst.push_front(item);
    }

  } else {

    cbdeque src2;
    cbdeque dst2;

    // divide the input evenly in two halves
    size_t mid = sz / 2;
    src.split(mid, src2);

    // recurse on subproblems
    // calls can execute in parallel
    pkeep_if(dst,  src,  p);
    pkeep_if(dst2, src2, p);

    // combine results (after parallel calls complete)
    dst.concat(dst2);

  }
}

int main(int argc, const char * argv[]) {

  cbdeque src;
  cbdeque dst;

  const long n = 1000000;

  // fill the source container with [1, ..., 2n]
  for (long i = 1; i <= 2*n; i++)
    src.push_back(i);

  // leave src empty and dst = [1, 3, 5, ... n-1]
  pkeep_if(dst, src, [] (long x) { return x%2 == 1; });

  assert(src.empty());
  assert(dst.size() == n);

  // calculate the sum
  long sum = 0;
  while (! dst.empty())
    sum += dst.pop_front();

  // the sum of n consecutive odd integers starting from 1 equals n^2
  assert(sum == n*n);
  std::cout << "sum = " << sum << std::endl;

  return 0;

}
//! [chunkedseq_example1]
