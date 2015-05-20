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
#include "hull.hpp"
#include "seqhull.hpp"

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
  
struct getX {
  point2d* P;
  getX(point2d* _P) : P(_P) {}
  double operator() (intT i) {return P[i].x;}
};

struct lessX {bool operator() (point2d a, point2d b) {
  return (a.x < b.x) ? 1 : (a.x > b.x) ? 0 : (a.y < b.y);} };

bool eq(point2d a, point2d b) {
  return (a.x == b.x) && (a.y == b.y);
}


template <class ET, class intT, class F, class G>
intT maxIndexSerial(intT s, intT e, F f, G g) {
  ET r = g(s);
  intT k = 0;
  for (intT j=s+1; j < e; j++) {
    ET v = g(j);
    if (f(v,r)) { r = v; k = j;}
  }
  return k;
}

bool checkHull(parray<point2d>& PIn,  parray<intT>& I) {
  point2d* P = PIn.begin();
  intT n = PIn.size();
  intT nOut = I.size();
  point2d* PO = newA(point2d, nOut);
  for (intT i=0; i < nOut; i++) PO[i] = P[I[i]];
  intT idx = maxIndexSerial<double>((intT)0, nOut,  greater<double>(), getX(PO));
  std::sort(P, P + n, lessX());
  if (!eq(P[0], PO[0])) {
    cout << "checkHull: bad leftmost point" << endl;
    P[0].print();  PO[0].print(); cout << endl;
    return 1;
  }
  if (!eq(P[n-1], PO[idx])) {
    cout << "checkHull: bad rightmost point" << endl;
    return 1;
  }
  intT k = 1;
  for (intT i = 0; i < idx; i++) {
    if (i > 0 && counterClockwise(PO[i-1],PO[i],P[i+1])) {
      cout << "checkHull: not convex" << endl;
      return 1;
    }
    if (PO[i].x > PO[i+1].x) {
      cout << "checkHull: not sorted by x" << endl;
      return 1;
    }
    while (!eq(P[k], PO[i+1]) && k < n)
      if (counterClockwise(PO[i],PO[i+1],P[k++])) {
        cout << "checkHull: above hull" << endl;
        return 1;
      }
    if (k == n) {
      cout << "checkHull: unexpected points in hull" << endl;
      return 1;
    }
    k++;
  }
  free(PO);
  return 0;
}


using parray_wrapper = container_wrapper<parray<point2d>>;

class consistent_hulls_property : public quickcheck::Property<parray_wrapper> {
public:
  
  bool holdsFor(const parray_wrapper& _in) {
    parray_wrapper in(_in);
    parray<intT> idxs = hull(in.c);
    return ! checkHull(in.c, idxs);
  }
  
};

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (bool sequential) {
    int nb_tests = pasl::util::cmdline::parse_or_default_int("n", 1000);
    checkit<pasl::pctl::consistent_hulls_property>(nb_tests, "quickhull is correct");
  });
  return 0;
}

/***********************************************************************/
