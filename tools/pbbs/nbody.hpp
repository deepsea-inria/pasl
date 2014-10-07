#include "geometry.hpp"

namespace pbbs {
  
class particle {
public:
  point3d pt;
  double mass;
  vect3d force;
  particle(point3d p, double m) : pt(p),mass(m) {}
};

template <class intT>
void nbody(particle** particles, intT n);

}