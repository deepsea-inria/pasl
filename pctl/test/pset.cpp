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
#include "quickcheck.hpp"
#include "io.hpp"
#include "pset.hpp"

/***********************************************************************/

namespace pasl {
namespace pctl {
  
using value_type = int;
using trusted_set_type = std::set<value_type>;
using untrusted_set_type = pset<value_type>;
using container_pair = std::pair<trusted_set_type, untrusted_set_type>;

/*---------------------------------------------------------------------*/
/* Quickcheck IO */

template <class Item>
std::ostream& operator<<(std::ostream& out, const std::set<Item>& xs) {
  out << "{ ";
  for (auto it = xs.cbegin(); it != xs.cend(); it++) {
    Item x = *it;
    out << x;
    auto it2 = it;
    it2++;
    if (it2 != xs.cend())
      out << ", ";
  }
  out << " }";
  return out;
}

std::ostream& operator<<(std::ostream& out, const container_pair& c) {
  out << c.first;
  return out;
}

/*---------------------------------------------------------------------*/
/* Quickcheck generators */

const value_type loval = 0;
const value_type hival = 1<<10;

value_type random_value() {
  return quickcheck::generateInRange(loval, hival);
}

void generate(size_t nb, container_pair& c) {
  for (long i = 0; i < nb; i++) {
    value_type x = random_value();
    c.first.insert(x);
    c.second.insert(x);
  }
}

/*---------------------------------------------------------------------*/
/* Trusted functions */

namespace trusted {
  
bool same_set(const container_pair& c) {
  if (c.first.size() != c.second.size()) {
    return false;
  }
  auto it2 = c.second.cbegin();
  for (auto it = c.first.cbegin(); it != c.first.cend(); it++, it2++) {
    value_type x1 = *it;
    value_type x2 = *it2;
    if (x1 != x2) {
      std::cout << "trusted   = " << c.first << std::endl;;
      std::cout << "untrusted = " << c.second << std::endl;;
      return false;
    }
  }
  return true;
}
  
void merge(container_pair& c1, container_pair& c2) {
  for (auto it = c2.first.cbegin(); it != c2.first.cend(); it++) {
    c1.first.insert(*it);
  }
  c2.first.clear();
  c1.second.merge(c2.second);
}
  
} // end namespace

/*---------------------------------------------------------------------*/
/* Quickcheck properties */

class insert_property : public quickcheck::Property<container_pair> {
public:
  
  bool holdsFor(const container_pair& in) {
    container_pair c(in);
    value_type x = random_value();
    c.first.insert(x);
    c.second.insert(x);
    return trusted::same_set(c);
  }
  
};
  
class erase_property : public quickcheck::Property<container_pair> {
public:
  
  bool holdsFor(const container_pair& in) {
    container_pair c(in);
    assert(trusted::same_set(c));
    int ix = quickcheck::generateInRange(0, (int)c.first.size()-1);
    auto it = c.first.cbegin();
    for (int i = 0; i < ix; i++, it++) {
    }
    value_type x = *it;
    c.first.erase(x);
    c.second.erase(x);
    return trusted::same_set(c);
  }
  
};
  
class merge_property : public quickcheck::Property<container_pair, container_pair> {
public:
  
  bool holdsFor(const container_pair& _in1, const container_pair& _in2) {
    container_pair in1(_in1);
    container_pair in2(_in2);
    assert(trusted::same_set(in1));
    assert(trusted::same_set(in2));
    trusted::merge(in1, in2);
    bool b1 = trusted::same_set(in1);
    bool b2 = trusted::same_set(in2);
    return b1 && b2;
  }
  
};

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (bool sequential) {
    int nb_tests = pasl::util::cmdline::parse_or_default_int("n", 1000);
    //checkit<pasl::pctl::insert_property>(nb_tests, "pset insert is correct");
    //checkit<pasl::pctl::erase_property>(nb_tests, "pset erase is correct");
    checkit<pasl::pctl::merge_property>(nb_tests, "pset merge is correct");

  });
  return 0;
}

/***********************************************************************/
