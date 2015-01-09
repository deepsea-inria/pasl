// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include <iostream>
#include <cstdlib>
#include "sequence.hpp"
#include "geometry.hpp"
#include "blockradixsort.hpp"

#ifndef _BENCH_OCTTREE_INCLUDED
#define _BENCH_OCTTREE_INCLUDED

namespace pbbs {
using namespace std;

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
    native::parallel_for(intT(0), n, [&] (intT i) {
      pt[i] = vv[i]->pt;
    });
    point minPt = sequence::reduce(pt, n, minpt());
    point maxPt = sequence::reduce(pt, n, maxpt());
    free(pt);
    //cout << "min "; minPt.print(); cout << endl;
    //cout << "max "; maxPt.print(); cout << endl;
    fvect box = maxPt-minPt;
    point center = minPt+(box/2.0);
    
    // copy before calling recursive routine since recursive routine is destructive
    vertex** v = newA(vertex*,n);
    native::parallel_for(intT(0), n, [&] (intT i) {
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
      native::parallel_for(int(0), nb, [&] (int i) {
        children[i]->applyIndex(pss[i],f);
      });
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
      intSort::iSort(S, offsets, n, (intT)quadrants, findChild(this));
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
      native::parallel_for(int(0), quadrants, [&] (int i) {
        point newcenter = center.offsetPoint(i, size/4.0);
        intT l = ((i == quadrants-1) ? n : offsets[i+1]) - offsets[i];
        children[i] = newTree(S + offsets[i], l, newcenter, size/2.0);
      });
      
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

} // end namespace

#endif // _BENCH_OCTTREE_INCLUDED
