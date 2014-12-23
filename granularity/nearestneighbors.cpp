#include <iostream>
#include <limits>
#include "sequence.hpp"
#include "geometryData.hpp"
#include "blockradixsort.hpp"
#include "cmdline.hpp"
#include "granularity-lite.hpp"

#ifdef CMDLINE
  typedef pasl::sched::granularity::control_by_cmdline control_type;
#elif PREDICTION
  typedef pasl::sched::granularity::control_by_prediction control_type;
#elif CUTOFF_WITH_REPORTING
  typedef pasl::sched::granularity::control_by_cutoff_with_reporting control_type;
#elif CUTOFF_WITHOUT_REPORTING
  typedef pasl::sched::granularity::control_by_cutoff_without_reporting control_type;
#endif

#ifdef BINARY
  typedef pasl::sched::granularity::loop_by_eager_binary_splitting<control_type> loop_type;
#elif LAZY_BINARY
  typedef pasl::sched::granularity::loop_by_lazy_binary_splitting<control_type> loop_type;
#elif SCHEDULING
  typedef pasl::sched::granularity::loop_by_lazy_binary_splitting_scheduling<control_type> loop_type;
#elif BINARY_WITH_SAMPLING
  typedef pasl::sched::granularity::loop_control_with_sampling<loop_by_eager_binary_splitting<control_type>> loop_type;
#elif BINARY_SEARCH
  typedef pasl::sched::granularity::loop_by_binary_search_splitting<control_type> loop_type;
#elif LAZY_BINARY_SEARCH
  typedef pasl::sched::granularity::loop_by_lazy_binary_search_splitting<control_type> loop_type;
#elif BINARY_SEARCH_WITH_SAMPLING
  typedef pasl::sched::granularity::loop_control_with_sampling<loop_by_binary_search_splitting<control_type>> loop_type;
#endif

pasl::sched::granularity::loop_by_eager_binary_splitting<control_type> cbuild("build");
//loop_type cbuild("build");
int build_cutoff_const;
loop_type crun("run");
int run_cutoff_const;

using namespace std;
template <class PT, int KK>
struct vertex {
  typedef PT pointT;
  int identifier;
  pointT pt;         // the point itself
  vertex* ngh[KK];    // the list of neighbors
  vertex(pointT p, int id) : pt(p), identifier(id) {}
  vertex();

};

// *************************************************************
//    QUAD/OCT TREE NODES
// *************************************************************

#define gMaxLeafSize 16  // number of points stored in each leaf

template <class intT, class vertex, class point>
struct nData {
  intT cnt;
  nData(intT x) : cnt(x) {}
  nData(point center) : cnt(0) {}
  nData& operator+=(const nData op) {cnt += op.cnt; return *this;}
  nData& operator+=(const vertex* op) {cnt += 1; return *this;}
};

template <class intT, class pointT, class vectT, class vertexT, class nodeData = nData<intT,vertexT,pointT> >
class gTreeNode {
  private :
  
  public :
  typedef pointT point;
  typedef vectT fvect;
  typedef vertexT vertex;
  struct getPoint {point operator() (vertex* v) {return v->pt;}};
  struct minpt {point operator() (point a, point b) {return a.minCoords(b);}};
  struct maxpt {point operator() (point a, point b) {return a.maxCoords(b);}};
  
  point center; // center of the box
  double size;   // width of each dimension, which have to be the same
  nodeData data; // total mass of vertices in the box
  intT count;  // number of vertices in the box
  gTreeNode *children[8];
  vertex **vertices;
  
