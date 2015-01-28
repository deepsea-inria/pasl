



#include "utils.hpp"
#include "datagen.hpp"

#ifndef _PBBS_RMAT_H_
#define _PBBS_RMAT_H_

namespace pbbs {

  template <class Edgelist>
  class rMat {
  public:
    using intT = typename Edgelist::vtxid_type;
    using edge_type = typename Edgelist::edge_type;
    using uintT = unsigned int;
    double a, ab, abc;
    intT n;
    intT h;
    rMat(intT _n, intT _seed,
         double _a, double _b, double _c) {
      n = _n; a = _a; ab = _a + _b; abc = _a+_b+_c;
      h = dataGen::hash<uintT>(_seed);
      utils::myAssert(abc <= 1.0,
                      "in rMat: a + b + c add to more than 1");
      utils::myAssert((1 << utils::log2Up(n)) == n,
                      "in rMat: n not a power of 2");
    }
    
    edge_type rMatRec(intT nn, intT randStart, intT randStride) {
      if (nn==1) return edge_type(0,0);
      else {
        edge_type x = rMatRec(nn/2, randStart + randStride, randStride);
        double r = dataGen::hash<double>(randStart);
        if (r < a) return x;
        else if (r < ab) return edge_type(x.src,x.dst+nn/2);
        else if (r < abc) return edge_type(x.src+nn/2, x.dst);
        else return edge_type(x.src+nn/2, x.dst+nn/2);
      }
    }
    
    edge_type operator() (intT i) {
      intT randStart = dataGen::hash<uintT>((2*i)*h);
      intT randStride = dataGen::hash<uintT>((2*i+1)*h);
      return rMatRec(n, randStart, randStride);
    }
  };
  
} // end namespace

#endif /*! _PBBS_RMAT_H_ */