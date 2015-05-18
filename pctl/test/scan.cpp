/*!
 * \file scan.cpp
 * \brief Quickcheck for scan
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include "pasl.hpp"
#include "prandgen.hpp"
#include "quickcheck.hpp"
#include "io.hpp"

//#include "blockradixsort.hpp"

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

/*---------------------------------------------------------------------*/
/* Quickcheck generators */

using value_type = int;

const value_type loval = 0;
const value_type hival = 1<<20;

value_type random_value() {
  return quickcheck::generateInRange(loval, hival);
}

void generate(size_t nb, parray<value_type>& dst) {
  dst.resize(nb, loval);
  for (size_t i = 0; i < nb; i++) {
    dst[i] = random_value();
  }
}

void generate(size_t nb, pchunkedseq<value_type>& dst) {
  dst.clear();
  for (size_t i = 0; i < nb; i++) {
    dst.seq.push_back(random_value());
  }
}

void generate(size_t nb, container_wrapper<parray<value_type>>& c) {
  generate(nb, c.c);
}
  
void generate(size_t, scan_type& st) {
  int r = quickcheck::generateInRange(0, 3);
  if (r == 0) {
    st = forward_exclusive_scan;
  } else if (r == 1) {
    st = forward_inclusive_scan;
  } else if (r == 2) {
    st = backward_exclusive_scan;
  } else {
    st = backward_inclusive_scan;
  }
}

/*---------------------------------------------------------------------*/
/* Trusted functions */

namespace trusted {
  
  auto plus = [] (value_type x, value_type y) {
    return x + y;
  };
  
  value_type id = (value_type)0;
  
  parray<value_type> scan_seq(const parray<value_type>& xs, scan_type st) {
    using output_type = level3::cell_output<value_type, typeof(plus)>;
    output_type out(0L, plus);
    auto convert = [] (typename parray<value_type>::const_iterator it, value_type& dst) {
      dst = *it;
    };
    long n = xs.size();
    parray<value_type> result(n);
    level4::scan_seq(xs.cbegin(), xs.cend(), result.begin(), out, id, convert, st);
    return result;
  }
  
} // end namespace

/*---------------------------------------------------------------------*/
/* Quickcheck properties */

using parray_wrapper = container_wrapper<parray<value_type>>;

class scan_property : public quickcheck::Property<parray_wrapper, scan_type> {
public:

  bool holdsFor(const parray_wrapper& in, const scan_type& st) {
    auto t = trusted::scan_seq(in.c, st);
    auto u = scan(in.c.cbegin(), in.c.cend(), trusted::id, trusted::plus, st);
    return same_sequence(t.cbegin(), t.cend(), u.cbegin(), u.cend());
  }
  
};

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (bool sequential) {
    int nb_tests = pasl::util::cmdline::parse_or_default_int("n", 1000);
    checkit<pasl::pctl::scan_property>(nb_tests, "scan is correct");
  });
  return 0;
}

/***********************************************************************/
