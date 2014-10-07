
#include <iostream>
#include "datagen.hpp"
#include "geometryData.hpp"
#include "sequence.hpp"
#include "benchmark.hpp"

namespace pbbs {
  
#include "defaults.hpp"
  
  using namespace sequence;
  
template <class ET, class F>
pair<intT,intT> split(ET* A, intT n, F lf, F rf) {
  intT ll = 0, lm = 0;
  intT rm = n-1, rr = n-1;
  while (1) {
    while ((lm <= rm) && !(rf(A[lm]) > 0)) {
      if (lf(A[lm]) > 0) A[ll++] = A[lm];
      lm++;
    }
    while ((rm >= lm) && !(lf(A[rm]) > 0)) {
      if (rf(A[rm]) > 0) A[rr--] = A[rm];
      rm--;
    }
    if (lm >= rm) break;
    ET tmp = A[lm++];
    A[ll++] = A[rm--];
    A[rr--] = tmp;
  }
  intT n1 = ll;
  intT n2 = n-rr-1;
  return pair<intT,intT>(n1,n2);
}

struct aboveLine {
  intT l, r;
  point2d* P;
  aboveLine(point2d* _P, intT _l, intT _r) : P(_P), l(_l), r(_r) {}
  bool operator() (intT i) {return triArea(P[l], P[r], P[i]) > 0.0;}
};

intT serialQuickHull(intT* I, point2d* P, intT n, intT l, intT r) {
  if (n < 2) return n;
  intT maxP = I[0];
  double maxArea = triArea(P[l],P[r],P[maxP]);
  for (intT i=1; i < n; i++) {
    intT j = I[i];
    double a = triArea(P[l],P[r],P[j]);
    if (a > maxArea) {
      maxArea = a;
      maxP = j;
    }
  }
  
  pair<intT,intT> nn = split(I, n, aboveLine(P,l,maxP), aboveLine(P,maxP,r));
  intT n1 = nn.first;
  intT n2 = nn.second;
  
  intT m1, m2;
  m1 = serialQuickHull(I,      P, n1, l,   maxP);
  m2 = serialQuickHull(I+n-n2, P, n2, maxP,r);
  for (intT i=0; i < m2; i++) I[i+m1+1] = I[i+n-n2];
  I[m1] = maxP;
  return m1+1+m2;
}

struct triangArea {
  intT l, r;
  point2d* P;
  intT* I;
  triangArea(intT* _I, point2d* _P, intT _l, intT _r) : I(_I), P(_P), l(_l), r(_r) {}
  double operator() (intT i) {return triArea(P[l], P[r], P[I[i]]);}
};

intT quickHull(intT* I, intT* Itmp, point2d* P, intT n, intT l, intT r, intT depth) {
  if (n < 2) // || depth == 0)
    return serialQuickHull(I, P, n, l, r);
  else {
    
    intT idx = maxIndex<double>((intT)0,n,greater<double>(),triangArea(I,P,l,r));
    intT maxP = I[idx];
    
    intT n1 = filter(I, Itmp,    n, aboveLine(P, l, maxP));
    intT n2 = filter(I, Itmp+n1, n, aboveLine(P, maxP, r));
    
    intT m1, m2;
    native::fork2([&] {
      m1 = quickHull(Itmp, I ,P, n1, l, maxP, depth-1);
    }, [&] {
      m2 = quickHull(Itmp+n1, I+n1, P, n2, maxP, r, depth-1);
    });
    
    native::parallel_for(intT(0), m1, [&] (intT i) {
      I[i] = Itmp[i];
    });
    I[m1] = maxP;
    native::parallel_for(intT(0), m2, [&] (intT i) {
      I[i+m1+1] = Itmp[i+n1];
    });
    return m1+1+m2;
  }
}

struct makePair {
  pair<intT,intT> operator () (intT i) { return pair<intT,intT>(i,i);}
};

struct minMaxIndex {
  point2d* P;
  minMaxIndex (point2d* _P) : P(_P) {}
  pair<intT,intT> operator () (pair<intT,intT> l, pair<intT,intT> r) {
    intT minIndex =
    (P[l.first].x < P[r.first].x) ? l.first :
    (P[l.first].x > P[r.first].x) ? r.first :
    (P[l.first].y < P[r.first].y) ? l.first : r.first;
    intT maxIndex = (P[l.second].x > P[r.second].x) ? l.second : r.second;
    return pair<intT,intT>(minIndex, maxIndex);
  }
};

_seq<intT> hull(point2d* P, intT n) {
  pair<intT,intT> minMax = reduce<pair<intT,intT> >((intT)0,n,minMaxIndex(P), makePair());
  intT l = minMax.first;
  intT r = minMax.second;
  bool* fTop = newA(bool,n);
  bool* fBot = newA(bool,n);
  intT* I = newA(intT, n);
  intT* Itmp = newA(intT, n);
  native::parallel_for(intT(0), n, [&] (intT i) {
    Itmp[i] = i;
    double a = triArea(P[l],P[r],P[i]);
    fTop[i] = a > 0;
    fBot[i] = a < 0;
  });
  
  intT n1 = pack(Itmp, I, fTop, n);
  intT n2 = pack(Itmp, I+n1, fBot, n);
  free(fTop); free(fBot);
  
  intT m1; intT m2;
  native::fork2([&] {
    m1 =  quickHull(I, Itmp, P, n1, l, r, 5);
  }, [&] {
    m2 = quickHull(I+n1, Itmp+n1, P, n2, r, l, 5);
  });
  
  native::parallel_for(intT(0), m1, [&] (intT i) {
    Itmp[i+1] = I[i];
  });
  native::parallel_for(intT(0), m2, [&] (intT i) {
    Itmp[i+m1+2] = I[i+n1];
  });

  free(I);
  Itmp[0] = l;
  Itmp[m1+1] = r;
  return _seq<intT>(Itmp, m1+2+m2);
}

template <class intT>
void doit(int argc, char** argv) {
  intT result = 0;
  intT n = 0;
  pbbs::point2d* Points;
  auto init = [&] {
    n = (intT)pasl::util::cmdline::parse_or_default_int64("n", 100000);
    using thunk_type = std::function<void ()>;
    pasl::util::cmdline::argmap<thunk_type> t;
    t.add("from_file", [&] {
      pasl::util::atomic::die("todo");
    });
    t.add("by_generator", [&] {
      pasl::util::cmdline::argmap<thunk_type> m;
      m.add("plummer", [&] {
        Points = pbbs::plummer2d(n);
      });
      m.add("uniform", [&] {
        bool inSphere = pasl::util::cmdline::parse_or_default_bool("in_sphere", false);
        bool onSphere = pasl::util::cmdline::parse_or_default_bool("on_sphere", false);
        Points = pbbs::uniform2d(inSphere, onSphere, n);
      });
      m.find_by_arg_or_default_key("generator", "plummer")();
    });
    t.find_by_arg_or_default_key("load", "by_generator")();
  };
  auto run = [&] (bool sequential) {
    hull(Points, n);
  };
  auto output = [&] {
  };
  auto destroy = [&] {
    free(Points);
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}

}

int main(int argc, char** argv) {
  pbbs::doit<int>(argc, argv);
  return 0;
}

