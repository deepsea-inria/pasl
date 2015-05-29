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
#include <vector>
#include <vector>
#include "dpsdatapar.hpp"
#include "geometry.hpp"
#include "nearestneighbors.hpp"
#include "topology.hpp"
#include "deterministichash.hpp"
#include "delaunay.hpp"

#ifndef _PCTL_DELAUNAY_REFINE_H_
#define _PCTL_DELAUNAY_REFINE_H_

// *************************************************************
//   PARALLEL HASH TABLE TO STORE WORK QUEUE OF SKINNY TRIANGLES
// *************************************************************

namespace pasl {
namespace pctl {

struct hashTriangles {
  typedef tri* eType;
  typedef tri* kType;
  eType empty() {return NULL;}
  kType getKey(eType v) { return v;}
  uintT hash(kType s) { return prandgen::hashi(s->id); }
  int cmp(kType s, kType s2) {
    return (s->id > s2->id) ? 1 : ((s->id == s2->id) ? 0 : -1);
  }
  bool replaceQ(eType s, eType s2) {return 0;}
};

typedef Table<hashTriangles,intT> TriangleTable;
TriangleTable makeTriangleTable(intT m) {return TriangleTable(m,hashTriangles());}

// *************************************************************
//   THESE ARE TAKEN FROM delaunay.C
//   Perhaps should be #included
// *************************************************************

// Holds vertex and simplex queues used to store the cavity created
// while searching from a vertex between when it is initially searched
// and later checked to see if all corners are reserved.
  /*
struct Qs {
  vector<vertex*> vertexQ;
  vector<simplex> simplexQ;
}; */

// Recursive routine for finding a cavity across an edge with
// respect to a vertex p.
// The simplex has orientation facing the direction it is entered.
//
//         a
//         | \ --> recursive call
//   p --> |T c
// enter   | / --> recursive call
//         b
//
//  If p is in circumcircle of T then
//     add T to simplexQ, c to vertexQ, and recurse
  /*
void findCavity(simplex t, vertex *p, Qs *q) {
  if (t.inCirc(p)) {
    q->simplexQ.push_back(t);
    t = t.rotClockwise();
    findCavity(t.across(), p, q);
    q->vertexQ.push_back(t.firstVertex());
    t = t.rotClockwise();
    findCavity(t.across(), p, q);
  }
} */

// Finds the cavity for v and tries to reserve vertices on the
// boundary (v must be inside of the simplex t)
// The boundary vertices are pushed onto q->vertexQ and
// simplices to be deleted on q->simplexQ (both initially empty)
// It makes no side effects to the mesh other than to X->reserve
  /*
void reserveForInsert(vertex *v, simplex t, Qs *q) {
  // each iteration searches out from one edge of the triangle
  for (int i=0; i < 3; i++) {
    q->vertexQ.push_back(t.firstVertex());
    findCavity(t.across(), v, q);
    t = t.rotClockwise();
  }
  // the maximum id new vertex that tries to reserve a boundary vertex
  // will have its id written.  reserve starts out as -1
  for (intT i = 0; i < q->vertexQ.size(); i++)
    utils::writeMax(&((q->vertexQ)[i]->reserve), v->id);
} */

// *************************************************************
//   DEALING WITH THE CAVITY
// *************************************************************

inline bool skinnyTriangle(tri *t) {
  double minAngle = 30;
  if (minAngleCheck(t->vtx[0]->pt, t->vtx[1]->pt, t->vtx[2]->pt, minAngle))
    return 1;
  return 0;
}

inline bool obtuse(simplex t) {
  int o = t.o;
  point2d p0 = t.t->vtx[(o+1)%3]->pt;
  vect2d v1 = t.t->vtx[o]->pt - p0;
  vect2d v2 = t.t->vtx[(o+2)%3]->pt - p0;
  return (v1.dot(v2) < 0.0);
}

inline point2d circumcenter(simplex t) {
  if (t.isTriangle())
    return triangleCircumcenter(t.t->vtx[0]->pt, t.t->vtx[1]->pt, t.t->vtx[2]->pt);
  else { // t.isBoundary()
    point2d p0 = t.t->vtx[(t.o+2)%3]->pt;
    point2d p1 = t.t->vtx[t.o]->pt;
    return p0 + (p1-p0)/2.0;
  }
}

// this side affects the simplex by moving it into the right orientation
// and setting the boundary if the circumcenter encroaches on a boundary
inline bool checkEncroached(simplex& t) {
  if (t.isBoundary()) return 0;
  int i;
  for (i=0; i < 3; i++) {
    if (t.across().isBoundary() && (t.farAngle() > 45.0)) break;
    t = t.rotClockwise();
  }
  if (i < 3) return t.boundary = 1;
  else return 0;
}

bool findAndReserveCavity(vertex* v, simplex& t, Qs* q) {
  t = simplex(v->badT,0);
  if (t.t == NULL) {cout << "refine: nothing in badT" << endl; abort();}
  if (t.t->bad == 0) return 0;
  //t.t->bad = 0;
  
  // if there is an obtuse angle then move across to opposite triangle, repeat
  if (obtuse(t)) t = t.across();
  while (t.isTriangle()) {
    int i;
    for (i=0; i < 2; i++) {
      t = t.rotClockwise();
      if (obtuse(t)) { t = t.across(); break; }
    }
    if (i==2) break;
  }
  
  // if encroaching on boundary, move to boundary
  checkEncroached(t);
  
  // use circumcenter to add (if it is a boundary then its middle)
  v->pt = circumcenter(t);
  reserveForInsert(v, t, q);
  return 1;
}

// checks if v "won" on all adjacent vertices and inserts point if so
// returns true if "won" and cavity was updated
bool addCavity(vertex *v, simplex t, Qs *q, TriangleTable TT) {
  bool flag = 1;
  for (intT i = 0; i < q->vertexQ.size(); i++) {
    vertex* u = (q->vertexQ)[i];
    if (u->reserve == v->id) u->reserve = -1; // reset to -1
    else flag = 0; // someone else with higher priority reserved u
  }
  if (flag) {
    tri* t0 = t.t;
    tri* t1 = v->t;  // the memory for the two new triangles
    tri* t2 = t1 + 1;
    t1->initialized = 1;
    if (t.isBoundary()) t.splitBoundary(v, t1);
    else {
      t2->initialized = 1;
      t.split(v, t1, t2);
    }
    
    // update the cavity
    for (intT i = 0; i<q->simplexQ.size(); i++)
      (q->simplexQ)[i].flip();
    q->simplexQ.push_back(simplex(t0,0));
    q->simplexQ.push_back(simplex(t1,0));
    if (!t.isBoundary()) q->simplexQ.push_back(simplex(t2,0));
    
    for (intT i = 0; i<q->simplexQ.size(); i++) {
      tri* t = (q->simplexQ)[i].t;
      if (skinnyTriangle(t)) {
        TT.insert(t);
        t->bad = 1;}
      else t->bad = 0;
    }
    v->badT = NULL;
  }
  q->simplexQ.clear();
  q->vertexQ.clear();
  return flag;
}

// *************************************************************
//    MAIN REFINEMENT LOOP
// *************************************************************

intT addRefiningVertices(vertex** v, intT n, intT nTotal, TriangleTable TT) {
  intT maxR = (intT) (nTotal/500) + 1; // maximum number to try in parallel
  parray<Qs> qqs(maxR);
  parray<Qs*> qs(maxR);
  for (intT i=0; i < maxR; i++) qs[i] = new (&qqs[i]) Qs;
  parray<simplex> t(maxR);
  parray<bool> flags(maxR);
  parray<vertex*> h(maxR);
  
  intT top = n; intT failed = 0;
  intT size = maxR;
  
  // process all vertices starting just below the top
  while(top > 0) {
    intT cnt = std::min<intT>(size,top);
    vertex** vv = v+top-cnt;
    
    parallel_for((intT)0, cnt, [&] (intT j) {
      flags[j] = findAndReserveCavity(vv[j],t[j],qs[j]);
    });
    
    parallel_for((intT)0, cnt, [&] (intT j) {
      flags[j] = flags[j] && !addCavity(vv[j], t[j], qs[j], TT);
    });
    
    // Pack the failed vertices back onto Q
    intT k = (intT)dps::pack(flags.cbegin(), vv, vv+cnt, h.begin());
    pmem::copy(h.cbegin(), h.cbegin()+k, vv);
    failed += k;
    top = top-cnt+k; // adjust top, accounting for failed vertices
  }
  return failed;
}

// *************************************************************
//    DRIVER
// *************************************************************

triangles<point2d> refine(triangles<point2d> Tri) {
  int expandFactor = 4;
  intT n = Tri.numPoints;
  intT m = Tri.numTriangles;
  intT extraVertices = expandFactor*n;
  intT totalVertices = n + extraVertices;
  intT totalTriangles = m + 2 * extraVertices;
  
  parray<vertex*> v(extraVertices);
  parray<vertex> vv(totalVertices);
  parray<tri> Triangs(totalTriangles);
  topologyFromTriangles(Tri, vv, Triangs);
  
  //  set up extra triangles
  parallel_for((intT)m, totalTriangles, [&] (intT i) {
    Triangs[i].id = i;
    Triangs[i].initialized = 0;
  });
  
  //  set up extra vertices
  parallel_for((intT)0, totalVertices-n, [&] (intT i) {
    v[i] = new (&vv[i+n]) vertex(point2d(0,0), i+n);
    // give each one a pointer to two triangles to use
    v[i]->t = Triangs.begin() + m + 2*i;
  });
  
  // these will increase as more are added
  intT numTriangs = m;
  intT numPoints = n;
  
  TriangleTable workQ = makeTriangleTable(numTriangs);
  parallel_for((intT)0, numTriangs, [&] (intT i) {
    if (skinnyTriangle(&Triangs[i])) {
      workQ.insert(&Triangs[i]);
      Triangs[i].bad = 1;
    }
  });
  
  // Each iteration processes all bad triangles from the workQ while
  // adding new bad triangles to a new queue
  while (1) {
    parray<tri*> badTT = workQ.entries();
    workQ.del();
    
    // packs out triangles that are no longer bad
    parray<bool> flags(badTT.size(), [&] (intT i) {
      return badTT[i]->bad;
    });
    parray<tri*> badT = pack(badTT.cbegin(), badTT.cend(), flags.cbegin());
    intT numBad = (intT)badT.size();
    
    //cout << "numBad = " << numBad << endl;
    if (numBad == 0) break;
    if (numPoints + numBad > totalVertices) {
      cout << "ran out of vertices" << endl;
      abort();
    }
    
    // allocate 1 vertex per bad triangle and assign triangle to it
    parallel_for((intT)0, numBad, [&] (intT i) {
      badT[i]->bad = 2; // used to detect whether touched
      v[i + numPoints - n]->badT = badT[i];
    });
    
    // the new work queue
    workQ = makeTriangleTable(numBad);
    
    // This does all the work
    addRefiningVertices(v.begin() + numPoints - n, numBad, numPoints, workQ);
    
    // push any bad triangles that were left untouched onto the Q
    parallel_for((intT)0, numBad, [&] (intT i) {
      if (badT[i]->bad==2) workQ.insert(badT[i]);
    });
    
    numPoints += numBad;
    numTriangs += 2*numBad;
  }
 
  // Extract Vertices for result
  parray<bool> flag(numTriangs, [&] (intT i) {
    return (vv[i].badT == NULL);
  });
  parray<long> I = pack_index(flag.cbegin(), flag.cbegin()+numPoints);
  intT nO = (intT)I.size();
  point2d* rp = newA(point2d, nO);
  parallel_for((intT)0, nO, [&] (intT i) {
    vv[I[i]].id = i;
    rp[i] = vv[I[i]].pt;
  });
  //cout << "total points = " << nO << endl;
  
  // Extract Triangles for result
  parallel_for((intT)0, numTriangs, [&] (intT i) {
    flag[i] = Triangs[i].initialized;
  });
  I = pack_index(flag.cbegin(), flag.cbegin()+numTriangs);
  triangle* rt = newA(triangle, I.size());
  parallel_for((intT)0, (intT)I.size(), [&] (intT i) {
    tri t = Triangs[I[i]];
    rt[i] = triangle(t.vtx[0]->id, t.vtx[1]->id, t.vtx[2]->id);
  });
  //cout << "total triangles = " << I.n << endl;
  
  return triangles<point2d>(nO, (intT)I.size(), rp, rt);
}
  
} // end namespace
} // end namespace

#endif /*! _PCTL_DELAUNAY_REFINE_H_ */