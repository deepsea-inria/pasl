/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Example use of the chunked sequence
 * \file chunkedseq_2.cpp
 * \example chunkedseq_2.cpp
 * \ingroup chunkedseq
 * \ingroup cached_measurement
 *
 */

//! [deque_example]
#include <iostream>
#include <string>
#include <assert.h>

#include "chunkedseq.hpp"

int main(int argc, const char * argv[]) {
  
  using mydeque_type = pasl::data::chunkedseq::bootstrapped::deque<int>;

  const int nb = 5;
  
  mydeque_type mydeque;
  
  for (int i = 0; i < nb; i++)
    mydeque.push_back(i);
  for (int i = 0; i < nb; i++)
    mydeque.push_front(nb+i);
  
  assert(mydeque.size() == 2*nb);
  
  std::cout << "mydeque contains:";
  for (int i = 0; i < 2*nb; i++) {
    int v = (i % 2) ? mydeque.pop_front() : mydeque.pop_back();
    std::cout << " " << v;
  }
  std::cout << std::endl;
  
  assert(mydeque.empty());
  
  return 0;
  
}
//! [deque_example]
