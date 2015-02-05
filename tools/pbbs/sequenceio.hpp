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
#include <fstream>
#include <string>
#include <cstring>
#include "io.hpp"

#ifndef _SEQUENCE_IO
#define _SEQUENCE_IO

namespace pbbs {

namespace benchIO {
  using namespace std;
  template <class intT>
  using intPair = pair<intT, intT>;
  template <class intT>
  using stringIntPair = pair<char*,intT>;
  
  enum elementType { none, intType, intPairT, stringIntPairT, doubleT, stringT};
  elementType dataType(long a) { return intType;}
  elementType dataType(int a) { return intType;}
  elementType dataType(double a) { return doubleT;}
  elementType dataType(char* a) { return stringT;}
  elementType dataType(intPair<int> a) { return intPairT;}
  elementType dataType(intPair<long> a) { return intPairT;}
  elementType dataType(stringIntPair<int> a) { return stringIntPairT;}
  elementType dataType(stringIntPair<long> a) { return stringIntPairT;}
  
  string seqHeader(elementType dt) {
    switch (dt) {
      case intType: return "sequenceInt";
      case doubleT: return "sequenceDouble";
      case stringT: return "sequenceChar";
      case intPairT: return "sequenceIntPair";
      case stringIntPairT: return "sequenceStringIntPair";
      default:
        cout << "writeArrayToFile: type not supported" << endl;
        abort();
    }
  }
  
  elementType elementTypeFromString(string s) {
    if (s == "double") return doubleT;
    else if (s == "string") return stringT;
    else if (s == "int") return intType;
    else return none;
  }
  
  struct seqData {
    void* A; long n; elementType dt;
    char* O; // used for strings to store pointer to character array
    seqData(void* _A, long _n, elementType _dt) :
    A(_A), O(NULL), n(_n), dt(_dt) {}
    seqData(void* _A, char* _O, long _n, elementType _dt) :
    A(_A), O(_O), n(_n), dt(_dt) {}
    void del() {
      if (O) free(O);
      free(A);
    }
  };
  
  template <class intT>
  seqData readSequenceFromFile(char* fileName) {
    _seq<char> S = readStringFromFile(fileName);
    words W = stringToWords(S.A, S.n);
    char* header = W.Strings[0];
    void* bytes;
    long n = W.m-1;
    elementType tp;
    
    if (header == seqHeader(intType)) {
      tp = intType;
      intT* A = new intT[n];
      native::parallel_for(long(0), n, [&] (long i) {
        A[i] = atoi(W.Strings[i+1]);
      });
      //W.del(); // to deal with performance bug in malloc
      return seqData((void*) A, n, intType);
    } else if (header == seqHeader(doubleT)) {
      double* A = new double[n];
      native::parallel_for(long(0), n, [&] (long i) {
        A[i] = atof(W.Strings[i+1]);
      });
      //W.del(); // to deal with performance bug in malloc
      return seqData((void*) A, n, doubleT);
    } else if (header == seqHeader(stringT)) {
      char** A = new char*[n];
      native::parallel_for(long(0), n, [&] (long i) {
        A[i] = W.Strings[i+1];
      });
      //free(W.Strings); // to deal with performance bug in malloc
      return seqData((void*) A, W.Chars, n, stringT);
    } else if (header == seqHeader(intPairT)) {
      n = n/2;
      intPair<intT>* A = new intPair<intT>[n];
      native::parallel_for(long(0), n, [&] (long i) {
        A[i].first = atoi(W.Strings[2*i+1]);
        A[i].second = atoi(W.Strings[2*i+2]);
      });
      //W.del();  // to deal with perfromance bug in malloc
      return seqData((void*) A, n, intPairT);
    } else if (header == seqHeader(stringIntPairT)) {
      n = n/2;
      stringIntPair<intT>* A = new stringIntPair<intT>[n];
      native::parallel_for(long(0), n, [&] (long i) {
        A[i].first = W.Strings[2*i+1];
        A[i].second = atoi(W.Strings[2*i+2]);
      });
      // free(W.Strings); // to deal with performance bug in malloc
      return seqData((void*) A, W.Chars, n, stringIntPairT);
    }
    abort();
  }
  
  template <class T>
  int writeSequenceToFile(T* A, long n, char* fileName) {
    elementType tp = dataType(A[0]);
    return writeArrayToFile(seqHeader(tp), A, n, fileName);
  }
  
};
  
}

#endif // _SEQUENCE_IO
