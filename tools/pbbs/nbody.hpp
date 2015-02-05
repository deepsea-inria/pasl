#include "geometry.hpp"

#ifndef _PBBS_NBODY_H_
#define _PBBS_NBODY_H_

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

#endif