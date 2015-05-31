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
#include "mis.hpp"
#include "pbbsio.hpp"

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
  

using graph_type = vertex<intT>;
  
void generate(size_t _nb, graph_type& dst) {

}

void generate(size_t nb, container_wrapper<graph_type>& c) {
  generate(nb, c.c);
}


/*---------------------------------------------------------------------*/
/* Quickcheck properties */

using namespace benchIO;

// Checks if valid maximal independent set
int checkMaximalIndependentSet(graph<intT> G, intT* Flags) {
  intT n = G.n;
  vertex<intT>* V = G.V;
  for (intT i=0; i < n; i++) {
    intT nflag;
    for (intT j=0; j < V[i].degree; j++) {
      intT ngh = V[i].Neighbors[j];
      if (Flags[ngh] == 1)
        if (Flags[i] == 1) {
          cout << "checkMaximalIndependentSet: bad edge "
          << i << "," << ngh << endl;
          return 1;
        } else nflag = 1;
    }
    if ((Flags[i] != 1) && (nflag != 1)) {
      cout << "checkMaximalIndependentSet: bad vertex " << i << endl;
      return 1;
    }
  }
  return 0;
}


class mis_property : public quickcheck::Property<graph_type> {
public:
  
  bool holdsFor(const graph_type& _in) {
    return true;
  }
  
};

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (bool sequential) {
    int nb_tests = pasl::util::cmdline::parse_or_default_int("n", 1000);
    checkit<pasl::pctl::mis_property>(nb_tests, "maximal independent set is correct");
  });
  return 0;
}

/***********************************************************************/