  // wraps a bounding box around the points and generates
  // a tree.
  static gTreeNode* gTree(vertex** vv, intT n) {
    
    // calculate bounding box
    point* pt = newA(point, n);
    // copying to an array of points to make reduce more efficient
    pasl::sched::native::parallel_for(intT(0), n, [&] (intT i) {
      pt[i] = vv[i]->pt;
    });
    point minPt = pbbs::sequence::reduce(pt, n, minpt());
    point maxPt = pbbs::sequence::reduce(pt, n, maxpt());
    free(pt);
    //cout << "min "; minPt.print(); cout << endl;
    //cout << "max "; maxPt.print(); cout << endl;
    fvect box = maxPt-minPt;
    point center = minPt+(box/2.0);
    
    // copy before calling recursive routine since recursive routine is destructive
    vertex** v = newA(vertex*,n);
    pasl::sched::native::parallel_for(intT(0), n, [&] (intT i) {
      v[i] = vv[i];
    });
    //cout << "about to build tree" << endl;
    
    gTreeNode* result = new gTreeNode(v, n, center, box.maxDim());
    free(v);
    return result;
  }
  
  int IsLeaf() { return (vertices != NULL); }
  
  void del() {
    if (IsLeaf()) delete [] vertices;
    else {
      for (int i=0 ; i < (1 << center.dimension()); i++) {
        children[i]->del();
        delete children[i];
      }
    }
  }
  
  // Returns the depth of the tree rooted at this node
  intT Depth() {
    intT depth;
    if (IsLeaf()) depth = 0;
    else {
      depth = 0;
      for (intT i=0 ; i < (1 << center.dimension()); i++)
        depth = max<intT>(depth,children[i]->Depth());
    }
    return depth+1;
  }
  
  // Returns the size of the tree rooted at this node
  intT Size() {
    intT sz;
    if (IsLeaf()) {
      sz = count;
      for (intT i=0; i < count; i++)
        if (vertices[i] < ((vertex*) NULL)+1000)
          cout << "oops: " << vertices[i] << "," << count << "," << i << endl;
    }
    else {
      sz = 0;
      for (int i=0 ; i < (1 << center.dimension()); i++)
        sz += children[i]->Size();
    }
    return sz;
  }
  
  template <class F>
  void applyIndex(intT s, F f) {
    if (IsLeaf())
      for (intT i=0; i < count; i++) f(vertices[i],s+i);
    else {
      intT ss = s;
      int nb = (1 << center.dimension());
      intT* pss = newA(intT, nb);
      for (int i=0 ; i < nb; i++) {
        pss[i] = ss;
        ss += children[i]->count;
      }
      pasl::sched::native::parallel_for(int(0), nb, [&] (int i) {
        children[i]->applyIndex(pss[i],f);
      });

;
      free(pss);
    }
  }
  
  struct flatten_FA {
    vertex** _A;
    flatten_FA(vertex** A) : _A(A) {}
    void operator() (vertex* p, intT i) {
      _A[i] = p;}
  };
  
  vertex** flatten() {
    vertex** A = new vertex*[count];
    applyIndex(0,flatten_FA(A));
    return A;
  }
  
  // Returns the child the vertex p appears in
  int findQuadrant (vertex* p) {
    return (p->pt).quadrant(center); }
  
  // A hack to get around Cilk shortcomings
  static gTreeNode *newTree(vertex** S, intT n, point cnt, double sz) {
    return new gTreeNode(S, n, cnt, sz); }
  
  // Used as a closure in collectD
  struct findChild {
    gTreeNode *tr;
    findChild(gTreeNode *t) : tr(t) {}
    int operator() (vertex* p) {
      int r = tr->findQuadrant(p);
      return r;}
  };
  
