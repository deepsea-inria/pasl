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

#include <float.h>

#include "geometry.hpp"
#include "dpsdatapar.hpp"
#include "raytriangleintersect.hpp"
#include "samplesort.hpp"
#include "ray.hpp"

#ifndef _PCTL_KDTREE_H_
#define _PCTL_KDTREE_H_

namespace pasl {
namespace pctl {
namespace kdtree {
    
// Stores coordinate of event along with index to its triangle and type
// Stores type of event (START or END) in lowest bit of index
struct event {
  float v;
  intT p;
  event(float value, intT index, bool type)
  : v(value), p((index << 1) + type) {}
  event() {}
};
#define START 0
#define IS_START(_event) (!(_event.p & 1))
#define END 1
#define IS_END(_event) ((_event.p & 1))
#define GET_INDEX(_event) (_event.p >> 1)

struct cmpVal { bool operator() (event a, event b) {return a.v < b.v;}};

struct range {
  float min;
  float max;
  range(float _min, float _max) : min(_min), max(_max) {}
  range() {}
};

typedef range* Boxes[3];
typedef event* Events[3];
typedef range BoundingBox[3];

static std::ostream& operator<<(std::ostream& os, const BoundingBox B) {
  return os << B[0].min << ":" << B[0].max << " + "
  << B[1].min << ":" << B[1].max << " + "
  << B[2].min << ":" << B[2].max;
}

struct cutInfo {
  float cost;
  float cutOff;
  intT numLeft;
  intT numRight;
  cutInfo(float _cost, float _cutOff, intT nl, intT nr)
  : cost(_cost), cutOff(_cutOff), numLeft(nl), numRight(nr) {}
  cutInfo() {}
};

template <class T>
class treeNode_contr {
public:
  static controller_type contr;
};

template <class T>
controller_type treeNode_contr<T>::contr("treeNode_delete");

struct treeNode {
  treeNode *left;
  treeNode *right;
  BoundingBox box;
  int cutDim;
  float cutOff;
  intT* triangleIndices;
  intT n;
  intT leaves;
  
  bool isLeaf() {return (triangleIndices != NULL);}
  
  treeNode(treeNode* L, treeNode* R,
           int _cutDim, float _cutOff, BoundingBox B)
  : left(L), right(R), triangleIndices(NULL), cutDim(_cutDim),
  cutOff(_cutOff) {
    for (int i=0; i < 3; i++) box[i] = B[i];
    n = L->n + R->n;
    leaves = L->leaves + R->leaves;
  }
  
  treeNode(Events E, intT _n, BoundingBox B)
  : left(NULL), right(NULL) {
    
    event* events = E[0];
    
    // extract indices from events
    triangleIndices = newA(intT, _n/2);
    intT k = 0;
    for (intT i = 0; i < _n; i++)
      if (IS_START(events[i]))
        triangleIndices[k++] = GET_INDEX(events[i]);
    
    n = _n/2;
    leaves = 1;
    for (int i=0; i < 3; i++) {
      box[i] = B[i];
      free(E[i]);
    }
  }
  
