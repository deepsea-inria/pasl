/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Example use of the chunked sequence
 * \file map_1.cpp
 * \example map_1.cpp
 * \ingroup chunkedseq
 * \ingroup cached_measurement
 *
 */

//! [map_example]
// accessing mapped values
#include <iostream>
#include <string>

#include "map.hpp"

int main () {
  pasl::data::map::map<char,std::string> mymap;
  
  mymap['a']="an element";
  mymap['b']="another element";
  mymap['c']=mymap['b'];
  
  std::cout << "mymap['a'] is " << mymap['a'] << '\n';
  std::cout << "mymap['b'] is " << mymap['b'] << '\n';
  std::cout << "mymap['c'] is " << mymap['c'] << '\n';
  std::cout << "mymap['d'] is " << mymap['d'] << '\n';
  
  std::cout << "mymap now contains " << mymap.size() << " elements.\n";
  
  return 0;
}
//! [map_example]