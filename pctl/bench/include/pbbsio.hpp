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

#ifndef _BENCH_IO
#define _BENCH_IO

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include "dpsdatapar.hpp"
#include "pstring.hpp"

namespace pasl {
namespace pctl {
    
namespace benchIO {
  using namespace std;
  
  // A structure that keeps a sequence of strings all allocated from
  // the same block of memory
  class words {
  public:
    pstring Chars;  // array storing all strings
    parray<char*> Strings; // pointers to strings (all should be null terminated)
    words() { }
    words(words&& other) = default;
  };
  
  inline bool isSpace(char c) {
    switch (c)  {
      case '\r':
      case '\t':
      case '\n':
      case 0:
      case ' ' : return true;
      default : return false;
    }
  }
  
  struct toLong { long operator() (bool v) {return (long) v;} };
  
  // parallel code for converting a string to words
  words stringToWords(pstring& Str) {
    long n = Str.size()+1;
    words ws;
    
    ws.Chars.swap(Str);
    
    parallel_for((long)0, n, [&] (long i) {
      if (isSpace(ws.Chars[i])) ws.Chars.chars[i] = 0;
    });
    
    // mark start of words
    parray<bool> FL(n, [&] (long i) {
      if (i == 0) {
        return ws.Chars.chars[0] != 0;
      } else {
        return (ws.Chars.chars[i] != 0) && (ws.Chars.chars[i-1] == 0);
      }
    });
    
    // offset for each start of word
    parray<long> Off = pack_index(FL.cbegin(), FL.cend());
    long m = Off.size();
    long *offsets = Off.begin();
    
    // pointer to each start of word
    ws.Strings = parray<char*>(m, [&] (long j) {
      return ws.Chars.begin()+offsets[j];
    });

    return ws;
  }
  
  int writeStringToFile(char* S, long n, char* fileName) {
    ofstream file (fileName, ios::out | ios::binary);
    if (!file.is_open()) {
      std::cout << "Unable to open file: " << fileName << std::endl;
      return 1;
    }
    file.write(S, n);
    file.close();
    return 0;
  }
  
  inline int xToStringLen(long a) { return 21;}
  inline void xToString(char* s, long a) { sprintf(s,"%ld",a);}
  
  inline int xToStringLen(int a) { return 12;}
  inline void xToString(char* s, int a) { sprintf(s,"%d",a);}
  
  inline int xToStringLen(double a) { return 18;}
  inline void xToString(char* s, double a) { sprintf(s,"%.11le", a);}
  
  inline int xToStringLen(char* a) { return strlen(a)+1;}
  inline void xToString(char* s, char* a) { sprintf(s,"%s",a);}
  
  template <class A, class B>
  inline int xToStringLen(pair<A,B> a) {
    return xToStringLen(a.first) + xToStringLen(a.second) + 1;
  }
  template <class A, class B>
  inline void xToString(char* s, pair<A,B> a) {
    int l = xToStringLen(a.first);
    xToString(s,a.first);
    s[l] = ' ';
    xToString(s+l+1,a.second);
  }
  
  struct notZero { bool operator() (char A) {return A > 0;}};
  
  template <class T>
  pstring arrayToString(T* A, long n) {
    parray<long> L(n, [&] (long i) {
      return xToStringLen(A[i])+1;;
    });
    long m = dps::scan(L.begin(), L.end(), 0L, [&] (long x, long y) {
      return x + y;
    }, L.begin(), forward_exclusive_scan);
    pstring B(m, 0);
    parallel_for(0L, n-1, [&] (long i) {
      xToString(B + L[i],A[i]);
      B[L[i+1] - 1] = '\n';
    });
    xToString(B + L[n-1],A[n-1]);
    B[m-1] = '\n';
    pstring C(m);
    long mm = dps::filter(B.cbegin(), B.cend(), C.begin(), [&] (char A) {
      return A > 0;
    });
    C.resize(mm);
    return C;
  }
  
  template <class T>
  void writeArrayToStream(ofstream& os, T* A, long n) {
    long BSIZE = 1000000;
    long offset = 0;
    while (offset < n) {
      // Generates a string for a sequence of size at most BSIZE
      // and then wrties it to the output stream
      pstring S = arrayToString(A+offset,std::min(BSIZE,n-offset));
      os.write(S.begin(), S.chars.size());
      offset += BSIZE;
    }
  }
  
  template <class T>
  int writeArrayToFile(string header, T* A, long n, char* fileName) {
    ofstream file (fileName, ios::out | ios::binary);
    if (!file.is_open()) {
      std::cout << "Unable to open file: " << fileName << std::endl;
      return 1;
    }
    file << header << endl;
    writeArrayToStream(file, A, n);
    file.close();
    return 0;
  }
  
  pstring readStringFromFile(char *fileName) {
    ifstream file (fileName, ios::in | ios::binary | ios::ate);
    if (!file.is_open()) {
      std::cout << "Unable to open file: " << fileName << std::endl;
      abort();
    }
    long end = file.tellg();
    file.seekg (0, ios::beg);
    long n = end - file.tellg();
    pstring bytes(n);
    file.read (bytes.begin(),n);
    file.close();
    return bytes;
  }
  
  string intHeaderIO = "sequenceInt";
  
  template <class intT>
  intT writeIntArrayToFile(intT* A, long n, char* fileName) {
    return writeArrayToFile(intHeaderIO, A, n, fileName);
  }
  
  template <class intT>
  parray<intT> readIntArrayFromFile(char *fileName) {
    pstring S = readStringFromFile(fileName);
    words W = stringToWords(S);
    string header = (string) W.Strings[0];
    if (header != intHeaderIO) {
      cout << "readIntArrayFromFile: bad input" << endl;
      abort();
    }
    long n = W.Strings.size()-1;
    parray<intT> A(n, [&] (long i) {
      return atol(W.Strings[i+1]);
    });
    return A;
  }
  
}
}
}

#endif // _BENCH_IO
