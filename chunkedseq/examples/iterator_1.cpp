/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Example use of the chunked sequence
 * \file iterator.cpp
 * \example iterator.cpp
 * \ingroup chunkedseq
 * \ingroup cached_measurement
 *
 */

//! [iterator_example]
#include <iostream>
#include <string>
#include <assert.h>

#include "chunkedseq.hpp"

int main(int argc, const char * argv[]) {
  
  using mydeque_type = pasl::data::chunkedseq::bootstrapped::deque<int>;
  using iterator = typename mydeque_type::iterator;
  
  mydeque_type mydeque = { 0, 1, 2, 3, 4 };
  
  std::cout << "mydeque contains:";
  iterator it = mydeque.begin();
  while (it != mydeque.end())
    std::cout << " " << *it++;
  
  std::cout << std::endl;
  
  return 0;
  
}
//! [iterator_example]