/*!
 * \file mergesort.cpp
 * \brief Benchmarking script for parallel sorting algorithms
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include "pasl.hpp"
#include "psort.hpp"
#include "io.hpp"
#include "quickcheck.hpp"
#include "samplesort.hpp"

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Quick check framework */

namespace pasl {
namespace pctl {

using value_type = int;

using pchunkedseq_type = pchunkedseq<value_type>;
using parray_type = parray<value_type>;

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
void generate(size_t nb, parray<Item>& dst) {
  if (quickcheck::generateInRange(0, 2) == 0) {
    nb *= quickcheck::generateInRange(1, 100);
  }
  dst.resize(nb, loval);
  for (size_t i = 0; i < nb; i++) {
    dst[i] = random_value();
  }
}

template <class Item>
void generate(size_t nb, pchunkedseq<Item>& dst) {
  dst.clear();
  for (size_t i = 0; i < nb; i++) {
    dst.seq.push_back(random_value());
  }
}

template <class Container>
void generate(size_t nb, container_wrapper<Container>& c) {
  generate(nb, c.c);
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
    pchunkedseq_type tmp = sort::mergesort(xs, std::less<value_type>());
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
  
auto compare = [] (value_type x, value_type y) {
  return x < y;
};
  
class parray_mergesort {
public:
  
  void operator()(parray_type& xs) {
    //sort::mergesort(xs, compare);
    sort::mergesort(xs.begin(), xs.end(), compare);
  }
  
};
  
class pbbs_samplesort {
public:
  
  void operator()(parray_type& xs) {
    sampleSort(xs.begin(), (int)xs.size(), compare);
  }
  
};

class parray_trusted_sort {
public:
  
  void operator()(parray_type& xs) {
    std::sort(xs.begin(), xs.end(), compare);
  }
  
};

using pchunkedseq_mergesort_property = sort_property<pchunkedseq_type, pchunkedseq_trusted_sort, pchunkedseq_mergesort>;
using parray_mergesort_property = sort_property<parray_type, parray_trusted_sort, parray_mergesort>;
using pbbs_samplesort_property = sort_property<parray_type, parray_trusted_sort, pbbs_samplesort>;

int nb_tests = 1000;

template <class Property>
void checkit(std::string msg) {
  assert(nb_tests > 0);
  quickcheck::check<Property>(msg.c_str(), nb_tests);
}

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (bool sequential) {
    int nb_tests = pasl::util::cmdline::parse_or_default_int("n", 1000);
    //checkit<pasl::pctl::pbbs_samplesort_property>(nb_tests, "pbbs samplesort is correct");
    checkit<pasl::pctl::parray_mergesort_property>(nb_tests, "parray mergesort is correct");
  });
  return 0;
}

/***********************************************************************/
