/*!
 * \file scan.cpp
 * \brief Quickcheck for scan
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include <limits>

#include "float.h"
#include "pasl.hpp"
#include "prandgen.hpp"
#include "quickcheck.hpp"
#include "io.hpp"
#include "geometrydata.hpp"
#include "nearestneighbors.hpp"

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
  
using intT = int;

void generate(size_t _nb, parray<point2d>& dst) {
  intT nb = (intT)_nb;
  if (quickcheck::generateInRange(0, 1) == 0) {
    dst = plummer2d(nb);
  } else {
    bool inSphere = quickcheck::generateInRange(0, 1) == 0;
    bool onSphere = quickcheck::generateInRange(0, 1) == 0;
    dst = uniform2d(inSphere, onSphere, nb);
  }
}

void generate(size_t nb, container_wrapper<parray<point2d>>& c) {
  generate(nb, c.c);
}


/*---------------------------------------------------------------------*/
/* Quickcheck properties */

template <class pointT>
int checkNeighbors(parray<intT>& neighbors, pointT* P, intT n, intT k, intT r) {
  if (neighbors.size() != k * n) {
    cout << "error in neighborsCheck: wrong length, n = " << n
    << " k = " << k << " neighbors = " << neighbors.size() << endl;
    return 1;
  }
  
  for (intT j = 0; j < r; j++) {
    intT jj = prandgen::hashi(j) % n;
    
    double* distances = newA(double,n);
    parallel_for (intT(0), n, [&] (intT i) {
      if (i == jj) distances[i] = DBL_MAX;
      else distances[i] = (P[jj] - P[i]).Length();
    });
    double id = (n == 0) ? 0.0 : distances[0];
    double minD = reduce(distances, distances+n, id, [&] (double x, double y) {
      return std::min(x, y);
    });
    
    double d = (P[jj] - P[(neighbors.begin())[k*jj]]).Length();
    
    double errorTolerance = 1e-6;
    if ((d - minD) / (d + minD)  > errorTolerance) {
      cout << "error in neighborsCheck: for point " << jj
      << " min distance reported is: " << d
      << " actual is: " << minD << endl;
      return 1;
    }
  }
  return 0;
}
  
template <class PT, int KK>
struct vertex {
  typedef PT pointT;
  int identifier;
  pointT pt;         // the point itself
  vertex* ngh[KK];    // the list of neighbors
  vertex(pointT p, int id) : pt(p), identifier(id) {}
  vertex() { }
  
};
  
using parray_wrapper = container_wrapper<parray<point2d>>;

template <class point, int maxK>
class nearestneighbors_property : public quickcheck::Property<parray_wrapper> {
public:
  
  bool holdsFor(const parray_wrapper& _in) {
    using vertex = vertex<point,maxK>;
    parray_wrapper in(_in);
    intT n = (intT)in.c.size();
    parray<point>& points = in.c;
    parray<vertex*> v(n);
    parray<vertex> vv(n);
    parallel_for(intT(0), n, [&] (intT i) {
      v[i] = new (&vv[i]) vertex(points[i],i);
    });
    int k = 1;
    ANN<intT,maxK>(v.begin(), n, k);
    int m = n * k;
    parray<int> Pout(m);
    parallel_for(intT(0), n, [&] (intT i) {
      for (int j=0; j < k; j++)
        Pout[maxK*i + j] = (v[i]->ngh[j])->identifier;
    });
    int r = 10;
    return ! checkNeighbors(Pout, points.begin(), n, k, r);
  }
  
};

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (bool sequential) {
    int nb_tests = pasl::util::cmdline::parse_or_default_int("n", 1000);
    checkit<pasl::pctl::nearestneighbors_property<point2d,1>>(nb_tests, "nearestneighbors is correct");
  });
  return 0;
}

/***********************************************************************/