  // the recursive routine for generating the tree -- actually mutually recursive
  // due to newTree
  gTreeNode(vertex** S, intT n, point cnt, double sz) : data(nodeData(cnt))
  {
    //cout << "n=" << n << endl;
    count = n;
    size = sz;
    center = cnt;
    vertices = NULL;
    int quadrants = (1 << center.dimension());
    
    if (count > gMaxLeafSize) {
      intT offsets[8];
      pbbs::intSort::iSort(S, offsets, n, (intT)quadrants, findChild(this));
      if (0) {
        for (intT i=0; i < n; i++) {
          cout << "  " << i << ":" << this->findQuadrant(S[i]);
        }
      }
      //for (int i=0; i < quadrants; i++)
      //cout << i << ":" << offsets[i] << "  ";
      //cout << endl;
      // Give each child its appropriate center and size
      // The centers are offset by size/4 in each of the dimensions

#ifdef LITE
      pasl::sched::granularity::parallel_for(cbuild, 
        [&] (int L, int R) {return ((R == quadrants) ? n : offsets[R]) - offsets[L] <= build_cutoff_const;},
        [&] (int L, int R) {
        return ((R == quadrants) ? n : offsets[R]) - offsets[L];},
        int(0), quadrants, [&] (int i) {
        point newcenter = center.offsetPoint(i, size/4.0);
        intT l = ((i == quadrants-1) ? n : offsets[i+1]) - offsets[i];
        children[i] = newTree(S + offsets[i], l, newcenter, size/2.0);
      });
#elif STANDART
      pasl::sched::native::parallel_for(
        int(0), quadrants, [&] (int i) {
        point newcenter = center.offsetPoint(i, size/4.0);
        intT l = ((i == quadrants-1) ? n : offsets[i+1]) - offsets[i];
        children[i] = newTree(S + offsets[i], l, newcenter, size/2.0);
      });;
#endif
      
      data = nodeData(center);
      for (int i=0 ; i < quadrants; i++)
        if (children[i]->count > 0)
          data += children[i]->data;
    } else {
      vertices = new vertex*[count];
      data = nodeData(center);
      for (intT i=0; i < count; i++) {
        vertex* p = S[i];
        data += p;
        vertices[i] = p;
      }
    }
  }
};

// A k-nearest neighbor structure
// requires vertexT to have pointT and vectT typedefs
template <class intT, class vertexT, int maxK>
struct kNearestNeighbor {
  typedef vertexT vertex;
  typedef typename vertexT::pointT point;
  typedef typename point::vectT fvect;
  
  typedef gTreeNode<intT,point,fvect,vertex> qoTree;
  qoTree *tree;
  
  // generates the search structure
  kNearestNeighbor(vertex** vertices, int n) {
    tree = qoTree::gTree(vertices, n);
  }
  kNearestNeighbor() : tree(NULL){};
  
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
;
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
struct AbstractRunner {
  virtual void initialize() = 0;
  virtual void run() = 0;
  virtual void free() = 0;
};

template <class intT, class pointT, int maxK>
struct Runner : AbstractRunner {
  typedef vertex<pointT, maxK> vertexT;
  int n, k;
  vertexT** v;
  vertexT** vr;
  typedef kNearestNeighbor<intT, vertexT, maxK> kNNT;
  kNNT T;

  Runner(vertexT** _v, int _n, int _k) : v(_v), n(_n), k(_k){
  }

  void initialize() {
    T = kNNT(v, n);
    vr = T.vertices();
  }

  void run() {
    std::cerr << "HUI!" << std::endl;
    // find nearest k neighbors for each point
#ifdef LITE
    pasl::sched::granularity::parallel_for(crun,
      [&] (int L, int R) {return (R - L) * k * log(n) <= run_cutoff_const;},
      [&] (int L, int R) {return (R - L) * k * log(n);},
      int(0), n, [&] (int i) {
      T.kNearest(vr[i], vr[i]->ngh, k);
    });
#elif STANDART
    pasl::sched::native::parallel_for1(int(0), n, [&] (int i) {
      T.kNearest(vr[i], vr[i]->ngh, k);
    });
#endif
/*    for (int i = 0; i < n; i++) {
      T.kNearest(vr[i], vr[i]->ngh, k);
    };*/
  }

  void output() {
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < k; j++) {
        std::cout << vr[i]->ngh[j].identifier << " ";
      }
      std::cout << std::endl;
    }
  }

  void free() {
    //free(vr);
    T.del();
  }
};

