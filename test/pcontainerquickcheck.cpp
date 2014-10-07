/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file pcontainerquickcheck.cpp
 *
 */

#include <fstream>

#include "pcontainer.hpp"
#include "benchmark.hpp"

/***********************************************************************/

namespace quickcheck {

using namespace pasl::data;

/*---------------------------------------------------------------------*/

template <class Container>
std::ostream& generic_print_container(std::ostream& out, const Container& seq);

template <
  class Configuration,
  template <
    class _Chunkedseq,
    class _Configuration
  >
class Iterator
>
std::ostream& operator<<(std::ostream& out, const chunkedseq::chunkedseqbase<Configuration, Iterator>& seq);

template <class Item>
void generate(size_t& nb, pcontainer::stack<Item>& dst);

template <class Item>
void generate(size_t& nb, pcontainer::deque<Item>& dst);

} // end namespace

#include "quickcheck.hh"

namespace quickcheck {
  
/*---------------------------------------------------------------------*/

template <class Container>
std::ostream& generic_print_container(std::ostream& out, const Container& seq) {
  using value_type = typename Container::value_type;
  using size_type = typename Container::size_type;
  size_type sz = seq.size();
  out << "[";
  seq.for_each([&] (value_type x) {
    sz--;
    if (sz == 0)
      out << x;
    else
      out << x << ", ";
  });
  out << "]";
  return out;
}

template <
  class Configuration,
  template <
    class _Chunkedseq,
    class _Configuration
  >
  class Iterator
>
std::ostream& operator<<(std::ostream& out, const chunkedseq::chunkedseqbase<Configuration, Iterator>& seq) {
  return generic_print_container(out, seq);
}

/*---------------------------------------------------------------------*/
  
template <class Container>
void random_sequence_by_insert(size_t nb, Container& dst) {
  using value_type = typename Container::value_type;
  for (size_t i = 0; i < nb; i++) {
    int sz = (int)dst.size();
    int pos = (sz == 0) ? 0 : quickcheck::generateInRange(0, sz-1);
    value_type v;
    quickcheck::generate(1<<15, v);
    dst.insert(dst.begin() + pos, v);
  }
}

template <class Item>
void generate(size_t& nb, pcontainer::stack<Item>& dst) {
  random_sequence_by_insert(nb, dst);
}

template <class Item>
void generate(size_t& nb, pcontainer::deque<Item>& dst) {
  random_sequence_by_insert(nb, dst);
}
  
} // end namespace

namespace pasl {
namespace data {
  
/*---------------------------------------------------------------------*/

template <class Item>
std::ostream& operator<<(std::ostream& out, const pointer_seq<Item>& seq);

template <class Item>
std::ostream& operator<<(std::ostream& out, const pointer_seq<Item>& seq) {
  return quickcheck::generic_print_container(out, seq);
}
  
template <
class Configuration,
template <
class _Chunkedseq,
class _Configuration
>
class Iterator
>
std::ostream& operator<<(std::ostream& out, const chunkedseq::chunkedseqbase<Configuration, Iterator>& seq) {
  return quickcheck::generic_print_container(out, seq);
}
  
template <class Container_src>
class prop_transfer_contents_to_array_correct : public quickcheck::Property<Container_src> {
public:
  
  using container_type = Container_src;
  using value_type = typename container_type::value_type;
  using size_type = typename container_type::size_type;
  
  bool holdsFor(const container_type& _cont) {
    container_type cont(_cont);
    container_type cont2(_cont);
    size_type sz = cont.size();
    value_type* array = new value_type[sz];
    pcontainer::transfer_contents_to_array(cont, array);
    bool ok = true;
    for (size_type i = 0; i < sz; i++) {
      if (_cont[i] != array[i]) {
        ok = false;
        break;
      }
    }
    if (! ok) {
      pointer_seq<value_type> seq(array, sz);
      std::cout << " orig:\n" << cont2 << std::endl;
      std::cout << "array:\n" << seq << std::endl;
    }
    free(array);
    return ok;
  }
  
};
  
/*---------------------------------------------------------------------*/
  
int nb_tests;
  
void check_transfer_contents_to_array() {
  prop_transfer_contents_to_array_correct<pcontainer::deque<int>> prop;
  prop.check(nb_tests);
}
  
} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

using namespace pasl;
using namespace pasl::data;

int main(int argc, char ** argv) {
  
  auto init = [&] {
    nb_tests = pasl::util::cmdline::parse_or_default_int("nb_tests", 1000);
  };
  auto run = [&] (bool sequential) {
    pasl::util::cmdline::argmap_dispatch c;
    c.add("transfer_contents_to_array",  [] { check_transfer_contents_to_array(); });
    pasl::util::cmdline::dispatch_by_argmap_with_default_all(c, "test");
  };
  auto output = [&] {
    std::cout << "All tests complete" << std::endl;
  };
  auto destroy = [&] {
    ;
  };
  sched::launch(argc, argv, init, run, output, destroy);
  
  return 0;
}

/***********************************************************************/
