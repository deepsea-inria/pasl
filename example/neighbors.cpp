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
#include "geometryio.hpp"
#include "randperm.hpp"
#include "sequenceio.hpp"
#include "benchmark.hpp"


namespace pbbs {

  using namespace std;
  using namespace pbbs::benchIO;


  
// A k-nearest neighbor structure
// requires vertexT to have pointT and vectT typedefs
template <class vertexT, int maxK>
struct kNearestNeighbor {
  typedef vertexT vertex;
  typedef typename vertexT::pointT point;
  typedef typename point::vectT fvect;

  typedef gTreeNode<int,point,fvect,vertex> qoTree;
  qoTree *tree;

  // generates the search structure
  kNearestNeighbor(vertex** vertices, int n) {
    tree = qoTree::gTree(vertices, n);
  }

  // returns the vertices in the search structure, in an
  //  order that has spacial locality
  vertex** vertices() {
    return tree->flatten();
  }

  void del() {tree->del();}

  struct kNN {
    vertex *ps;  // the element for which we are trying to find a NN
    vertex *pn[maxK];  // the current k nearest neighbors (nearest last)
    double rn[maxK]; // radius of current k nearest neighbors
    int quads;
    int k;
    kNN() {}

    // returns the ith smallest element (0 is smallest) up to k-1
    vertex* operator[] (const int i) { return pn[k-i-1]; }

    kNN(vertex *p, int kk) {
      if (kk > maxK) {cout << "k too large in kNN" << endl; abort();}
      k = kk;
      quads = (1 << (p->pt).dimension());
      ps = p;
      for (int i=0; i<k; i++) {
	pn[i] = (vertex*) NULL; 
	rn[i] = numeric_limits<double>::max();
      }
    }

    // if p is closer than pn then swap it in
    void update(vertex *p) { 
      //inter++;
      point opt = (p->pt);
      fvect v = (ps->pt) - opt;
      double r = v.Length();
      if (r < rn[0]) {
	pn[0]=p; rn[0] = r;
	for (int i=1; i < k && rn[i-1]<rn[i]; i++) {
	  swap(rn[i-1],rn[i]); swap(pn[i-1],pn[i]); }
      }
    }

    // looks for nearest neighbors in boxes for which ps is not in
    void nearestNghTrim(qoTree *T) {
      if (!(T->center).outOfBox(ps->pt, (T->size/2)+rn[0]))
	if (T->IsLeaf())
	  for (int i = 0; i < T->count; i++) update(T->vertices[i]);
	else 
	  for (int j=0; j < quads; j++) nearestNghTrim(T->children[j]);
    }

    // looks for nearest neighbors in box for which ps is in
    void nearestNgh(qoTree *T) {
      if (T->IsLeaf())
	for (int i = 0; i < T->count; i++) {
	  vertex *pb = T->vertices[i];
	  if (pb != ps) update(pb);
	}
      else {
	int i = T->findQuadrant(ps);
	nearestNgh(T->children[i]);
	for (int j=0; j < quads; j++) 
	  if (j != i) nearestNghTrim(T->children[j]);
      }
    }
  };

  vertex* nearest(vertex *p) {
    kNN nn(p,1);
    nn.nearestNgh(tree); 
    return nn[0];
  }

  // version that writes into result
  void kNearest(vertex *p, vertex** result, int k) {
    kNN nn(p,k);
    nn.nearestNgh(tree); 
    for (int i=0; i < k; i++) result[i] = 0;
    for (int i=0; i < k; i++) result[i] = nn[i];
  }

  // version that allocates result
  vertex** kNearest(vertex *p, int k) {
    vertex** result = newA(vertex*,k);
    kNearest(p,result,k);
    return result;
  }

};

// find the k nearest neighbors for all points in tree
// places pointers to them in the .ngh field of each vertex
template <int maxK, class vertexT>
void ANN(vertexT** v, int n, int k) {
  typedef kNearestNeighbor<vertexT,maxK> kNNT;

  kNNT T = kNNT(v, n);

  //cout << "built tree" << endl;

  // this reorders the vertices for locality
  vertexT** vr = T.vertices();

  // find nearest k neighbors for each point
  native::parallel_for(0, n, [&] (int i) {
    T.kNearest(vr[i], vr[i]->ngh, k);
    });
  
  free(vr);
  T.del();
}

// *************************************************************
//  SOME DEFINITIONS
// *************************************************************

#define K 10

template <class PT, int KK>
struct vertex {
  typedef PT pointT;
  int identifier;
  pointT pt;         // the point itself
  vertex* ngh[KK];    // the list of neighbors
  vertex(pointT p, int id) : pt(p), identifier(id) {}

};

static constexpr int maxK = 10;
typedef vertex<point2d,maxK> vertex2d;
typedef vertex<point3d,maxK> vertex3d;

template <class T>
void free_if_nonnull(T* p) {
  if (p != nullptr)
    free(p);
}

template <class intT, class uintT>
void doit(int argc, char** argv) {
  vertex2d** v = nullptr;
  vertex2d* vv = nullptr;
  vertex3d** v3d = nullptr;
  vertex3d* vv3d = nullptr;
  int dimensions;
  _seq<point2d> PIn2d;
  _seq<point3d> PIn3d;
  auto init = [&] {
    std::string infile = pasl::util::cmdline::parse_or_default_string("infile", "");
    dimensions = pasl::util::cmdline::parse_int("dimensions");
    char* s = (char*)infile.c_str();
    if (dimensions == 2) {
      PIn2d = readPointsFromFile<point2d>(s);
      v = newA(vertex2d*,PIn2d.n);
      vv =  newA(vertex2d, PIn2d.n);
      point2d* pts = PIn2d.A;
      {for (int i=0; i < PIn2d.n; i++) 
          v[i] = new (&vv[i]) vertex2d(pts[i],i);}
    } else if (dimensions == 3) {
      PIn3d = readPointsFromFile<point3d>(s);
      v3d = newA(vertex3d*,PIn3d.n);
      vv3d =  newA(vertex3d, PIn3d.n);
      point3d* pts = PIn3d.A;
      {for (int i=0; i < PIn3d.n; i++) 
          v3d[i] = new (&vv3d[i]) vertex3d(pts[i],i);}
    }
  };
  auto run = [&] (bool sequential) {
    if (dimensions == 2) {
      ANN<maxK>(v, PIn2d.n, 1);
    } else if (dimensions == 3) {
      ANN<maxK>(v3d, PIn3d.n, 1);
    }
  };
  auto output = [&] {
    
  };
  auto destroy = [&] {
    free_if_nonnull(v);
    free_if_nonnull(vv);
    free_if_nonnull(v3d);
    free_if_nonnull(vv3d);
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}
  
} // end namespace


int main(int argc, char** argv) {
  pbbs::doit<int, unsigned>(argc, argv);
  return 0;
}
