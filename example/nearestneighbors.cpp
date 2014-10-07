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
#include "geometryData.hpp"
#include "nearestneighbors.hpp"
#include "benchmark.hpp"

namespace pbbs {
  
using namespace std;
using namespace benchIO;

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

// *************************************************************
//  TIMING
// *************************************************************
  
template <class intT>
point2d* load_points2d() {
  point2d* Points;
  intT n = (intT)pasl::util::cmdline::parse_or_default_int64("n", 100000);
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
  return Points;
}
  
template <class intT>
point3d* load_points3d() {
  point3d* Points;
  intT n = (intT)pasl::util::cmdline::parse_or_default_int64("n", 100000);
  using thunk_type = std::function<void ()>;
  pasl::util::cmdline::argmap<thunk_type> t;
  t.add("from_file", [&] {
    pasl::util::atomic::die("todo");
  });
  t.add("by_generator", [&] {
    pasl::util::cmdline::argmap<thunk_type> m;
    m.add("plummer", [&] {
      Points = pbbs::plummer3d(n);
    });
    m.add("uniform", [&] {
      bool inSphere = pasl::util::cmdline::parse_or_default_bool("in_sphere", false);
      bool onSphere = pasl::util::cmdline::parse_or_default_bool("on_sphere", false);
      Points = pbbs::uniform3d(inSphere, onSphere, n);
    });
    m.find_by_arg_or_default_key("generator", "plummer")();
  });
  t.find_by_arg_or_default_key("load", "by_generator")();
  return Points;
}
  
template <class intT, int maxK, class point, class Load_points>
void doit(int argc, char** argv, const Load_points& load_points) {
  intT result = 0;
  intT n = 0;
  point* pts;
  typedef vertex<point,maxK> vertex;
  int dimensions;
  vertex** v;
  vertex* vv;
  int* Pout;
  int k;
  auto init = [&] {
    k = pasl::util::cmdline::parse_or_default_int("k", 1);
    dimensions = pts[0].dimension();
    v = newA(vertex*,n);
    vv = newA(vertex, n);
    pts = load_points();
    native::parallel_for(intT(0), n, [&] (intT i) {
      v[i] = new (&vv[i]) vertex(pts[i],i); });
  };
  auto run = [&] (bool sequential) {
    ANN<intT,maxK>(v, n, k);
  };
  auto output = [&] {
    std::string outfile = pasl::util::cmdline::parse_or_default_string("outfile", "");
    const char* outFile = (outfile=="") ? nullptr : outfile.c_str();
    int m = n * k;
    Pout = newA(int, m);
    native::parallel_for(intT(0), n, [&] (intT i) {
      for (int j=0; j < k; j++)
        Pout[maxK*i + j] = (v[i]->ngh[j])->identifier;
    });
    if (outFile != NULL) writeIntArrayToFile<intT>(Pout, m, outFile);
  };
  auto destroy = [&] {
    free(pts);
    free(v);
    free(vv);
    
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}
  
} // end namespace

int main(int argc, char** argv) {
  using intT = int;
  auto load2d = [&] {
    return pbbs::load_points2d<intT>();
  };
  pbbs::doit<intT, 1, pbbs::point2d, typeof(load2d)>(argc, argv, load2d);
  return 0;
}

/*
int parallel_main(int argc, char* argv[]) {
  commandLine P(argc,argv,"[-k {1,...,10}] [-d {2,3}] [-o <outFile>] [-r <rounds>] <inFile>");
  char* iFile = P.getArgument(0);
  char* oFile = P.getOptionValue("-o");
  int rounds = P.getOptionIntValue("-r",1);
  int k = P.getOptionIntValue("-k",1);
  int d = P.getOptionIntValue("-d",2);
  if (k > 10 || k < 1) P.badArgument();
  if (d < 2 || d > 3) P.badArgument();
  
  if (d == 2) {
    _seq<point2d> PIn = readPointsFromFile<point2d>(iFile);
    if (k == 1) timeNeighbors<1>(PIn.A, PIn.n, 1, rounds, oFile);
    else timeNeighbors<10>(PIn.A, PIn.n, k, rounds, oFile);
  }
  
  if (d == 3) {
    _seq<point3d> PIn = readPointsFromFile<point3d>(iFile);
    if (k == 1) timeNeighbors<1>(PIn.A, PIn.n, 1, rounds, oFile);
    else timeNeighbors<10>(PIn.A, PIn.n, k, rounds, oFile);
  }
  
}
 */
