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
#include "delaunay.hpp"

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
  
// Note that this is not currently a complete test of correctness
// For example it would allow a set of disconnected triangles, or even no
// triangles
bool checkDelaunay(tri *triangs, intT n, intT boundarySize) {
  parray<intT> bcount(n, 0);
  intT insideOutError = n;
  intT inCircleError = n;
  parallel_for((intT)0, n, [&] (intT i) {
    if (triangs[i].initialized) {
      simplex t = simplex(&triangs[i],0);
      for (int j=0; j < 3; j++) {
        simplex a = t.across();
        if (a.valid()) {
          vertex* v = a.rotClockwise().firstVertex();
          
          // Check that the neighbor is outside the triangle
          if (!t.outside(v)) {
            double vz = triAreaNormalized(t.t->vtx[(t.o+2)%3]->pt,
                                          v->pt, t.t->vtx[t.o]->pt);
            //cout << "i=" << i << " vz=" << vz << endl;
            // allow for small error
            if (vz < -1e-10)  utils::writeMin(&insideOutError,i);
          }
          
          // Check that the neighbor is not in circumcircle of the triangle
          if (t.inCirc(v)) {
            double vz = inCircleNormalized(t.t->vtx[0]->pt, t.t->vtx[1]->pt,
                                           t.t->vtx[2]->pt, v->pt);
            //cout << "i=" << i << " vz=" << vz << endl;
            // allow for small error
            if (vz > 1e-10) utils::writeMin(&inCircleError,i);
          }
        } else bcount[i]++;
        t = t.rotClockwise();
      }
    }
  });
  //intT cnt = sum(bcount.cbegin(), bcount.cend());
  //if (boundarySize != cnt) {
  //cout << "delaunayCheck: wrong boundary size, should be " << boundarySize
  //<< " is " << cnt << endl;
  //return 1;
  //}
  
  if (insideOutError < n) {
    cout << "delaunayCheck: neighbor inside triangle at triangle " 
    << inCircleError << endl;
    return 1;
  }
  if (inCircleError < n) {
    cout << "In Circle Violation at triangle " << inCircleError << endl;
    return 1;
  }
  
  return 0;
}

bool dcheck(triangles<point2d> Tri, parray<point2d>& P) {
  intT m = Tri.numTriangles;
  for (intT i=0; i < P.size(); i++)
    if (P[i].x != Tri.P[i].x || P[i].y != Tri.P[i].y) {
      cout << "checkDelaunay: prefix of points don't match input at "
      << i << endl;
      cout << P[i] << " " << Tri.P[i] << endl;
      return 0;
    }
  vertex* V = NULL;
  tri* Triangs = NULL;
  topologyFromTriangles(Tri, &V, &Triangs);
  return checkDelaunay(Triangs, m, 10);
}

using parray_wrapper = container_wrapper<parray<point2d>>;

class prop : public quickcheck::Property<parray_wrapper> {
public:
  
  bool holdsFor(const parray_wrapper& _in) {
    parray_wrapper in(_in);
    triangles<point2d> tri = delaunay(in.c.begin(), in.c.size());
    return ! dcheck(tri, in.c);
  }
  
};

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (bool sequential) {
    int nb_tests = pasl::util::cmdline::parse_or_default_int("n", 1000);
    checkit<pasl::pctl::prop>(nb_tests, "delaunay triangulation is correct");
  });
  return 0;
}

/***********************************************************************/
