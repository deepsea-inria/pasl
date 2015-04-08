/*!
 * \file mergesort.cpp
 * \brief Benchmarking script for parallel sorting algorithms
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include "benchmark.hpp"
#include "psort.hpp"
#include "io.hpp"

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Preliminaries */

namespace pasl {
namespace pctl {

using value_type = int;

using pchunkedseq_type = pchunkedseq::pchunkedseq<value_type>;
using parray_type = parray::parray<value_type>;

template <class Container>
class container_wrapper {
public:
  Container c;
};

template <class Container>
std::ostream& operator<<(std::ostream& out, const container_wrapper<Container>& c);

template <class Container>
void generate(size_t nb, container_wrapper<Container>& c);

} // end namespace
} // end namespace

// This header must be included after the above definitions
#include "quickcheck.hh"

/*---------------------------------------------------------------------*/
/* Quick check framework */

namespace pasl {
namespace pctl {

using value_type = int;

using pchunkedseq_type = pchunkedseq::pchunkedseq<value_type>;
using parray_type = parray::parray<value_type>;

template <class Container>
std::ostream& operator<<(std::ostream& out, const container_wrapper<Container>& c) {
  out << c.c;
  return out;
}

const value_type loval = 0;
const value_type hival = INT_MAX;

value_type random_value() {
  return quickcheck::generateInRange(loval, hival);
}

template <class Item>
void generate(size_t nb, parray::parray<Item>& dst) {
  dst.resize(nb, loval);
  for (size_t i = 0; i < nb; i++) {
    dst[i] = random_value();
  }
}

template <class Item>
void generate(size_t nb, pchunkedseq::pchunkedseq<Item>& dst) {
  if (nb == 0) {
    return;
  }
  parray_type tmp;
  generate(nb, tmp);
  dst.seq.clear();
  dst.seq.pushn_back(&tmp[0], nb);
}

template <class Container>
void generate(size_t nb, container_wrapper<Container>& c) {
  generate(nb, c.c);
}

template <class Iter>
bool same_sequence(Iter xs_lo, Iter xs_hi, Iter ys_lo, Iter ys_hi) {
  if (xs_hi-xs_lo != ys_hi-ys_lo) {
    return false;
  }
  Iter ys_it = ys_lo;
  for (Iter xs_it = xs_lo; xs_it != xs_hi; xs_it++, ys_it++) {
    if (*xs_it != *ys_it) {
      return false;
    }
  }
  return  true;
}
  
bool verbose = true;

template <class Sequence, class Trusted_sort, class Untrusted_sort>
class sort_property : public quickcheck::Property<container_wrapper<Sequence>> {
public:
  
  Trusted_sort trusted_sort;
  Untrusted_sort untrusted_sort;
  
  bool holdsFor(const container_wrapper<Sequence>& in) {
    Sequence trusted(in.c);
    Sequence untrusted(in.c);
    trusted_sort(trusted);
    untrusted_sort(untrusted);
    bool same = same_sequence(trusted.begin(), trusted.end(), untrusted.begin(), untrusted.end());
    if (verbose && !same) {
      std::cout << "input=     " << in.c << std::endl;
      std::cout << "trusted=   " << trusted << std::endl;
      std::cout << "untrusted= " << untrusted << std::endl;
    }
    return same;
  }
  
};

class pchunkedseq_mergesort {
public:
  
  void operator()(pchunkedseq_type& xs) {
    pchunkedseq_type tmp = sort::mergesort(xs);
    tmp.seq.swap(xs.seq);
  }
  
};

class pchunkedseq_trusted_sort {
public:
  
  void operator()(pchunkedseq_type& xs) {
    long nb = xs.seq.size();
    parray_type tmp(nb);
    xs.seq.backn(tmp.begin(), nb);
    std::sort(tmp.begin(), tmp.end());
    xs.clear();
    xs.seq.pushn_back(tmp.cbegin(), nb);
  }
  
};

using pchunkedseq_mergesort_property = sort_property<pchunkedseq_type, pchunkedseq_trusted_sort, pchunkedseq_mergesort>;
  
class parray_mergesort {
public:
  
  void operator()(parray_type& xs) {
    sort::mergesort(xs);
  }
  
};

class parray_trusted_sort {
public:
  
  void operator()(parray_type& xs) {
    std::sort(xs.begin(), xs.end());
  }
  
};

using parray_mergesort_property = sort_property<parray_type, parray_trusted_sort, parray_mergesort>;

int nb_tests = 1000;

template <class Property>
void checkit(std::string msg) {
  assert(nb_tests > 0);
  quickcheck::check<Property>(msg.c_str(), nb_tests);
}

void doit() {
//  checkit<pchunkedseq_mergesort_property>("pchunkedseq mergesort");
  checkit<parray_mergesort_property>("parray mergesort");
}

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  
  auto init = [&] {
    
  };
  auto run = [&] (bool sequential) {
    pasl::pctl::doit();
  };
  auto output = [&] {
    
  };
  auto destroy = [&] {
    ;
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
  return 0;
}

/***********************************************************************/
