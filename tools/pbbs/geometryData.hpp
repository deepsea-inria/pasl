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


#include "geometry.hpp"
#include "datagen.hpp"

#ifndef _GEOMETRY_DATA
#define _GEOMETRY_DATA

namespace pbbs {

template <class uintT>
point2d rand2d(uintT i) {
  uintT s1 = i;
  uintT s2 = i + dataGen::hash<uintT>(s1);
  return point2d(2*dataGen::hash<double>(s1)-1,
                 2*dataGen::hash<double>(s2)-1);
}

template <class intT, class uintT>
point3d rand3d(intT i) {
  uintT s1 = i;
  uintT s2 = i + dataGen::hash<uintT>(s1);
  uintT s3 = 2*i + dataGen::hash<uintT>(s2);
  return point3d(2*dataGen::hash<double>(s1)-1,
                 2*dataGen::hash<double>(s2)-1,
                 2*dataGen::hash<double>(s3)-1);
}

template <class intT>
point2d randInUnitSphere2d(intT i) {
  intT j = 0;
  vect2d v;
  do {
    intT o = dataGen::hash<intT>(j++);
    v = vect2d(rand2d(o+i));
  } while (v.Length() > 1.0);
  return point2d(v);
}

template <class intT, class uintT>
point3d randInUnitSphere3d(intT i) {
  intT j = 0;
  vect3d v;
  do {
    intT o = dataGen::hash<intT>(j++);
    v = vect3d(rand3d<intT, uintT>(o+i));
  } while (v.Length() > 1.0);
  return point3d(v);
}

template <class intT>
point2d randOnUnitSphere2d(intT i) {
  vect2d v = vect2d(randInUnitSphere2d(i));
  return point2d(v/v.Length());
}

template <class intT,class uintT>
point3d randOnUnitSphere3d(intT i) {
  vect3d v = vect3d(randInUnitSphere3d<intT, uintT>(i));
  return point3d(v/v.Length());
}
  
  
using namespace dataGen;
using namespace pasl;

template <class intT>
point2d randKuzmin(intT i) {
  vect2d v = vect2d(randOnUnitSphere2d(i));
  intT j = hash<intT>(i);
  double s = hash<double>(j);
  double r = sqrt(1.0/((1.0-s)*(1.0-s))-1.0);
  return point2d(v*r);
}

template <class intT,class uintT>
point3d randPlummer(intT i) {
  vect3d v = vect3d(randOnUnitSphere3d<intT,uintT>(i));
  intT j = hash<intT>(i);
  double s = pow(hash<double>(j),2.0/3.0);
  double r = sqrt(s/(1-s));
  return point3d(v*r);
}

template <class intT>
point2d* plummer2d(intT n) {
  point2d* Points = newA(point2d,n);
  sched::native::parallel_for(intT(0), n, [&] (intT i) {
    Points[i] = randKuzmin(i);
  });
  return Points;
}

template <class intT>
point2d* uniform2d(bool inSphere, bool onSphere, intT n) {
  point2d* Points = newA(point2d,n);
  sched::native::parallel_for(intT(0), n, [&] (intT i) {
    if (inSphere) Points[i] = randInUnitSphere2d(i);
    else if (onSphere) Points[i] = randOnUnitSphere2d(i);
    else Points[i] = rand2d(i);
  });
  return Points;
}
  
template <class intT, class uintT>
point3d* plummer3d(intT n) {
  point3d* Points = newA(point3d,n);
  sched::native::parallel_for(intT(0), n, [&] (intT i) {
    Points[i] = randPlummer<intT, uintT>(i);
  });
  return Points;
}
  
template <class intT, class uintT>
point3d* uniform3d(bool inSphere, bool onSphere, intT n) {
  point3d* Points = newA(point3d,n);
  sched::native::parallel_for(intT(0), n, [&] (intT i) {
    if (inSphere) Points[i] = randInUnitSphere3d(i);
    else if (onSphere) Points[i] = randOnUnitSphere3d(i);
    else Points[i] = rand3d<intT, uintT>(i);
  });
  return Points;
}
  
} // end namespace

#endif // _GEOMETRY_DATA
