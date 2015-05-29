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
#include "geometrydata.hpp"
#include "refine.hpp"

/***********************************************************************/

namespace pasl {
namespace pctl {

/*---------------------------------------------------------------------*/
/* Quickcheck IO */

template <class Container>
std::ostream& operator<<(std::ostream& out, const container_wrapper<Container>& c) {
  // out << c.c;
  return out;
}

/*---------------------------------------------------------------------*/
/* Quickcheck generators */

void generate(size_t _nb, parray<point2d>& dst) {
  intT nb = (intT)_nb;
  if (quickcheck::generateInRange(0, 1) == 0) {
    dst = plummer2d(nb);
  } else {
    dst = uniform2d(true, false, nb);
  }
}

void generate(size_t nb, container_wrapper<parray<point2d>>& c) {
  generate(nb, c.c);
}


/*---------------------------------------------------------------------*/
/* Quickcheck properties */

bool checkDelaunay(tri *triangs, intT n, intT boundarySize);

#define MIN_ANGLE 30.0

double angle(tri *t) {
  return std::min(angle(t->vtx[0]->pt, t->vtx[1]->pt, t->vtx[2]->pt),
                  std::min(angle(t->vtx[1]->pt, t->vtx[0]->pt, t->vtx[2]->pt),
                           angle(t->vtx[2]->pt, t->vtx[0]->pt, t->vtx[1]->pt)));
}

bool rcheck(triangles<point2d> Tri) {
  intT m = Tri.numTriangles;
  parray<vertex> V;
  parray<tri> Triangs;
  topologyFromTriangles(Tri, V, Triangs);
  if (checkDelaunay(Triangs.begin(), m, 10)) return 1;
  parray<intT> bad(m, [&] (intT i) {
    return skinnyTriangle(&Triangs[i]);
  });
  intT nbad = sum(bad.cbegin(), bad.cend());
  if (nbad > 0) {
    cout << "Delaunay refine check: " << nbad << " skinny triangles" << endl;
    return 1;
  }
  return 0;
}
  
using parray_wrapper = container_wrapper<parray<point2d>>;

class prop : public quickcheck::Property<parray_wrapper> {
public:
  
  bool holdsFor(const parray_wrapper& _in) {
    parray_wrapper in(_in);
    triangles<point2d> tri = delaunay(in.c.begin(), in.c.size());
    triangles<point2d> R = refine(tri);
    bool b = rcheck(R);
    tri.del();
    R.del();
    return ! b;
  }
  
};

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (bool sequential) {
    int nb_tests = pasl::util::cmdline::parse_or_default_int("n", 1000);
    checkit<pasl::pctl::prop>(nb_tests, "delaunay refine is correct");
  });
  return 0;
}

/***********************************************************************/
