/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Example use of the chunked sequence
 * \file chunkedseq_3.cpp
 * \example chunkedseq_3.cpp
 * \ingroup chunkedseq
 * \ingroup cached_measurement
 *
 */

//! [stack_example]
#include <iostream>
#include <string>
#include <assert.h>

#include "chunkedseq.hpp"

int main(int argc, const char * argv[]) {
  
  using mystack_type = pasl::data::chunkedseq::bootstrapped::stack<int>;
  
  mystack_type mystack = { 0, 1, 2, 3, 4 };
  
  std::cout << "mystack contains:";
  while (! mystack.empty())
    std::cout << " " << mystack.pop_back();
  std::cout << std::endl;
  
  return 0;
  
}
//! [stack_example]
