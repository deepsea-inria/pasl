/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Example use of the chunked sequence
 * \file chunkedseq_5.cpp
 * \example chunkedseq_5.cpp
 * \ingroup chunkedseq
 * \ingroup cached_measurement
 *
 */

//! [split_example]
#include <iostream>
#include <string>
#include <assert.h>

#include "chunkedseq.hpp"

using mydeque_type = pasl::data::chunkedseq::bootstrapped::deque<int>;

static
void myprint(mydeque_type& mydeque) {
  auto it = mydeque.begin();
  while (it != mydeque.end())
    std::cout << " " << *it++;
  std::cout << std::endl;
}

int main(int argc, const char * argv[]) {
  
  mydeque_type mydeque = { 0, 1, 2, 3, 4, 5 };
  mydeque_type mydeque2;
  
  mydeque.split(size_t(3), mydeque2);
  
  mydeque.pop_back();
  mydeque.push_back(8888);
  
  mydeque2.pop_front();
  mydeque2.push_front(9999);
  
  std::cout << "Just after split:" << std::endl;
  std::cout << "contents of mydeque:";
  myprint(mydeque);
  std::cout << "contents of mydeque2:";
  myprint(mydeque2);
  
  mydeque.concat(mydeque2);
  
  std::cout << "Just after merge:" << std::endl;
  std::cout << "contents of mydeque:";
  myprint(mydeque);
  std::cout << "contents of mydeque2:";
  myprint(mydeque2);
  
  return 0;
  
}
//! [split_example]
