/*!
 * \file scan.cpp
 * \brief Quickcheck for scan
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include <set>

#include "pasl.hpp"
#include "prandgen.hpp"
#include "quickcheck.hpp"
#include "io.hpp"
#include "sequenceio.hpp"
#include "deterministichash.hpp"

/***********************************************************************/

namespace pasl {
namespace pctl {

/*---------------------------------------------------------------------*/
/* Quickcheck IO */

template <class Container>
std::ostream& operator<<(std::ostream& out, const container_wrapper<Container>& c) {
  out << c.c;
  return out;
}
  
template <class Item>
std::ostream& operator<<(std::ostream& out, const std::set<Item>& xs) {
  out << "{ ";
  for (auto it = xs.cbegin(); it != xs.cend(); it++) {
    out << (*it);
    auto it2 = it;
    it2++;
    if (it2 != xs.cend())
      out << ", ";
  }
  out << " }";
  return out;
}

/*---------------------------------------------------------------------*/
/* Quickcheck generators */

using value_type = int;

void generate(size_t nb, parray<value_type>& dst) {
  std::vector<value_type> vec;
  quickcheck::generate(nb, vec);
  { // generate only positive numbers
    for (auto it = vec.begin(); it != vec.end(); it++) {
      *it = std::abs(*it);
    }
  }
  { // add a randomly chosen number of duplicates to random positions
    int m = quickcheck::generateInRange(0, 1<<12);
    int nb_duplicates = std::min((int)vec.size(), m);
    for (long i = 0; i < nb_duplicates; i++) {
      int ix = quickcheck::generateInRange(0, (int)vec.size()-1);
      vec.push_back(vec[ix]);
    }
    int nb_swaps = std::min((int)vec.size(), 1<<10);
    for (long i = 0; i < nb_swaps; i++) {
      int p1 = quickcheck::generateInRange(0, (int)vec.size()-1);
      int p2 = quickcheck::generateInRange(0, (int)vec.size()-1);
      std::swap(vec[p1], vec[p2]);
    }
  }
  { // copy out result to target parray
    parray<value_type> tmp(vec.size(), [&] (long i) {
      return vec[i];
    });
    tmp.swap(dst);
  }
}

void generate(size_t nb, container_wrapper<parray<value_type>>& c) {
  generate(nb, c.c);
}


/*---------------------------------------------------------------------*/
/* Quickcheck properties */
  
using parray_wrapper = container_wrapper<parray<value_type>>;

class prop : public quickcheck::Property<parray_wrapper> {
public:
  
  bool holdsFor(const parray_wrapper& _in) {
    parray_wrapper in(_in);
    parray<value_type> vals = removeDuplicates(in.c);
    { // ensure that there are no duplicates
      std::set<value_type> set;
      for (auto it = vals.cbegin(); it != vals.cend(); it++) {
        auto v = set.insert(*it);
        if (! v.second) { // same item was already inserted => we had a duplicate
          return false;
        }
      }
    }
    { // ensure that there are no dropped items
      std::set<value_type> t;
      std::set<value_type> u;
      for (auto it = in.c.cbegin(); it != in.c.cend(); it++) {
        t.insert(*it);
      }
      for (auto it = vals.cbegin(); it != vals.cend(); it++) {
        u.insert(*it);
      }
      if (t != u) {
        std::cout << in.c << std::endl;
        std::cout << vals << std::endl;
        std::cout << t << std::endl;
        std::cout << u << std::endl;
        return false;
      }
    }
    return true;
  }
  
};

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (bool sequential) {
    int nb_tests = pasl::util::cmdline::parse_or_default_int("n", 1000);
    checkit<pasl::pctl::prop>(nb_tests, "deterministic hash is correct");
  });
  return 0;
}

/***********************************************************************/
