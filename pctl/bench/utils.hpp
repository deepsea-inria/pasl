#include <atomic>

#ifndef _PBBS_UTIL_H_
#define _PBBS_UTIL_H_

namespace pasl {
namespace pctl {
namespace utils {
  
  // returns the log base 2 rounded up (works on ints or longs or unsigned versions)
  template <class T>
  static int log2Up(T i) {
    int a=0;
    T b=i-1;
    while (b > 0) {b = b >> 1; a++;}
    return a;
  }
  
  static int logUp(unsigned int i) {
    int a=0;
    int b=i-1;
    while (b > 0) {b = b >> 1; a++;}
    return a;
  }
  
  static int logUpLong(unsigned long i) {
    int a=0;
    long b=i-1;
    while (b > 0) {b = b >> 1; a++;}
    return a;
  }

} // end namespace
} // end namespace
} // end namespace

#endif /*! _PBBS_UTIL_H_ */