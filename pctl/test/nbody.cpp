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
#include "cknbody.hpp"
#include "geometrydata.hpp"

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

using value_type = point3d;

void generate(size_t nb, parray<value_type>& dst) {
  dst = plummer3d<int, unsigned int>(nb);
}

void generate(size_t nb, container_wrapper<parray<value_type>>& c) {
  generate(nb, c.c);
}


/*---------------------------------------------------------------------*/
/* Quickcheck properties */
  
double error_bound;
  
double check1(point3d* p, vect3d* forces, intT n) {
  int nCheck = min<intT>(n,200);
  double* Err = newA(double,nCheck);
  double mass = 1.0; // all masses are 1 for now
  
  parallel_for((int)0, nCheck, [&] (int i) {
    intT idx = prandgen::hashi(i)%n;
    vect3d force(0.,0.,0.);
    for (intT j=0; j < n; j++) {
      if (idx != j) {
        vect3d v = p[j] - p[idx];
        double r2 = v.dot(v);
        force = force + (v * (mass * mass / (r2*sqrt(r2))));
      }
    }
    Err[i] = (force - forces[idx]).Length()/force.Length();
  });
  double total = 0.0;
  for(int i=0; i < nCheck; i++)
    total += Err[i];
  return total/nCheck;
}

using parray_wrapper = container_wrapper<parray<value_type>>;

class prop : public quickcheck::Property<parray_wrapper> {
public:
  
  bool holdsFor(const parray_wrapper& _in) {
    parray_wrapper in(_in);
    parray<point3d>& pts = in.c;
    long n = pts.size();
    parray<particle*> p(n);
    parray<particle> pp(n);
    parallel_for(0L, n, [&] (intT i) {
      p[i] = new (&pp[i]) particle(pts[i],1.0);
    });
    nbody(p.begin(), n);
    parray<vect3d> O(n, [&] (long i) {
      return vect3d(0.,0.,0.) + p[i]->force;
    });
    double err = check1(pts.begin(), O.begin(), n);
    return err < error_bound;
  }
  
};

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (bool sequential) {
    pasl::pctl::error_bound = pasl::util::cmdline::parse_or_default_double("error_bound", 1e-6);
    int nb_tests = pasl::util::cmdline::parse_or_default_int("n", 1000);
    checkit<pasl::pctl::prop>(nb_tests, "nbody is correct");
  });
  return 0;
}

/***********************************************************************/
