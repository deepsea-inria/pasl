/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Example use of the chunked sequence
 * \file chunkedseq_4.cpp
 * \example chunkedseq_4.cpp
 * \ingroup chunkedseq
 * \ingroup cached_measurement
 *
 */

//! [bag_example]
#include <iostream>
#include <string>

#include "chunkedbag.hpp"

int main(int argc, const char * argv[]) {
  
  using mybag_type = pasl::data::chunkedseq::bootstrapped::bagopt<int>;
  
  mybag_type mybag = { 0, 1, 2, 3, 4 };

  std::cout << "mybag contains:";
  while (! mybag.empty())
    std::cout << " " << mybag.pop();
  std::cout << std::endl;
  
  return 0;
  
}
//! [bag_example]
