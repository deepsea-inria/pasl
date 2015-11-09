#include <iostream>
#include <vector>
#include "sequence.hpp"
#include "nbody.hpp"
#include "utils.hpp"
#include "octTree.hpp"
#include "geometry.hpp"
#include "spherical.hpp"
#include "benchmark.hpp"
#include "geometryData.hpp"
#include "defaults.hpp"

namespace pbbs {
using namespace std;
namespace native = pasl::sched::native;

  #define CHECK 1

// Following for 1e-1 accuracy (1.05 seconds for 1million 8 cores)
//#define ALPHA 1.9
//#define terms 3
//#define BOXSIZE 25

// Following for 1e-3 accuracy (4 seconds for 1million 8 cores)
//#define ALPHA 2.2
//#define terms 7
//#define BOXSIZE 60

// Following for 1e-6 accuracy (12.5 seconds for 1million insphere 8 cores)
#define ALPHA 2.65
#define terms 12
#define BOXSIZE 130

// Following for 1e-9 accuracy (40 seconds for 1million 8 cores)
//#define ALPHA 3.0
//#define terms 17
//#define BOXSIZE 250

double check(particle** p, intT n) {
  int nCheck = min<intT>(n,200);
  double* Err = newA(double,nCheck);
  
  native::parallel_for(0, nCheck, [&] (int i) {
    intT idx = utils::hash(i)%n;
    vect3d force(0.,0.,0.);
    for (intT j=0; j < n; j++) {
      if (idx != j) {
	vect3d v = (p[j]->pt) - (p[idx]->pt);
	double r2 = v.dot(v);
	force = force + (v * (p[j]->mass * p[idx]->mass / (r2*sqrt(r2))));
      }
    }
    Err[i] = (force - p[idx]->force).Length()/force.Length();
  });
  double total = 0.0;
  for(int i=0; i < nCheck; i++) 
    total += Err[i];
  free(Err);
  return total/nCheck;
}

// *************************************************************
//    FORCE CALCULATIONS
// *************************************************************

struct innerExpansion {
  Transform<terms>* TR;
  complex<double> coefficients[terms*terms];
  point3d center;
  void addTo(point3d pt, double mass) {
    TR->P2Madd(coefficients, mass, center, pt);
  }
  void addTo(innerExpansion* y) {
    TR->M2Madd(coefficients, center, y->coefficients, y->center);
  }
  innerExpansion(Transform<terms>* _TR, point3d _center) : TR(_TR), center(_center) {
    for (intT i=0; i < terms*terms; i++) coefficients[i] = 0.0;
  }
  vect3d force(point3d y, double mass) {
    vect3d result;
    double potential;
    TR->M2P(potential, result, y, coefficients, center);
    result = result*mass;
    return result;
  }
  innerExpansion() {}
};

struct outerExpansion {
  Transform<terms>* TR;
  complex<double> coefficients[terms*terms];
  point3d center;
  void addTo(innerExpansion* y) {
    TR->M2Ladd(coefficients, center, y->coefficients, y->center);}
  void addTo(outerExpansion* y) {
    TR->L2Ladd(coefficients, center, y->coefficients, y->center);
  }
  vect3d force(point3d y, double mass) {
    vect3d result;
    double potential;
    TR->L2P(potential, result, y, coefficients, center);
    result = result*mass;
    return result;
  }
  outerExpansion(Transform<terms>* _TR, point3d _center) : TR(_TR), center(_center) {
    for (intT i=0; i < terms*terms; i++) coefficients[i] = 0.0;
  }
  outerExpansion() {}
};

Transform<terms>* TRglobal = new Transform<terms>();

struct node {
  typedef pair<node*,intT> edge;
  node* left;
  node* right;
  particle** particles;
  intT n;
  point3d bot;
  point3d top;
  innerExpansion* Inx;
  outerExpansion* Outx;
  vector<node*> indirectNeighbors;
  vector<edge> leftNeighbors;
  vector<edge> rightNeighbors;
  vector<vect3d*> hold;
  bool leaf() {return left == NULL;}
  node() {}
  point3d center() { return bot + (top-bot)/2.0;}
  double radius() { return (top - bot).Length()/2.0;}
  double lmax() {
    vect3d d = top-bot;
    return max(d.x,max(d.y,d.z));
  }
  node(node* L, node* R, intT _n, point3d _minPt, point3d _maxPt) 
    : left(L), right(R), particles(NULL), n(_n), bot(_minPt), top(_maxPt) {}
  node(particle** P, intT _n, point3d _minPt, point3d _maxPt) 
    : left(NULL), right(NULL), particles(P), n(_n), bot(_minPt), top(_maxPt) {}
};

typedef pair<node*,intT> edge;
typedef pair<point3d,point3d> ppair;

struct ppairF {
  ppair operator() (particle* a) { return ppair(a->pt,a->pt);}
};

struct minmaxpt {
  ppair operator() (ppair a, ppair b) {
    return ppair((a.first).minCoords(b.first),
		 (a.second).maxCoords(b.second));}
};

node* buildTree(particle** particles, particle** Tmp, bool* Tflags, intT n, intT depth) {
  if (depth > 100) abort();

  ppair R = sequence::mapReduce<ppair>(particles, n, minmaxpt(), ppairF());
  point3d minPt = R.first;
  point3d maxPt = R.second;
  if (n < BOXSIZE) return new node(particles, n, minPt, maxPt);

  intT d = 0;
  double mind = 0.0;
  for (int i=0; i < 3; i++) {
    if (maxPt[i] - minPt[i] > mind) {
      d = i;
      mind = maxPt[i] - minPt[i];
    }
  }
  double splitpoint = (maxPt[d] + minPt[d])/2.0;

  native::parallel_for((intT)0, n, [&] (intT i) {
    Tflags[i] = particles[i]->pt[d] < splitpoint;
  });
  intT l = sequence::pack(particles,Tmp,Tflags,n);

  native::parallel_for((intT)0, n, [&] (intT i) {
      Tflags[i] = !Tflags[i];
    });
  intT r = sequence::pack(particles,Tmp+l,Tflags,n);
  native::parallel_for((intT)0, n, [&] (intT i) {
      particles[i] = Tmp[i];
    });

  node* a;
  node* b;
  native::fork2([&] {
      a = buildTree(particles,Tmp,Tflags,l,depth+1);
  }, [&] {
      b = buildTree(particles+l,Tmp+l,Tflags+l,n-l,depth+1);
  });

  return new node(a, b, n, minPt, maxPt);
}

bool far(node* a, node* b) {
  vect3d sep;
  for (int dim =0; dim < 3; dim++) {
    if (a->bot[dim] > b->top[dim]) sep[dim] = a->bot[dim] - b->top[dim];
    else if (a->top[dim] < b->bot[dim]) sep[dim] = b->bot[dim] - a->top[dim];
    else sep[dim] = 0.0;
  }
  double sepDistance = sep.Length();
  double rmax = max(a->radius(), b->radius());
  double r = (a->center() - b->center()).Length();
  return r >= (ALPHA * rmax);
  //return sepDistance/rmax > 1.2;
}

// used to count the number of interactions
struct ipair {
  long direct;
  long indirect;
  ipair() {}
  ipair(long a, long b) : direct(a), indirect(b) {}
  ipair operator+ (ipair b) {
    return ipair(direct + b.direct, indirect + b.indirect);}
};


ipair interactions(node* Left, node* Right) {
  if (far(Left,Right)) {
    Left->indirectNeighbors.push_back(Right); 
    Right->indirectNeighbors.push_back(Left); 
    return ipair(0,2);
  } else {
    if (!Left->leaf() && (Left->lmax() >= Right->lmax() || Right->leaf())) {
      ipair x = interactions(Left->left, Right);
      ipair y = interactions(Left->right, Right);
      return x + y;
    } else if (!Right->leaf()) {
      ipair x = interactions(Left, Right->left);
      ipair y = interactions(Left, Right->right);
      return x + y;
    } else {
      if (Right->n > Left->n) swap(Right,Left);
      intT rn = Right->leftNeighbors.size();
      intT ln = Left->rightNeighbors.size();
      Right->leftNeighbors.push_back(edge(Left,ln)); 
      Left->rightNeighbors.push_back(edge(Right,rn));
      return ipair(Right->n*Left->n,0);
    }
  }
}

ipair interactions(node* tr) {
  if (!tr->leaf()) {
    ipair x, y, z;
    native::fork2([&] {
        x = interactions(tr->left);
      }, [&] {
        y = interactions(tr->right);        
      });
    z = interactions(tr->left,tr->right);
    return x + y + z;
  } else return ipair(0,0);
}

intT numLeaves(node* tr) {
  if (tr->leaf()) return 1;
  else return(numLeaves(tr->left)+numLeaves(tr->right));
}

intT allocateExpansionsR(node* tr, innerExpansion* I, outerExpansion* O) {
  if (tr->leaf()) {
    tr->Inx = new (I) innerExpansion(TRglobal, tr->center());
    tr->Outx = new (O) outerExpansion(TRglobal, tr->center());
    return 1;
  } else {
    intT l = allocateExpansionsR(tr->left,I,O);
    tr->Inx = new (I+l) innerExpansion(TRglobal, tr->center());
    tr->Outx = new (O+l) outerExpansion(TRglobal, tr->center());
    intT r = allocateExpansionsR(tr->right,I+l+1,O+l+1);
    return l+r+1;
  }
}

void allocateExpansions(node* tr) {
  intT n = numLeaves(tr);
  innerExpansion* I = newA(innerExpansion,2*n-1);
  outerExpansion* O = newA(outerExpansion,2*n-1);
  intT m = allocateExpansionsR(tr,I,O);
}

void doIndirect(node* tr) {
  //tr->Outx = new outerExpansion(TRglobal, tr->center());
  for (intT i = 0; i < tr->indirectNeighbors.size(); i++) 
    tr->Outx->addTo(tr->indirectNeighbors[i]->Inx);
  if (!tr->leaf()) {
        native::fork2([&] {
        doIndirect(tr->left);        
              }, [&] {
        doIndirect(tr->right);
              });
  }
}

void upSweep(node* tr) {
  //tr->Inx = new innerExpansion(TRglobal, tr->center());
  if (tr->leaf()) {
    for (intT i=0; i < tr->n; i++) {
      particle* P = tr->particles[i];
      tr->Inx->addTo(P->pt, P->mass);
    }
  } else {
    native::fork2([&] {
        upSweep(tr->left);
      }, [&] {
        upSweep(tr->right);
      });
    tr->Inx->addTo(tr->left->Inx);
    tr->Inx->addTo(tr->right->Inx);
  }
}

void downSweep(node* tr) {
  if (tr->leaf()) {
    for (intT i=0; i < tr->n; i++) {
      particle* P = tr->particles[i];
      P->force = P->force + tr->Outx->force(P->pt, P->mass);
    }
  } else {
    native::fork2([&] {
        tr->left->Outx->addTo(tr->Outx);
        downSweep(tr->left);
      }, [&] {
        tr->right->Outx->addTo(tr->Outx);
        downSweep(tr->right);
      });
  }
}

intT getLeaves(node* tr, node** Leaves) {
  if (tr->leaf()) {
    Leaves[0] = tr;
    return 1;
  } else {
    intT l = getLeaves(tr->left, Leaves);
    intT r = getLeaves(tr->right, Leaves + l);
    return l + r;
  }
}

void direct(node* Left, node* ngh, vect3d* hold) {
  particle** LP = Left->particles;
  particle** RP = ngh->particles;
  intT nl = Left->n;
  intT nr = ngh->n;
  //vect3d* rfrc = E.second;
  for (intT j=0; j < nr; j++) 
    hold[j] = vect3d(0.,0.,0.);
  for (intT i=0; i < nl; i++) {
    vect3d frc(0.,0.,0.);
    particle* pa = LP[i];
    for (intT j=0; j < nr; j++) {
      particle* pb = RP[j];
      vect3d v = (pb->pt) - (pa->pt);
      double r2 = v.dot(v);
      vect3d force;
      if (terms > 15) {
	force = (v * (pa->mass * pb->mass / (r2*sqrt(r2))));;
      } else { // use single precision sqrt for lower accuracy
	float rf2 = r2;
	force = (v * (pa->mass * pb->mass / (r2*sqrt(rf2))));
      }
      frc = frc + force;
      hold[j] = hold[j] - force;
    }
    pa->force = pa->force + frc;
  }
}

void self(node* Tr) {
  particle** PP = Tr->particles;
  for (intT i=0; i < Tr->n; i++) {
    particle* pa = PP[i];
    for (intT j=i+1; j < Tr->n; j++) {
	particle* pb = PP[j];
	vect3d v = (pb->pt) - (pa->pt);
	double r2 = v.dot(v);
	vect3d force = (v * (pa->mass * pb->mass / (r2*sqrt(r2))));
	pb->force = pb->force - force;
	pa->force = pa->force + force;
      }
  }
}

void doDirect(node* a) {
  intT nleaves = numLeaves(a);
  node** Leaves = newA(node*,nleaves);
  getLeaves(a,Leaves);

  intT* counts = newA(intT,nleaves);

  // the following allocates space avoiding malloc
  native::parallel_for((intT)0, nleaves, [&] (intT i) {
    counts[i] = 0;
    for (intT j =0; j < Leaves[i]->rightNeighbors.size(); j++)
      counts[i] += Leaves[i]->rightNeighbors[j].first->n;
  });
  intT total = sequence::plusScan(counts,counts,nleaves);
  vect3d* hold = newA(vect3d,total);

  // calculates interactions and neighbors results in hold
  native::parallel_for1((intT)0, nleaves, [&] (intT i) {
    vect3d* lhold = hold + counts[i];
    for (intT j =0; j < Leaves[i]->rightNeighbors.size(); j++) {
      Leaves[i]->hold.push_back(lhold);
      node* ngh = Leaves[i]->rightNeighbors[j].first;
      direct(Leaves[i], ngh, lhold);
      lhold += ngh->n;
    }
  });
  free(counts);

  // picks up results from neighbors
  native::parallel_for1((intT)0, nleaves, [&] (intT i) {
    for (intT j =0; j < Leaves[i]->leftNeighbors.size(); j++) {
      node* L = Leaves[i];
      edge e = L->leftNeighbors[j];
      vect3d* hold = e.first->hold[e.second];
      for (intT k=0; k < Leaves[i]->n; k++) 
	L->particles[k]->force = L->particles[k]->force + hold[k];
    }
  });
  free(hold);

  native::parallel_for1((intT)0, nleaves, [&] (intT i) {
    self(Leaves[i]);
  });
  free(Leaves);
}

// *************************************************************
//   STEP
// *************************************************************

// takes one step and places forces in particles[i]->force
void stepBH(particle** particles, intT n, double alpha) {
  TRglobal->precompute();

  native::parallel_for((intT)0, n, [&] (intT i) {
    particles[i]->force = vect3d(0.,0.,0.);
  });

  particle** Tmp = newA(particle*,n);
  particle** Hold = newA(particle*,n);
  native::parallel_for((intT)0, n, [&] (intT i) {
    Hold[i] = particles[i];
  });
  bool* Tflags = newA(bool,n);

  // build the CK tree
  node* a = buildTree(particles, Tmp, Tflags, n, 0);
  //nextTime("build tree");
  allocateExpansions(a);

  // Sweep up the tree calculating multipole expansions for each node
  upSweep(a);
  //nextTime("up sweep");

  // Determine all far-field interactions using the CK method
  ipair z = interactions(a);
  //nextTime("interactions");

  // Translate multipole to local expansions along the far-field
  // interactions
  doIndirect(a);
  //nextTime("do Indirect");

  // Translate the local expansions down the tree to the leaves
  downSweep(a);
  //nextTime("down sweep");

  // Add in all the direct (near-field) interactions
  doDirect(a);
  //nextTime("do Direct");
  //  cout << "Direct = " << (long) z.direct << " Indirect = " << z.indirect
  //       << " Boxes = " << numLeaves(a) << endl;
  native::parallel_for((intT)0, n, [&] (intT i) {
      particles[i] = Hold[i];
  });
  if (CHECK) {
    cout << "  Sampled RMS Error = "<< check(particles,n) << endl;
    //nextTime("check");
  }
}

void mynbody(particle** particles, intT n) { 
  stepBH(particles, n, ALPHA); }

  
template <class intT, class uintT>
void doit(int argc, char** argv) {
  intT n = 0;
  pbbs::point3d* Points;
  pbbs::particle** p;
  pbbs::particle* pp;
  auto init = [&] {
    n = (intT)pasl::util::cmdline::parse_or_default_int64("n", 24);
    p  = newA(pbbs::particle*,n);
    pp = newA(pbbs::particle, n);
    using thunk_type = std::function<void ()>;
    pasl::util::cmdline::argmap<thunk_type> t;
    t.add("from_file", [&] {
      pasl::util::atomic::die("todo");
    });
    t.add("by_generator", [&] {
      pasl::util::cmdline::argmap<thunk_type> m;
      m.add("plummer", [&] {
        Points = pbbs::plummer3d<intT,uintT>(n);
      });
      m.add("uniform", [&] {
        bool inSphere = pasl::util::cmdline::parse_or_default_bool("in_sphere", false);
        bool onSphere = pasl::util::cmdline::parse_or_default_bool("on_sphere", false);
        Points = pbbs::uniform3d<intT,uintT>(inSphere, onSphere, n);
      });
      m.find_by_arg_or_default_key("generator", "plummer")();
    });
    t.find_by_arg_or_default_key("load", "by_generator")();
    pasl::sched::native::parallel_for(intT(0), n, [&] (intT i) {
      p[i] = new (&pp[i]) pbbs::particle(Points[i],1.0);
    });
  };
  auto run = [&] (bool sequential) {
    mynbody(p, n);
  };
  auto output = [&] {
    pbbs::point3d* O = newA(pbbs::point3d,n);
    pasl::sched::native::parallel_for(intT(0), n, [&] (intT i) {
      O[i] = pbbs::point3d(0.,0.,0.) + p[i]->force;
    });
    // later optionally write points to a file
    free(O);
  };
  auto destroy = [&] {
    ;
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
  free(p); free(pp); free(Points);
}
  
} // end namespace


int main(int argc, char** argv) {
  pbbs::doit<int, unsigned>(argc, argv);
  return 0;
}

