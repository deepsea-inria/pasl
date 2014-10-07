#include <iostream>
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

// good for .1 accuracy
#define ALPHA 1.0

// good for 10e-2 accuracy
//#define ALPHA 2.4

//typedef Transform<5> Trans;

// gravitational constant
#define gGrav 1.0

double check(particle** p, int n) {
  int nCheck = 10;
  double Err = 0.0;
  for (int i=0; i < nCheck; i++) {
    int idx = utils::hash(i)%n;
    vect3d force;
    for (int j=0; j < n; j++) {
      if (idx != j) {
        vect3d v = (p[j]->pt) - (p[idx]->pt);
        double r = v.Length();
        force = force + (v * (p[j]->mass * p[idx]->mass * gGrav /(r*r*r)));
      }
    }
    //Err += pow((point3d(force)-point3d(p[idx]->force)).Length()/force.Length(),
    //2.0);
    Err += (point3d(force)-point3d(p[idx]->force)).Length()/force.Length();
  }
  //return sqrt(Err/nCheck);
  return Err/nCheck;
}

// *************************************************************
//    STATISTICS
// *************************************************************

int gDirectInteractions = 0; // counts number of direct interactions
int gIndirectInteractions = 0; // counts number of indirect interactions
int sequential = 0;

// *************************************************************
//    FORCE CALCULATIONS
// *************************************************************

struct centerMass {
  point3d center;
  double mass;
  centerMass(point3d c) : center(c), mass(0.0) {}
  centerMass(point3d c, double m) : center(c), mass(m) {}
  centerMass() : center(point3d(0,0,0)), mass(0.0) {}
  centerMass& operator+=(centerMass op) {
    if (mass == 0.0) center = op.center;
    else center = center + (op.center - center)* (op.mass/(op.mass+mass));
    mass += op.mass;
    return *this;
  }
  centerMass& operator+=(particle* op) {
    if (mass == 0.0) center = op->pt;
    else center = center + (op->pt - center)*(op->mass/(op->mass+mass));
    mass += op->mass;
    return *this;
  }
  vect3d force(point3d y, double ymass) {
    vect3d v = center - y;
    double r2 = v.dot(v);
    return v * (mass * ymass / (r2*sqrt(r2)));
  }
};


#define terms 3

Transform<terms>* TRglobal = new Transform<terms>();

struct multipole {
  Transform<terms>* TR;
  complex<double> coefficients[terms*terms];
  point3d center;
  multipole(point3d c) : TR(TRglobal), center(c) {
    for (int i=0; i < terms*terms; i++) coefficients[i] = 0.0;
  }
  multipole& operator+=(particle* op) {
    TR->P2Madd(coefficients, op->mass, center, op->pt);
    return *this;
  }
  multipole& operator+=(multipole y) {
    TR->M2Madd(coefficients, center, y.coefficients, y.center);
    return *this;
  }
  
  vect3d force(point3d y, double mass) {
    vect3d result;
    double potential;
    TR->M2P(potential, result, y, coefficients, center);
    result = result*mass;
    return result;
  }
};


// Create an octree type with
//   point type = point3d
//   vector type = vect3d
//   type of elements stored in the tree = particle
//   type of summary data at each internal node = centerMass
typedef gTreeNode<point3d,vect3d,particle,centerMass> octTree;

//using multipole is not fast enough without first optimizizing it
//   for low p
//typedef gTreeNode<point3d,vect3d,particle,multipole> octTree;

// Calculate the force from p to nodes in an octree T
vect3d forceTo(particle *p, octTree *T, double alpha) {
  double alpha2 = alpha*alpha;
  vect3d v = T->data.center - p->pt;
  double r2 = v.dot(v);
  
  // If far enough away, approximate force on certer of mass of T
  if (r2 > alpha2 * T->size *T->size) {
    vect3d v = T->data.center - p->pt;
    double r2 = v.dot(v);
    if (sequential) gIndirectInteractions++;
    return T->data.force(p->pt, p->mass);
  }
  
  else {
    vect3d force(0.0, 0.0, 0.0);
    
    // If node is a leaf loop over the particles in the leaf
    if (T->IsLeaf()) {
      for (int i = 0; i < T->count; i++) {
        particle *pb = T->vertices[i];
        if (pb != p) {
          vect3d v = (pb->pt) - (p->pt);
          double r2 = v.dot(v);
          force = force + (v * (p->mass * pb->mass / (r2*sqrt(r2))));
        } else if (sequential) gDirectInteractions--;
      }
      if (sequential) gDirectInteractions += T->count;
      return force;
    }
    
    // Otherwise loop over children of the tree
    else {
      for (int i=0; i < 8 ; i++)
        force = force + forceTo(p,T->children[i],alpha);
      return force;
    }
  }
}

// *************************************************************
//   STEP
// *************************************************************

// takes one step and places forces in particles[i]->force
void stepBH(particle** particles, int n, double alpha) {
  TRglobal->precompute();
  //startTime();
  
  // generate the oct tree
  octTree *myTree = octTree::gTree(particles, n);
  //nextTime("tree");
  
  // this reorders the particles for locality
  particle** p2 = myTree->flatten();
  //nextTime("flatten");
  
  // generate all the forces
  native::parallel_for(int(0), n, [&] (int i) {
    p2[i]->force = forceTo(p2[i],myTree,alpha);
  });
//  nextTime("forces");
  myTree->del();
  if (sequential)
    cout << "Direct = " << gDirectInteractions << " Indirect = " << gIndirectInteractions << endl;
  //cout << "  Sampled RMS Error = "<< check(particles,n) << endl;
}

void nbody(particle** particles, int n) { 
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
    pbbs::nbody(p, n);
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