  static void del(treeNode *T) {
    using controller_type = treeNode_contr<int>;
    par::cstmt(controller_type::contr, [&] { return T->n; }, [&] {
      if (T->isLeaf())  free(T->triangleIndices);
      else {
        par::fork2([&] {
          del(T->left);
        }, [&] {
          del(T->right);
        });
      }
      free(T);
    });
  }
};

using namespace std;

int CHECK12 = 0;  // if set checks 10 rays against brute force method
int STATS = 0;  // if set prints out some tree statistics

// Constants for deciding when to stop recursion in building the KDTree
float CT = 6.0;
float CL = 1.25;
float maxExpand = 1.6;
int maxRecursionDepth = 25;

// Constant for switching to sequential versions
int minParallelSize = 500000;

typedef pointT::floatT floatT;
typedef _vect3d<floatT> vectT;
typedef triangles<pointT> trianglesT;
typedef ray<pointT> rayT;

float boxSurfaceArea(BoundingBox B) {
  float r0 = B[0].max-B[0].min;
  float r1 = B[1].max-B[1].min;
  float r2 = B[2].max-B[2].min;
  return 2*(r0*r1 + r1*r2 + r0*r2);
}

float epsilon = .0000001;
range fixRange(float minv, float maxv) {
  if (minv == maxv) return range(minv,minv+epsilon);
  else return range(minv,maxv);
}

inline float inBox(pointT p, BoundingBox B) {
  return (p.x >= (B[0].min - epsilon) && p.x <= (B[0].max + epsilon) &&
          p.y >= (B[1].min - epsilon) && p.y <= (B[1].max + epsilon) &&
          p.z >= (B[2].min - epsilon) && p.z <= (B[2].max + epsilon));
}

// sequential version of best cut
cutInfo bestCutSerial(event* E, range r, range r1, range r2, intT n) {
  if (r.max - r.min == 0.0) return cutInfo(FLT_MAX, r.min, n, n);
  float area = 2 * (r1.max-r1.min) * (r2.max-r2.min);
  float diameter = 2 * ((r1.max-r1.min) + (r2.max-r2.min));
  
  // calculate cost of each possible split
  intT inLeft = 0;
  intT inRight = n/2;
  float minCost = FLT_MAX;
  intT k = 0;
  intT rn = inLeft;
  intT ln = inRight;
  for (intT i=0; i <n; i++) {
    float cost;
    if (IS_END(E[i])) inRight--;
    float leftLength = E[i].v - r.min;
    float leftArea = area + diameter * leftLength;
    float rightLength = r.max - E[i].v;
    float rightArea = area + diameter * rightLength;
    cost = (leftArea * inLeft + rightArea * inRight);
    if (cost < minCost) {
      rn = inRight;
      ln = inLeft;
      minCost = cost;
      k = i;
    }
    if (IS_START(E[i])) inLeft++;
  }
  return cutInfo(minCost, E[k].v, ln, rn);
}

// parallel version of best cut
cutInfo bestCut(event* E, range r, range r1, range r2, intT n) {
  if (n < minParallelSize)
    return bestCutSerial(E, r, r1, r2, n);
  if (r.max - r.min == 0.0) return cutInfo(FLT_MAX, r.min, n, n);
  
  // area of two orthogonal faces
  float orthogArea = 2 * ((r1.max-r1.min) * (r2.max-r2.min));
  
  // length of diameter of orthogonal face
  float diameter = 2 * ((r1.max-r1.min) + (r2.max-r2.min));
  
  // count number that end before i
  parray<intT> upperC(n, [&] (intT i) {
    return IS_END(E[i]);
  });
  intT u = dps::scan(upperC.begin(), upperC.end(), (intT)0, [&] (intT x, intT y) {
    return x + y; }, upperC.begin(), forward_exclusive_scan);
  
  // calculate cost of each possible split location
  parray<float> cost(n, [&] (intT i) {
    intT inLeft = i - upperC[i];
    intT inRight = n/2 - (upperC[i] + IS_END(E[i]));
    float leftLength = E[i].v - r.min;
    float leftArea = orthogArea + diameter * leftLength;
    float rightLength = r.max - E[i].v;
    float rightArea = orthogArea + diameter * rightLength;
    return (leftArea * inLeft + rightArea * inRight);
  });
  
  // find minimum across all (maxIndex with less is minimum)
  intT k = (intT)max_index(cost.cbegin(), cost.cend(), cost[0], [&] (float x, float y) {
    return x < y;
  });
  
  float c = cost[k];
  intT ln = k - upperC[k];
  intT rn = n/2 - (upperC[k] + IS_END(E[k]));
  return cutInfo(c, E[k].v, ln, rn);
}

void splitEventsSerial(range* boxes, event* events,
                       float cutOff, intT n,
                       parray<event>& left, parray<event>& right) {
  intT l = 0;
  intT r = 0;
  parray<event> eventsLeft(n);
  parray<event> eventsRight(n);
  for (intT i=0; i < n; i++) {
    intT b = GET_INDEX(events[i]);
    if (boxes[b].min < cutOff) {
      eventsLeft[l++] = events[i];
      if (boxes[b].max > cutOff)
        eventsRight[r++] = events[i];
    } else eventsRight[r++] = events[i];
  }
  new (&left)  parray<event>(eventsLeft.begin(), eventsLeft.begin()+l);
  new (&right) parray<event>(eventsRight.begin(), eventsRight.begin()+r);
}

void splitEvents(range* boxes, event* events, float cutOff, intT n,
                 parray<event>& left, parray<event>& right) {
  if (n < minParallelSize)
    return splitEventsSerial(boxes, events, cutOff, n, left, right);
  parray<bool> lower(n, [&] (intT i) {
    intT b = GET_INDEX(events[i]);
    return boxes[b].min < cutOff;
  });
  parray<bool> upper(n, [&] (intT i) {
    intT b = GET_INDEX(events[i]);
    return boxes[b].max > cutOff;
  });
  const event* events2 = (const event*)events;
  left =  pack(events2, events2+n, lower.cbegin());
  right = pack(events2, events2+n, upper.cbegin());
}
  
template <class T>
class generateNode_contr {
public:
  static controller_type contr;
};

template <class T>
controller_type generateNode_contr<T>::contr("generateNode");

// n is the number of events (i.e. twice the number of triangles)
treeNode* generateNode(Boxes boxes, Events events, BoundingBox B,
                       intT n, intT maxDepth) {
  using controller_type = generateNode_contr<intT>;
  treeNode* result;
  par::cstmt(controller_type::contr, [&] { return n; }, [&] {
    //cout << "n=" << n << " maxDepth=" << maxDepth << endl;
    if (n <= 2 || maxDepth == 0) {
      result = new treeNode(events, n, B);
      return;
    }
    
    // loop over dimensions and find the best cut across all of them
    cutInfo cuts[3];
    parallel_for(0, 3, [&] (int) { return n; }, [&] (int d) {
      cuts[d] = bestCut(events[d], B[d], B[(d+1)%3], B[(d+2)%3], n);
    });
    
    int cutDim = 0;
    for (int d = 1; d < 3; d++)
      if (cuts[d].cost < cuts[cutDim].cost) cutDim = d;
    
    range* cutDimRanges = boxes[cutDim];
    float cutOff = cuts[cutDim].cutOff;
    float area = boxSurfaceArea(B);
    float bestCost = CT + CL * cuts[cutDim].cost/area;
    float origCost = (float) (n/2);
    
    // quit recursion early if best cut is not very good
    if (bestCost >= origCost ||
        cuts[cutDim].numLeft + cuts[cutDim].numRight > maxExpand * n/2) {
      result = new treeNode(events, n, B);
      return;
    }
    
    // declare structures for recursive calls
    BoundingBox BBL;
    for (int i=0; i < 3; i++) BBL[i] = B[i];
    BBL[cutDim] = range(BBL[cutDim].min, cutOff);
    event* leftEvents[3];
    intT nl;
    
    BoundingBox BBR;
    for (int i=0; i < 3; i++) BBR[i] = B[i];
    BBR[cutDim] = range(cutOff, BBR[cutDim].max);
    event* rightEvents[3];
    intT nr;
    
    // now split each event array to the two sides
    parray<event> Xl[3];
    parray<event> Xr[3];
    parallel_for(0, 3, [&] (int) { return n; }, [&] (int d) {
      splitEvents(cutDimRanges, events[d], cutOff, n, Xl[d], Xr[d]);
    });
    
    for (int d = 0; d < 3; d++) {
      leftEvents[d] = Xl[d].begin();
      rightEvents[d] = Xr[d].begin();
      if (d == 0) {
        nl = Xl[d].size();
        nr = Xr[d].size();
      } else if (Xl[d].size() != nl || Xr[d].size() != nr) {
        cout << "kdTree: mismatched lengths, something wrong" << endl;
        abort();
      }
    }
    
    // free old events and make recursive calls
    for (int i=0; i < 3; i++) free(events[i]);
    treeNode *L;
    treeNode *R;
    par::fork2([&] {
      L = generateNode(boxes, leftEvents, BBL, nl, maxDepth-1);
    }, [&] {
      R = generateNode(boxes, rightEvents, BBR, nr, maxDepth-1);
    });
    
    result = new treeNode(L, R, cutDim, cutOff, B);
  });
  return result;
}

intT tcount = 0;
intT ccount = 0;

// Given an a ray, a bounding box, and a sequence of triangles, returns the
// index of the first triangle the ray intersects inside the box.
// The triangles are given by n indices I into the triangle array Tri.
// -1 is returned if there is no intersection
intT findRay(rayT r, intT* I, intT n, triangles<pointT> Tri, BoundingBox B) {
  if (STATS) { tcount += n; ccount += 1;}
  pointT* P = Tri.P;
  floatT tMin = FLT_MAX;
  intT k = -1;
  for (intT i = 0; i < n; i++) {
    intT j = I[i];
    triangle* tr = Tri.T + j;
    pointT m[3] = {P[tr->C[0]],  P[tr->C[1]],  P[tr->C[2]]};
    floatT t = rayTriangleIntersect(r, m);
    if (t > 0.0 && t < tMin && inBox(r.o + r.d*t, B)) {
      tMin = t;
      k = j;
    }
  }
  return k;
}

// Given a ray and a tree node find the index of the first triangle the
// ray intersects inside the box represented by that node.
// -1 is returned if there is no intersection
intT findRay(rayT r, treeNode* TN, trianglesT Tri) {
  //cout << "TN->n=" << TN->n << endl;
  if (TN->isLeaf())
    return findRay(r, TN->triangleIndices, TN->n, Tri, TN->box);
  pointT o = r.o;
  vectT d = r.d;
  
  floatT oo[3] = {o.x,o.y,o.z};
  floatT dd[3] = {d.x,d.y,d.z};
  
  // intersect ray with splitting plane
  int k0 = TN->cutDim;
  int k1 = (k0 == 2) ? 0 : k0+1;
  int k2 = (k0 == 0) ? 2 : k0-1;
  point2d o_p(oo[k1], oo[k2]);
  vect2d d_p(dd[k1], dd[k2]);
  // does not yet deal with dd[k0] == 0
  floatT scale = (TN->cutOff - oo[k0])/dd[k0];
  point2d p_i = o_p + d_p * scale;
  
  range rx = TN->box[k1];
  range ry = TN->box[k2];
  floatT d_0 = dd[k0];
  
  // decide which of the two child boxes the ray intersects
  enum {LEFT, RIGHT, BOTH};
  int recurseTo = LEFT;
  if      (p_i.x < rx.min) { if (d_p.x*d_0 > 0) recurseTo = RIGHT;}
  else if (p_i.x > rx.max) { if (d_p.x*d_0 < 0) recurseTo = RIGHT;}
  else if (p_i.y < ry.min) { if (d_p.y*d_0 > 0) recurseTo = RIGHT;}
  else if (p_i.y > ry.max) { if (d_p.y*d_0 < 0) recurseTo = RIGHT;}
  else recurseTo = BOTH;
  
  if (recurseTo == RIGHT) return findRay(r, TN->right, Tri);
  else if (recurseTo == LEFT) return findRay(r, TN->left, Tri);
  else if (d_0 > 0) {
    intT t = findRay(r, TN->left, Tri);
    if (t >= 0) return t;
    else return findRay(r, TN->right, Tri);
  } else {
    intT t = findRay(r, TN->right, Tri);
    if (t >= 0) return t;
    else return findRay(r, TN->left, Tri);
  }
}

void processRays(trianglesT Tri, rayT* rays, intT numRays,
                 treeNode* R, intT* results) {
  parallel_for((intT)0, numRays, [&] (intT i) {
    results[i] = findRay(rays[i], R, Tri);
  });
}

parray<intT> rayCast(triangles<pointT> Tri, ray<pointT>* rays, intT numRays) {
  
  // Extract triangles into a separate array for each dimension with
  // the lower and upper bound for each triangle in that dimension.
  Boxes boxes;
  intT n = Tri.numTriangles;
  for (int d = 0; d < 3; d++) boxes[d] = newA(range, n);
  pointT* P = Tri.P;
  parallel_for((intT)0, n, [&] (intT i) {
    pointT p0 = P[Tri.T[i].C[0]];
    pointT p1 = P[Tri.T[i].C[1]];
    pointT p2 = P[Tri.T[i].C[2]];
    boxes[0][i] = fixRange(min(p0.x,min(p1.x,p2.x)),max(p0.x,max(p1.x,p2.x)));
    boxes[1][i] = fixRange(min(p0.y,min(p1.y,p2.y)),max(p0.y,max(p1.y,p2.y)));
    boxes[2][i] = fixRange(min(p0.z,min(p1.z,p2.z)),max(p0.z,max(p1.z,p2.z)));
  });
  
  // Loop over the dimensions creating an array of events for each
  // dimension, sorting each one, and extracting the bounding box
  // from the first and last elements in the sorted events in each dim.
  Events events;
  BoundingBox boundingBox;
  for (int d = 0; d < 3; d++) {
    events[d] = newA(event, 2*n); // freed while generating tree
    parallel_for((intT)0, n, [&] (intT i) {
      events[d][2*i] = event(boxes[d][i].min, i, START);
      events[d][2*i+1] = event(boxes[d][i].max, i, END);
    });
    compSort(events[d], n*2, cmpVal());
    boundingBox[d] = range(events[d][0].v, events[d][2*n-1].v);
  }
  
  // build the tree
  intT recursionDepth = min(maxRecursionDepth, utils::log2Up(n)-1);
  treeNode* R = generateNode(boxes, events, boundingBox, n*2,
                             recursionDepth);
  
  if (STATS)
    cout << "Triangles across all leaves = " << R->n 
    << " Leaves = " << R->leaves << endl;
  for (int d = 0; d < 3; d++) free(boxes[d]);
  
  // get the intersections
  parray<intT> results(numRays);
  processRays(Tri, rays, numRays, R, results.begin());
  treeNode::del(R);
  
  if (CHECK12) {
    int nr = 10;
    parray<intT> indx(n, [&] (intT i) {
      return i;
    });
    for (int i= 0; i < nr; i++) {
      cout << results[i] << endl;
      if (findRay(rays[i], indx.begin(), n, Tri, boundingBox) != results[i]) {
        cout << "bad intersect in checking ray intersection" << endl;
        abort();
      }
    }
  }
  
  if (STATS)
    cout << "tcount=" << tcount << " ccount=" << ccount << endl;
  return results;
}
  
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PCTL_KDTREE_H_ */

