/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Example use of the chunked sequence for weighted split
 * \file weighted_split.cpp
 * \example weighted_split.cpp
 * \ingroup chunkedseq
 * \ingroup cached_measurement
 *
 */

//! [weighted_split_example]
#include <iostream>
#include <string>

#include "chunkedseq.hpp"

namespace cachedmeasure = pasl::data::cachedmeasure;
namespace chunkedseq = pasl::data::chunkedseq::bootstrapped;

const int chunk_capacity = 512;

int main(int argc, const char * argv[]) {
  
  using value_type = std::string;
  using weight_type = int;
  
  class my_weight_fct {
  public:
    // returns 1 if the length of the string is an even number; 0 otherwise
    weight_type operator()(const value_type& str) const {
      return (str.size() % 2 == 0) ? 1 : 0;
    }
  };
  
  using my_cachedmeasure_type =
    cachedmeasure::weight<value_type, weight_type, size_t, my_weight_fct>;

  using my_weighted_deque_type =
    chunkedseq::deque<value_type, chunk_capacity, my_cachedmeasure_type>;
  
  my_weighted_deque_type d = { "Let's", "divide", "this", "sequence", "of",
                               "strings", "into", "two", "pieces" };  
  
  weight_type nb_even_length_strings = d.get_cached();
  std::cout << "nb even-length strings: " << nb_even_length_strings << std::endl;
  
  my_weighted_deque_type f;
  
  d.split([=] (weight_type v) { return v >= nb_even_length_strings/2; }, f);
  
  std::cout << "d = " << std::endl;
  d.for_each([] (value_type& s) { std::cout << s << " "; });
  std::cout << std::endl;
  std::cout << std::endl;
  
  std::cout << "f = " << std::endl;
  f.for_each([] (value_type& s) { std::cout << s << " "; });
  std::cout << std::endl;
  
  return 0;
  
}
//! [weighted_split_example]
