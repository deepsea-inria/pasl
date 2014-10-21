/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file check.cpp
 * \brief Unit testing driver
 *
 */

#include "benchmark.hpp"
#include "dup.hpp"
#include "string.hpp"
#include "sort.hpp"
#include "quickcheck.hh"

/***********************************************************************/

long nb_tests;

template <class Property>
void checkit(std::string msg) {
  quickcheck::check<Property>(msg.c_str(), nb_tests);
}

bool same_array(const_array_ref xs, const_array_ref ys) {
  if (xs.size() != ys.size())
    return false;
  for (long i = 0; i < xs.size(); i++)
    if (xs[i] != ys[i])
      return false;
  return true;
}

array array_of_vector(const std::vector<value_type>& vec) {
  return tabulate([&] (long i) { return vec[i]; }, vec.size());
}

template <class Trusted_sort_fct, class Untrusted_sort_fct>
class sort_correct : public quickcheck::Property<std::vector<value_type>> {
public:
  
  Trusted_sort_fct trusted_sort;
  Untrusted_sort_fct untrusted_sort;
  
  bool holdsFor(const std::vector<value_type>& vec) {
    array xs = array_of_vector(vec);
    return same_array(trusted_sort(xs), untrusted_sort(xs));
  }
  
};

void check_sort() {
  pasl::util::cmdline::argmap_dispatch c;
  class trusted_fct {
  public:
    array operator()(const_array_ref xs) {
      return seqsort(xs);
    }
  };
  c.add("mergesort", [&] {
    class untrusted_fct {
    public:
      array operator()(const_array_ref xs) {
        return mergesort(xs);
      }
    };
    using property_type = sort_correct<trusted_fct, untrusted_fct>;
    checkit<property_type>("checking mergesort");
  });
  c.add("quicksort", [&] {
    class untrusted_fct {
    public:
      array operator()(const_array_ref xs) {
        return quicksort(xs);
      }
    };
    using property_type = sort_correct<trusted_fct, untrusted_fct>;
    checkit<property_type>("checking quicksort");
  });
  c.find_by_arg("algo")();
}

void check() {
  nb_tests = pasl::util::cmdline::parse_or_default_long("nb_tests", 100);
  pasl::util::cmdline::argmap_dispatch c;
  c.add("sort", std::bind(check_sort));
  c.find_by_arg("check")();
}

/*---------------------------------------------------------------------*/
/* PASL Driver */

int main(int argc, char** argv) {
  
  auto init = [&] {
    
  };
  auto run = [&] (bool) {
    check();
  };
  auto output = [&] {
  };
  auto destroy = [&] {
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}

/***********************************************************************/
