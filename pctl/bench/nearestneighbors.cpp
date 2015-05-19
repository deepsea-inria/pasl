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
#include <algorithm>

#include "utils.hpp"
#include "geometry.hpp"
#include "geometryio.hpp"
#include "geometrydata.hpp"
#include "nearestneighbors.hpp"
#include "benchmark.hpp"

namespace pasl {
namespace pctl {
  
using namespace std;

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
  vertex() { }
  
};

// *************************************************************
//  TIMING
// *************************************************************

template <class intT, int maxK, class point, class Load_points, class Exp>
void doit(const Load_points& load_points, const Exp& exp) {
  parray<point> points = load_points();
  intT n = (intT)points.size();
  using vertex = vertex<point,maxK>;
  int dimensions;
  parray<vertex*> v(n);
  parray<vertex> vv(n);
  int k;
  k = pasl::util::cmdline::parse_or_default_int("k", 1);
  dimensions = points[0].dimension();
  
  parallel_for(intT(0), n, [&] (intT i) {
    v[i] = new (&vv[i]) vertex(points[i],i); });

  exp([&] {
    ANN<intT,maxK>(v.begin(), n, k);
  });
  
  std::string outfile = pasl::util::cmdline::parse_or_default_string("outfile", "");
  const char* outFile = (outfile=="") ? nullptr : outfile.c_str();
  int m = n * k;
  parray<int> Pout(m);
  parallel_for(intT(0), n, [&] (intT i) {
    for (int j=0; j < k; j++)
      Pout[maxK*i + j] = (v[i]->ngh[j])->identifier;
  });
    //if (outFile != nullptr) writeIntArrayToFile<intT>(Pout, m, outFile);
}
  
} // end namespace
} // end namespace

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (pasl::sched::experiment exp) {
    using intT = int;
    using uintT = unsigned int;
    pasl::util::cmdline::argmap_dispatch d;
    d.add("2", [&] {
      auto load_points = [&] {
        return pasl::pctl::load_points2d<intT>();
      };
      pasl::pctl::doit<intT, 1, point2d, decltype(load_points), decltype(exp)>(load_points, exp);
    });
    d.add("3", [&] {
      auto load_points = [&] {
        return pasl::pctl::load_points3d<intT,uintT>();
      };
      pasl::pctl::doit<intT, 1, point3d, decltype(load_points), decltype(exp)>(load_points, exp);
    });
    d.find_by_arg_or_default_key("dim", "2")();
  });
  return 0;
}

