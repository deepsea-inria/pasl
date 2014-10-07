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

//! [map_example2]
// accessing mapped values
#include <iostream>
#include <string>

#include "map.hpp"

int main ()
{
  pasl::data::map::map<char,int> mymap;
  pasl::data::map::map<char,int>::iterator it;
  
  // insert some values:
  mymap['a']=10;
  mymap['b']=20;
  mymap['c']=30;
  mymap['d']=40;
  mymap['e']=50;
  mymap['f']=60;
  
  it=mymap.find('b');
  mymap.erase (it);                   // erasing by iterator
  
  mymap.erase ('c');                  // erasing by key
  
  // show content:
  for (it=mymap.begin(); it!=mymap.end(); ++it)
    std::cout << (*it).first << " => " << (*it).second << '\n';
  
  return 0;
}
//! [map_example2]
