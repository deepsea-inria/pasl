#include <atomic>

#ifndef _PCTL_PBBS_UTIL_H_
#define _PCTL_PBBS_UTIL_H_

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

#define newA(__E,__n) (__E*) malloc((__n)*sizeof(__E))
  
  // later: replace cas functions below by std::atomic counterparts
  
  // compare and swap on 8 byte quantities
  inline bool LCAS(long *ptr, long oldv, long newv) {
    unsigned char ret;
    /* Note that sete sets a 'byte' not the word */
    __asm__ __volatile__ (
                          "  lock\n"
                          "  cmpxchgq %2,%1\n"
                          "  sete %0\n"
                          : "=q" (ret), "=m" (*ptr)
                          : "r" (newv), "m" (*ptr), "a" (oldv)
                          : "memory");
    return ret;
  }
  
  // compare and swap on 4 byte quantity
  inline bool SCAS(int *ptr, int oldv, int newv) {
    unsigned char ret;
    /* Note that sete sets a 'byte' not the word */
    __asm__ __volatile__ (
                          "  lock\n"
                          "  cmpxchgl %2,%1\n"
                          "  sete %0\n"
                          : "=q" (ret), "=m" (*ptr)
                          : "r" (newv), "m" (*ptr), "a" (oldv)
                          : "memory");
    return ret;
  }
  
  // this should work with pointer types, or pairs of integers
  template <class ET>
  inline bool CAS(ET *ptr, ET oldv, ET newv) {
    if (sizeof(ET) == 8) {
      return utils::LCAS((long*) ptr, *((long*) &oldv), *((long*) &newv));
    } else if (sizeof(ET) == 4) {
      return utils::SCAS((int *) ptr, *((int *) &oldv), *((int *) &newv));
    } else {
      std::cout << "CAS bad length" << std::endl;
      abort();
    }
  }
  
  template <class ET>
  inline bool CAS_GCC(ET *ptr, ET oldv, ET newv) {
    return __sync_bool_compare_and_swap(ptr, oldv, newv);
  }
  
  template <class ET>
  inline ET fetchAndAdd(ET *a, ET b) {
    volatile ET newV, oldV;
    abort();
    do {oldV = *a; newV = oldV + b;}
    while (!CAS(a, oldV, newV));
    return oldV;
  }
  
  template <class ET>
  inline void writeAdd(ET *a, ET b) {
    volatile ET newV, oldV;
    do {oldV = *a; newV = oldV + b;}
    while (!CAS(a, oldV, newV));
  }
  
  template <class ET>
  inline bool writeMax(ET *a, ET b) {
    ET c; bool r=0;
    do c = *a;
    while (c < b && !(r=CAS(a,c,b)));
    return r;
  }
  
  template <class ET>
  inline bool writeMin(ET *a, ET b) {
    ET c; bool r=0;
    do c = *a;
    while (c > b && !(r=CAS(a,c,b)));
    // while (c > b && !(r=__sync_bool_compare_and_swap(a, c, b)));
    return r;
  }
  
  template <class ET>
  inline bool writeMin(ET **a, ET *b) {
    ET* c; bool r = 0;
    do c = *a;
    while (c > b && !(r=CAS(a,c,b)));
    return r;
  }
  
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PCTL_PBBS_UTIL_H_ */