#include "geometry.hpp"
#include "dpsdatapar.hpp"

namespace pasl {
  namespace pctl {
    using intT = int;
    typedef _point3d<double> pointT;
    
    intT* rayCast(triangles<pointT>, ray<pointT>*, intT);
  }
}