template <class point, int maxK>
vertex<point, maxK>** preparePoints(int n, point* points) {
  typedef vertex<point, maxK> vertex;
  vertex** v = newA(vertex*,n);
  vertex* vv = newA(vertex, n);
  pasl::sched::native::parallel_for(0, n, [&] (int i) {
    v[i] = new (&vv[i]) vertex(points[i], i);
  });
  return v;
}

void initialization() {
  pasl::util::ticks::set_ticks_per_seconds(1000);
  cbuild.initialize(1);
  crun.initialize(1, 10);
//  pasl::sched::granularity::execmode.init(pasl::sched::granularity::dynidentifier<pasl::sched::granularity::execmode_type>());
}

int main(int argc, char** argv) {
  int n = 0;
  AbstractRunner* runner = NULL;
  
  auto init = [&] {
    initialization();
    n = pasl::util::cmdline::parse_or_default_int(
        "n", 1000);
    int d = pasl::util::cmdline::parse_or_default_int(
        "d", 2);
    int k = pasl::util::cmdline::parse_or_default_int(
        "k", 2);
    std::string gen_type = pasl::util::cmdline::parse_or_default_string(
        "gen", "uniform");
    bool inSphere = pasl::util::cmdline::parse_or_default_bool(
        "in-sphere", false);
    bool onSphere = pasl::util::cmdline::parse_or_default_bool(
        "on-sphere", false);
    build_cutoff_const = pasl::util::cmdline::parse_or_default_int(
        "build_cutoff", 1000);
    run_cutoff_const = pasl::util::cmdline::parse_or_default_int(
        "run_cutoff", 1000);

    if (d == 2) {
      if (gen_type.compare("uniform") == 0) {
        pbbs::point2d* points = pbbs::uniform2d(inSphere, onSphere, n);
        runner = new Runner<int, pbbs::point2d, 20>(preparePoints<pbbs::point2d, 20>(n, points), n, k);
      } else if (gen_type.compare("plummer") == 0) {        pbbs::point2d* points = pbbs::plummer2d(n);
        runner = new Runner<int, pbbs::point2d, 20>(preparePoints<pbbs::point2d, 20>(n, points), n, k);
      } else {
        std::cerr << "Wrong generator type " << gen_type << "\n";
        exit(-1);
      }
    } else {
      if (gen_type.compare("uniform") == 0) {
        pbbs::point3d* points = pbbs::uniform3d<int, int>(inSphere, onSphere, n);
        runner = new Runner<int, pbbs::point3d, 20>(preparePoints<pbbs::point3d, 20>(n, points), n, k);
      } else if (gen_type.compare("plummer") == 0) {
        pbbs::point3d* points = pbbs::plummer3d<int, int>(n);
        runner = new Runner<int, pbbs::point3d, 20>(preparePoints<pbbs::point3d, 20>(n, points), n, k);
      } else {
        std::cerr << "Wrong generator type " << gen_type << "\n";
        exit(-1);
      }
    }
    std::string running_mode = pasl::util::cmdline::parse_or_default_string(
          "mode", std::string("by_force_sequential"));

    #ifdef CMDLINE
      std::cout << "Using " << running_mode << " mode" << std::endl;
    #elif PREDICTION
      std::cout << "Using by_prediction mode" << std::endl;
    #elif CUTOFF_WITH_REPORTING
      std::cout << "Using by_cutoff_with_reporting mode" << std::endl;
    #elif CUTOFF_WITHOUT_REPORTING        
      std::cout << "Using by_cutoff_without_reporting mode" << std::endl;
    #endif

    cbuild.set(running_mode);crun.set(running_mode);
  };

  auto run = [&] (bool sequential) {
    runner->initialize();
    runner->run();
  };

  auto output = [&] {
    std::cout << "The evaluation have finished" << std::endl;
  };
  auto destroy = [&] {
    runner->free();
  };

  pasl::sched::launch(argc, argv, init, run, output, destroy);
}

