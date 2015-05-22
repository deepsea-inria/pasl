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
#include "suffixarray.hpp"
#include "trigrams.hpp"
#include <string>

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
  
using value_type = unsigned char;
  
void generate(size_t nb, parray<value_type>& str) {
  str.resize(nb+1, 0);
  for (int i = 0; i < nb; i++) {
    quickcheck::generate(nb, str[i]);
  }
}
  
void generate(size_t nb, container_wrapper<parray<value_type>>& c) {
  generate(nb, c.c);
}


/*---------------------------------------------------------------------*/
/* Quickcheck properties */
  
typedef unsigned char uchar;

bool strLessBounded (uchar* s1c, uchar* s2c, intT n) {
  uchar* s1 = s1c, *s2 = s2c;
  while (*s1 && *s1==*s2) {
    if (n-- < 0) return 1;
    s1++;
    s2++;};
  return (*s1 < *s2);
}

bool isPermutation(intT *SA, intT n) {
  parray<intT> seen(n, (intT)0);

  parallel_for((intT)0, n, [&] (intT i) {
    seen[SA[i]] = 1;
  });
  intT nseen = reduce(seen.cbegin(), seen.cend(), 0, [&] (intT x, intT y) {
    return x + y;
  });
  if (nseen != n) {
    std::cout << "hi" << std::endl;
  }
  return (nseen == n || nseen+1==n);
}

bool isSorted(intT *SA, uchar *s, intT n) {
  int checkLen = 100;
  intT error = n;
  parallel_for((intT)0, n-1, [&] (intT i) {
    if (!strLessBounded(s+SA[i], s+SA[i+1], checkLen)) {
      //cout.write((char*) s+SA[i],checkLen); cout << endl;
      //cout.write((char*) s+SA[i+1],min(checkLen,n-SA[i+1]));cout << endl;
      utils::writeMin(&error,i);
    }
  });
  if (error != n) {
    cout << "Suffix Array Check: not sorted at i = "
    << error+1 << endl;
    return 0;
  }
  return 0;
}

using parray_wrapper = container_wrapper<parray<value_type>>;

class suffixarray_property : public quickcheck::Property<parray_wrapper> {
public:
  
  bool holdsFor(const parray_wrapper& _in) {
    parray_wrapper in(_in);
    intT m = (intT)in.c.size();
    parray<intT> SA = suffixArray(in.c.begin(), m);
    intT n = (intT)SA.size();
    if (n != m + 1) {
      return false;
    }
    if (! isPermutation(SA.begin(), m)) {
      return false;
    }
    if (isSorted(SA.begin(), in.c.begin(), m)) {
      return false;
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
    checkit<pasl::pctl::suffixarray_property>(nb_tests, "suffixarray is correct");
  });
  return 0;
}

/***********************************************************************/
