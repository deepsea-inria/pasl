#include "geometry.hpp"

class particle {
public:
  point3d pt;
  double mass;
  vect3d force;
  particle() { }
  particle(point3d p, double m) : pt(p),mass(m) {}
};

//void nbody(particle** particles, intT n);
